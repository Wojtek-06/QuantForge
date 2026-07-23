#include "quantforge/sim/naive_bar_backtester.hpp"

#include "quantforge/engine/trade.hpp"

#include <algorithm>
#include <cmath>

namespace quantforge::sim {

NaiveBarBacktester::NaiveBarBacktester(NaiveBarConfig config)
    : config_(std::move(config))
{
}

void NaiveBarBacktester::setStrategy(std::unique_ptr<strategy::IStrategy> strategy)
{
    strategy_ = std::move(strategy);
}

std::vector<Bar> NaiveBarBacktester::buildBars(
    const marketdata::MarketEventSeries& events,
    engine::Timestamp bar_width
)
{
    std::vector<Bar> bars;
    if (events.empty() || bar_width == 0) {
        return bars;
    }

    engine::Timestamp horizon = events.back().timestamp;
    for (engine::Timestamp start = 0; start <= horizon; start += bar_width) {
        const engine::Timestamp end = start + bar_width;
        Bar bar;
        bar.start = start;
        bar.end = end;
        bool any = false;

        for (const auto& e : events) {
            if (e.kind != marketdata::EventKind::Mid) {
                continue;
            }
            if (e.timestamp < start || e.timestamp >= end) {
                continue;
            }
            if (!any) {
                bar.open = bar.high = bar.low = bar.close = e.price;
                any = true;
            } else {
                bar.high = std::max(bar.high, e.price);
                bar.low = std::min(bar.low, e.price);
                bar.close = e.price;
            }
        }

        if (any) {
            bars.push_back(bar);
        }
    }

    return bars;
}

NaiveBarResult NaiveBarBacktester::run(const marketdata::MarketEventSeries& events)
{
    NaiveBarResult result;
    result.strategy_name = strategy_ ? strategy_->name() : "none";
    result.used_look_ahead = config_.leak_same_bar_close;

    if (strategy_) {
        strategy_->reset();
    }

    metrics::Accounting accounting(config_.strategy_participant);
    const auto bars = buildBars(events, config_.bar_width);
    result.bars = bars.size();

    for (const auto& bar : bars) {
        if (!strategy_) {
            accounting.markToMarket(bar.close);
            continue;
        }

        // LEAKAGE: decision fair price = bar close (same information used for fills).
        // A non-leaking bar model would decide on open / prior close only.
        const engine::Price decision_mid =
            config_.leak_same_bar_close ? bar.close : bar.open;

        strategy::BookView view;
        view.now = bar.start;
        view.fair_price = decision_mid;
        view.book.best_bid = decision_mid - 1;
        view.book.best_ask = decision_mid + 1;
        view.book.bid_size = 100;
        view.book.ask_size = 100;

        const auto& snap = accounting.snapshot();
        strategy::PortfolioView portfolio{
            snap.inventory,
            snap.cash,
            snap.mtm_pnl
        };

        accounting.recordQuoteAttempt();
        const auto intent = strategy_->onTick(view, portfolio);
        if (!intent.quote) {
            accounting.markToMarket(bar.close);
            continue;
        }
        accounting.recordQuotePosted();

        // Fantasy touch rule: if bar trades through the quote, fill at close.
        if (bar.low <= intent.bid_price) {
            engine::Trade trade;
            trade.price = bar.close;  // fantasy fill price
            trade.quantity = intent.bid_size;
            trade.buyer_id = config_.strategy_participant;
            trade.seller_id = 99;
            trade.buyer_is_taker = false;
            trade.timestamp = bar.end;
            accounting.onTrade(trade, false);
            ++result.fantasy_fills;
        }

        if (bar.high >= intent.ask_price) {
            engine::Trade trade;
            trade.price = bar.close;
            trade.quantity = intent.ask_size;
            trade.buyer_id = 99;
            trade.seller_id = config_.strategy_participant;
            trade.buyer_is_taker = false;
            trade.timestamp = bar.end;
            accounting.onTrade(trade, false);
            ++result.fantasy_fills;
        }

        accounting.markToMarket(bar.close);
    }

    if (!bars.empty()) {
        accounting.markToMarket(bars.back().close);
    }

    result.final_snapshot = accounting.snapshot();
    result.metrics = accounting.metrics();
    return result;
}

} // namespace quantforge::sim
