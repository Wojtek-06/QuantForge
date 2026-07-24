#include "quantforge/engine/order_book.hpp"

#include <algorithm>

#include "quantforge/engine/exceptions.hpp"

namespace quantforge::engine {

namespace {

double notional(Price price, Quantity quantity)
{
    return static_cast<double>(price) * static_cast<double>(quantity);
}

double feeFromBps(double notional_value, double bps)
{
    return notional_value * (bps / 10'000.0);
}

} // namespace

OrderBook::OrderBook(FeeSchedule fees)
    : fees_(fees)
{
}

void OrderBook::setFeeSchedule(const FeeSchedule& fees)
{
    fees_ = fees;
}

const FeeSchedule& OrderBook::feeSchedule() const
{
    return fees_;
}

std::vector<Trade> OrderBook::addOrder(const Order& order)
{
    validateOrder(order);

    if (order.type == OrderType::Stop) {
        return restStopOrder(order);
    }

    std::vector<Trade> trades;
    if (order.side == Side::Buy) {
        trades = matchBuyOrder(order);
    } else {
        trades = matchSellOrder(order);
    }

    if (!trades.empty()) {
        auto cascaded = triggerStops(trades.back().price);
        trades.insert(trades.end(), cascaded.begin(), cascaded.end());
    }

    return trades;
}

bool OrderBook::cancelOrder(OrderId order_id)
{
    auto it = order_lookup_.find(order_id);

    if (it == order_lookup_.end()) {
        return false;
    }

    const auto& location = it->second;

    if (location.is_stop) {
        if (location.side == Side::Buy) {
            auto book_it = stop_buys_.find(location.price);
            if (book_it != stop_buys_.end()) {
                book_it->second.erase(location.iterator);
                if (book_it->second.empty()) {
                    stop_buys_.erase(book_it);
                }
            }
        } else {
            auto book_it = stop_sells_.find(location.price);
            if (book_it != stop_sells_.end()) {
                book_it->second.erase(location.iterator);
                if (book_it->second.empty()) {
                    stop_sells_.erase(book_it);
                }
            }
        }
    } else if (location.side == Side::Buy) {
        auto book_it = bids_.find(location.price);
        if (book_it != bids_.end()) {
            book_it->second.erase(location.iterator);

            if (book_it->second.empty()) {
                bids_.erase(book_it);
            }
        }
    } else {
        auto book_it = asks_.find(location.price);
        if (book_it != asks_.end()) {
            book_it->second.erase(location.iterator);

            if (book_it->second.empty()) {
                asks_.erase(book_it);
            }
        }
    }

    order_lookup_.erase(it);
    return true;
}

std::optional<Price> OrderBook::lastTradePrice() const
{
    return last_trade_price_;
}

std::optional<Price> OrderBook::bestBid() const
{
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const
{
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

bool OrderBook::empty() const
{
    return bids_.empty() && asks_.empty();
}

void OrderBook::validateOrder(const Order& order) const
{
    if (order.id == 0) {
        throw InvalidOrder("Order ID must be greater than zero.");
    }

    if (order.quantity == 0) {
        throw InvalidOrder("Order quantity must be greater than zero.");
    }

    if (order.type == OrderType::Limit && order.price <= 0) {
        throw InvalidOrder("Limit order price must be greater than zero.");
    }

    if (order.type == OrderType::Stop && order.price <= 0) {
        throw InvalidOrder("Stop trigger price must be greater than zero.");
    }

    if (order.type == OrderType::Market && order.tif == TimeInForce::FOK) {
        // Market FOK is allowed; liquidity checked in match path.
    }

    if (order_lookup_.contains(order.id)) {
        throw DuplicateOrderId("A resting order with this ID already exists.");
    }
}

Quantity OrderBook::bidQuantityAt(Price price) const
{
    const auto level_it = bids_.find(price);

    if (level_it == bids_.end()) {
        return 0;
    }

    Quantity total_quantity = 0;

    for (const auto& order : level_it->second) {
        total_quantity += order.remaining_quantity;
    }

    return total_quantity;
}

Quantity OrderBook::askQuantityAt(Price price) const
{
    const auto level_it = asks_.find(price);

    if (level_it == asks_.end()) {
        return 0;
    }

    Quantity total_quantity = 0;

    for (const auto& order : level_it->second) {
        total_quantity += order.remaining_quantity;
    }

    return total_quantity;
}

std::optional<std::size_t> OrderBook::queuePosition(OrderId order_id) const
{
    const auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return std::nullopt;
    }

    const auto& location = it->second;
    const OrderList* level = nullptr;

    if (location.is_stop) {
        if (location.side == Side::Buy) {
            const auto book_it = stop_buys_.find(location.price);
            if (book_it == stop_buys_.end()) {
                return std::nullopt;
            }
            level = &book_it->second;
        } else {
            const auto book_it = stop_sells_.find(location.price);
            if (book_it == stop_sells_.end()) {
                return std::nullopt;
            }
            level = &book_it->second;
        }
    } else if (location.side == Side::Buy) {
        const auto book_it = bids_.find(location.price);
        if (book_it == bids_.end()) {
            return std::nullopt;
        }
        level = &book_it->second;
    } else {
        const auto book_it = asks_.find(location.price);
        if (book_it == asks_.end()) {
            return std::nullopt;
        }
        level = &book_it->second;
    }

    std::size_t position = 0;
    for (auto order_it = level->begin(); order_it != level->end(); ++order_it) {
        if (order_it == location.iterator) {
            return position;
        }
        ++position;
    }

    return std::nullopt;
}

Quantity OrderBook::availableAskLiquidity(Price limit_price, bool is_market) const
{
    Quantity available = 0;

    for (const auto& [price, orders] : asks_) {
        if (!is_market && price > limit_price) {
            break;
        }

        for (const auto& order : orders) {
            available += order.remaining_quantity;
        }
    }

    return available;
}

Quantity OrderBook::availableBidLiquidity(Price limit_price, bool is_market) const
{
    Quantity available = 0;

    for (const auto& [price, orders] : bids_) {
        if (!is_market && price < limit_price) {
            break;
        }

        for (const auto& order : orders) {
            available += order.remaining_quantity;
        }
    }

    return available;
}

Trade OrderBook::makeTrade(
    OrderId buy_id,
    OrderId sell_id,
    Price price,
    Quantity quantity,
    Timestamp timestamp,
    ParticipantId buyer_id,
    ParticipantId seller_id,
    bool buyer_is_taker
) const
{
    const double n = notional(price, quantity);

    Trade trade{
        buy_id,
        sell_id,
        price,
        quantity,
        timestamp,
        buyer_id,
        seller_id,
        0.0,
        0.0,
        buyer_is_taker
    };

    if (buyer_is_taker) {
        trade.taker_fee = feeFromBps(n, fees_.taker_fee_bps);
        trade.maker_fee = feeFromBps(n, fees_.maker_fee_bps);
    } else {
        trade.taker_fee = feeFromBps(n, fees_.taker_fee_bps);
        trade.maker_fee = feeFromBps(n, fees_.maker_fee_bps);
    }

    return trade;
}

std::vector<Trade> OrderBook::matchBuyOrder(Order incoming)
{
    const bool is_market = incoming.type == OrderType::Market;

    if (incoming.tif == TimeInForce::FOK) {
        const Quantity available =
            availableAskLiquidity(incoming.price, is_market);
        if (available < incoming.remaining_quantity) {
            return {};
        }
    }

    std::vector<Trade> trades;

    while (incoming.remaining_quantity > 0 && !asks_.empty()) {
        auto best_ask_it = asks_.begin();
        Price best_ask_price = best_ask_it->first;

        if (!is_market && incoming.price < best_ask_price) {
            break;
        }

        auto& orders_at_level = best_ask_it->second;

        while (incoming.remaining_quantity > 0 && !orders_at_level.empty()) {
            auto resting_it = orders_at_level.begin();

            Quantity trade_quantity = std::min(
                incoming.remaining_quantity,
                resting_it->remaining_quantity
            );

            trades.push_back(makeTrade(
                incoming.id,
                resting_it->id,
                best_ask_price,
                trade_quantity,
                incoming.timestamp,
                incoming.participant_id,
                resting_it->participant_id,
                true
            ));

            incoming.remaining_quantity -= trade_quantity;
            resting_it->remaining_quantity -= trade_quantity;

            if (resting_it->remaining_quantity == 0) {
                order_lookup_.erase(resting_it->id);
                orders_at_level.erase(resting_it);
            }
        }

        if (orders_at_level.empty()) {
            asks_.erase(best_ask_it);
        }
    }

    const bool may_rest =
        !is_market &&
        incoming.tif == TimeInForce::GTC &&
        incoming.remaining_quantity > 0;

    if (may_rest) {
        addRestingOrder(incoming);
    }

    return trades;
}

std::vector<Trade> OrderBook::matchSellOrder(Order incoming)
{
    const bool is_market = incoming.type == OrderType::Market;

    if (incoming.tif == TimeInForce::FOK) {
        const Quantity available =
            availableBidLiquidity(incoming.price, is_market);
        if (available < incoming.remaining_quantity) {
            return {};
        }
    }

    std::vector<Trade> trades;

    while (incoming.remaining_quantity > 0 && !bids_.empty()) {
        auto best_bid_it = bids_.begin();
        Price best_bid_price = best_bid_it->first;

        if (!is_market && incoming.price > best_bid_price) {
            break;
        }

        auto& orders_at_level = best_bid_it->second;

        while (incoming.remaining_quantity > 0 && !orders_at_level.empty()) {
            auto resting_it = orders_at_level.begin();

            Quantity trade_quantity = std::min(
                incoming.remaining_quantity,
                resting_it->remaining_quantity
            );

            trades.push_back(makeTrade(
                resting_it->id,
                incoming.id,
                best_bid_price,
                trade_quantity,
                incoming.timestamp,
                resting_it->participant_id,
                incoming.participant_id,
                false
            ));

            incoming.remaining_quantity -= trade_quantity;
            resting_it->remaining_quantity -= trade_quantity;

            if (resting_it->remaining_quantity == 0) {
                order_lookup_.erase(resting_it->id);
                orders_at_level.erase(resting_it);
            }
        }

        if (orders_at_level.empty()) {
            bids_.erase(best_bid_it);
        }
    }

    const bool may_rest =
        !is_market &&
        incoming.tif == TimeInForce::GTC &&
        incoming.remaining_quantity > 0;

    if (may_rest) {
        addRestingOrder(incoming);
    }

    return trades;
}

std::vector<Trade> OrderBook::restStopOrder(Order order)
{
    // Immediate arm if last trade already through the trigger.
    if (last_trade_price_) {
        const bool armed =
            (order.side == Side::Buy && *last_trade_price_ >= order.price) ||
            (order.side == Side::Sell && *last_trade_price_ <= order.price);
        if (armed) {
            Order market = order;
            market.type = OrderType::Market;
            market.price = 0;
            market.tif = TimeInForce::IOC;
            std::vector<Trade> trades;
            if (market.side == Side::Buy) {
                trades = matchBuyOrder(market);
            } else {
                trades = matchSellOrder(market);
            }
            if (!trades.empty()) {
                auto cascaded = triggerStops(trades.back().price);
                trades.insert(trades.end(), cascaded.begin(), cascaded.end());
            }
            return trades;
        }
    }

    if (order.side == Side::Buy) {
        auto& level = stop_buys_[order.price];
        level.push_back(order);
        auto it = std::prev(level.end());
        order_lookup_[order.id] = OrderLocation{
            order.side,
            order.price,
            it,
            true
        };
    } else {
        auto& level = stop_sells_[order.price];
        level.push_back(order);
        auto it = std::prev(level.end());
        order_lookup_[order.id] = OrderLocation{
            order.side,
            order.price,
            it,
            true
        };
    }

    return {};
}

std::vector<Trade> OrderBook::triggerStops(Price trade_price)
{
    last_trade_price_ = trade_price;
    std::vector<Trade> all;

    bool progressed = true;
    while (progressed) {
        progressed = false;
        Price last = *last_trade_price_;

        // Buy stops: ascending map — fire while trigger <= last.
        while (!stop_buys_.empty() && stop_buys_.begin()->first <= last) {
            auto level_it = stop_buys_.begin();
            auto& level = level_it->second;
            Order stop = level.front();
            level.pop_front();
            order_lookup_.erase(stop.id);
            if (level.empty()) {
                stop_buys_.erase(level_it);
            }

            stop.type = OrderType::Market;
            stop.price = 0;
            stop.tif = TimeInForce::IOC;
            stop.remaining_quantity = stop.quantity;

            auto trades = matchBuyOrder(stop);
            if (!trades.empty()) {
                last = trades.back().price;
                last_trade_price_ = last;
                all.insert(all.end(), trades.begin(), trades.end());
            }
            progressed = true;
        }

        // Sell stops: descending map — fire while trigger >= last.
        while (!stop_sells_.empty() && stop_sells_.begin()->first >= last) {
            auto level_it = stop_sells_.begin();
            auto& level = level_it->second;
            Order stop = level.front();
            level.pop_front();
            order_lookup_.erase(stop.id);
            if (level.empty()) {
                stop_sells_.erase(level_it);
            }

            stop.type = OrderType::Market;
            stop.price = 0;
            stop.tif = TimeInForce::IOC;
            stop.remaining_quantity = stop.quantity;

            auto trades = matchSellOrder(stop);
            if (!trades.empty()) {
                last = trades.back().price;
                last_trade_price_ = last;
                all.insert(all.end(), trades.begin(), trades.end());
            }
            progressed = true;
        }
    }

    return all;
}

void OrderBook::addRestingOrder(const Order& order)
{
    if (order.side == Side::Buy) {
        auto& orders_at_level = bids_[order.price];
        orders_at_level.push_back(order);

        auto it = std::prev(orders_at_level.end());

        order_lookup_[order.id] = OrderLocation{
            order.side,
            order.price,
            it,
            false
        };
    } else {
        auto& orders_at_level = asks_[order.price];
        orders_at_level.push_back(order);

        auto it = std::prev(orders_at_level.end());

        order_lookup_[order.id] = OrderLocation{
            order.side,
            order.price,
            it,
            false
        };
    }
}

} // namespace quantforge::engine
