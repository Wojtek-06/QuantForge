#include "quantforge/sim/simulator.hpp"

#include <cmath>

namespace quantforge::sim {

Simulator::Simulator(SimulatorConfig config)
    : config_(std::move(config)),
      book_(config_.fees),
      accounting_(config_.strategy_participant),
      latency_(config_.latency),
      fair_price_(config_.initial_mid),
      rng_(config_.seed)
{
}

void Simulator::setStrategy(std::unique_ptr<strategy::IStrategy> strategy)
{
    strategy_ = std::move(strategy);
}

void Simulator::enqueue(Event event)
{
    event.sequence = next_sequence_++;
    queue_.push(event);
}

engine::OrderId Simulator::allocateOrderId()
{
    return next_order_id_++;
}

strategy::BookView Simulator::makeBookView() const
{
    strategy::BookView view;
    view.now = now_;
    view.fair_price = fair_price_;
    view.book.best_bid = book_.bestBid();
    view.book.best_ask = book_.bestAsk();

    if (view.book.best_bid) {
        view.book.bid_size = book_.bidQuantityAt(*view.book.best_bid);
    }
    if (view.book.best_ask) {
        view.book.ask_size = book_.askQuantityAt(*view.book.best_ask);
    }

    return view;
}

void Simulator::onTrades(
    const std::vector<engine::Trade>& trades,
    engine::ParticipantId aggressor
)
{
    for (const auto& trade : trades) {
        const bool we_involved =
            trade.buyer_id == config_.strategy_participant ||
            trade.seller_id == config_.strategy_participant;

        if (!we_involved) {
            continue;
        }

        const bool we_are_taker =
            (aggressor == config_.strategy_participant) &&
            ((trade.buyer_is_taker &&
              trade.buyer_id == config_.strategy_participant) ||
             (!trade.buyer_is_taker &&
              trade.seller_id == config_.strategy_participant));

        accounting_.onTrade(trade, we_are_taker);
    }
}

void Simulator::scheduleExogenousFlow()
{
    std::uniform_int_distribution<engine::Timestamp> gap(1, 5);
    std::bernoulli_distribution buy_side(0.5);
    std::bernoulli_distribution jump(config_.jump_prob);
    std::uniform_int_distribution<int> jump_size(-20, 20);
    // Aggressive flow: market orders take resting MM quotes;
    // occasional passive IOC provides two-sided pressure near fair.
    std::bernoulli_distribution aggressive(0.65);

    engine::Price local_fair = config_.initial_mid;
    engine::Timestamp t = 1;

    while (t < config_.horizon) {
        if (jump(rng_)) {
            local_fair += jump_size(rng_);
            if (local_fair < 100) {
                local_fair = 100;
            }
        }

        // Publish fair-price update for strategy marking (deterministic).
        Event mtm;
        mtm.time = t;
        mtm.type = EventType::MarkToMarket;
        mtm.payload = MarkToMarket{local_fair};
        enqueue(mtm);

        const auto side =
            buy_side(rng_) ? engine::Side::Buy : engine::Side::Sell;

        engine::Order order;
        if (aggressive(rng_)) {
            order = engine::Order{
                allocateOrderId(),
                side,
                engine::OrderType::Market,
                0,
                config_.exogenous_qty,
                t,
                engine::TimeInForce::IOC,
                config_.noise_participant
            };
        } else {
            engine::Price limit_px = side == engine::Side::Buy
                ? local_fair + config_.tick_size * 8
                : local_fair - config_.tick_size * 8;
            if (limit_px <= 0) {
                limit_px = 1;
            }

            order = engine::Order{
                allocateOrderId(),
                side,
                engine::OrderType::Limit,
                limit_px,
                config_.exogenous_qty,
                t,
                engine::TimeInForce::IOC,
                config_.noise_participant
            };
        }

        Event event;
        event.time = t + latency_.market_data_latency;
        event.type = EventType::MarketOrderSubmit;
        event.payload = MarketOrderSubmit{order};
        enqueue(event);

        t += gap(rng_);
    }
}

void Simulator::scheduleStrategyTicks()
{
    for (engine::Timestamp t = 0; t < config_.horizon; t += config_.tick_interval) {
        Event event;
        event.time = t + latency_.submitDelay(config_.strategy_participant);
        event.type = EventType::StrategyTick;
        event.payload = StrategyTick{t};
        enqueue(event);
    }

    Event mtm;
    mtm.time = config_.horizon;
    mtm.type = EventType::MarkToMarket;
    mtm.payload = MarkToMarket{fair_price_};
    enqueue(mtm);
}

void Simulator::handleMarketOrder(const engine::Order& order)
{
    auto mutable_order = order;
    if (mutable_order.id == 0) {
        mutable_order.id = allocateOrderId();
    }
    // Ensure remaining qty is consistent if callers only set quantity.
    if (mutable_order.remaining_quantity == 0 && mutable_order.quantity > 0) {
        mutable_order.remaining_quantity = mutable_order.quantity;
    }
    mutable_order.timestamp = now_;

    const auto trades = book_.addOrder(mutable_order);
    onTrades(trades, mutable_order.participant_id);
}

void Simulator::handleCancel(engine::OrderId order_id)
{
    book_.cancelOrder(order_id);
}

void Simulator::cancelStrategyQuotes()
{
    for (const auto id : strategy_resting_bids_) {
        book_.cancelOrder(id);
    }
    for (const auto id : strategy_resting_asks_) {
        book_.cancelOrder(id);
    }
    strategy_resting_bids_.clear();
    strategy_resting_asks_.clear();
}

void Simulator::submitStrategyQuotes(const strategy::QuoteIntent& intent)
{
    accounting_.recordQuoteAttempt();

    if (!intent.quote) {
        return;
    }

    accounting_.recordQuotePosted();

    engine::Order bid{
        allocateOrderId(),
        engine::Side::Buy,
        engine::OrderType::Limit,
        intent.bid_price,
        intent.bid_size,
        now_,
        engine::TimeInForce::GTC,
        config_.strategy_participant
    };

    engine::Order ask{
        allocateOrderId(),
        engine::Side::Sell,
        engine::OrderType::Limit,
        intent.ask_price,
        intent.ask_size,
        now_,
        engine::TimeInForce::GTC,
        config_.strategy_participant
    };

    const auto bid_trades = book_.addOrder(bid);
    onTrades(bid_trades, config_.strategy_participant);

    const auto ask_trades = book_.addOrder(ask);
    onTrades(ask_trades, config_.strategy_participant);

    // Resting ids only if still in book.
    if (book_.queuePosition(bid.id)) {
        strategy_resting_bids_.push_back(bid.id);
    }
    if (book_.queuePosition(ask.id)) {
        strategy_resting_asks_.push_back(ask.id);
    }
}

void Simulator::handleStrategyTick()
{
    if (!strategy_) {
        return;
    }

    cancelStrategyQuotes();

    const auto book_view = makeBookView();
    const auto& snap = accounting_.snapshot();

    strategy::PortfolioView portfolio{
        snap.inventory,
        snap.cash,
        snap.mtm_pnl
    };

    const auto intent = strategy_->onTick(book_view, portfolio);
    submitStrategyQuotes(intent);
    accounting_.markToMarket(fair_price_);
}

void Simulator::processEvent(const Event& event)
{
    now_ = event.time;

    switch (event.type) {
    case EventType::MarketOrderSubmit:
        handleMarketOrder(std::get<MarketOrderSubmit>(event.payload).order);
        break;
    case EventType::CancelOrder:
        handleCancel(std::get<CancelOrder>(event.payload).order_id);
        break;
    case EventType::StrategyTick:
        handleStrategyTick();
        break;
    case EventType::MarkToMarket: {
        const auto mid_hint = std::get<MarkToMarket>(event.payload).mid_hint;
        if (mid_hint > 0) {
            fair_price_ = mid_hint;
        }
        accounting_.markToMarket(fair_price_);
        break;
    }
    }
}

SimulationResult Simulator::run()
{
    if (strategy_) {
        strategy_->reset();
    }

    next_sequence_ = 0;
    next_order_id_ = 1;
    now_ = 0;
    fair_price_ = config_.initial_mid;
    accounting_ = metrics::Accounting(config_.strategy_participant);
    book_ = engine::OrderBook(config_.fees);
    strategy_resting_bids_.clear();
    strategy_resting_asks_.clear();
    queue_ = {};
    rng_.seed(config_.seed);

    // Seed a thin book so early quotes have a mid reference.
    book_.addOrder(engine::Order{
        allocateOrderId(),
        engine::Side::Buy,
        engine::OrderType::Limit,
        config_.initial_mid - 10,
        50,
        0,
        engine::TimeInForce::GTC,
        config_.noise_participant
    });
    book_.addOrder(engine::Order{
        allocateOrderId(),
        engine::Side::Sell,
        engine::OrderType::Limit,
        config_.initial_mid + 10,
        50,
        0,
        engine::TimeInForce::GTC,
        config_.noise_participant
    });

    scheduleExogenousFlow();
    scheduleStrategyTicks();

    SimulationResult result;
    result.strategy_name = strategy_ ? strategy_->name() : "none";

    while (!queue_.empty()) {
        const Event event = queue_.top();
        queue_.pop();

        if (event.time > config_.horizon) {
            break;
        }

        processEvent(event);
        ++result.events_processed;
    }

    accounting_.markToMarket(fair_price_);
    result.final_snapshot = accounting_.snapshot();
    result.metrics = accounting_.metrics();
    result.trades = result.metrics.fills;

    return result;
}

} // namespace quantforge::sim
