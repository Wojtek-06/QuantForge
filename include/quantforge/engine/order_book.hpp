#pragma once

#include "quantforge/engine/order.hpp"
#include "quantforge/engine/trade.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace quantforge::engine {

class OrderBook {
public:
    explicit OrderBook(FeeSchedule fees = FeeSchedule{});

    void setFeeSchedule(const FeeSchedule& fees);
    const FeeSchedule& feeSchedule() const;

    std::vector<Trade> addOrder(const Order& order);
    bool cancelOrder(OrderId order_id);

    /// Last trade price used to arm stop triggers (0 if none yet).
    std::optional<Price> lastTradePrice() const;

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;

    Quantity bidQuantityAt(Price price) const;
    Quantity askQuantityAt(Price price) const;

    /// 0-based queue depth ahead of `order_id` at its price level.
    /// Returns nullopt if the order is not resting.
    std::optional<std::size_t> queuePosition(OrderId order_id) const;

    bool empty() const;

private:
    using OrderList = std::list<Order>;

    FeeSchedule fees_;

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList> asks_;
    /// Stop books keyed by trigger price.
    std::map<Price, OrderList> stop_buys_;   // trigger ascending
    std::map<Price, OrderList, std::greater<Price>> stop_sells_;

    struct OrderLocation {
        Side side;
        Price price;
        OrderList::iterator iterator;
        bool is_stop{false};
    };

    std::unordered_map<OrderId, OrderLocation> order_lookup_;
    std::optional<Price> last_trade_price_{};

    void validateOrder(const Order& order) const;

    std::vector<Trade> matchBuyOrder(Order incoming);
    std::vector<Trade> matchSellOrder(Order incoming);
    std::vector<Trade> restStopOrder(Order order);
    std::vector<Trade> triggerStops(Price trade_price);

    void addRestingOrder(const Order& order);

    Quantity availableAskLiquidity(Price limit_price, bool is_market) const;
    Quantity availableBidLiquidity(Price limit_price, bool is_market) const;

    Trade makeTrade(
        OrderId buy_id,
        OrderId sell_id,
        Price price,
        Quantity quantity,
        Timestamp timestamp,
        ParticipantId buyer_id,
        ParticipantId seller_id,
        bool buyer_is_taker
    ) const;
};

} // namespace quantforge::engine
