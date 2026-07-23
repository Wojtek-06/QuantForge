#pragma once

#include "quantforge/engine/order.hpp"
#include "quantforge/engine/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace quantforge::sim {

enum class EventType {
    MarketOrderSubmit,
    CancelOrder,
    StrategyTick,
    MarkToMarket
};

struct MarketOrderSubmit {
    engine::Order order;
};

struct CancelOrder {
    engine::OrderId order_id{};
};

struct StrategyTick {
    engine::Timestamp wall_time{};
};

struct MarkToMarket {
    engine::Price mid_hint{0};
};

using EventPayload = std::variant<
    MarketOrderSubmit,
    CancelOrder,
    StrategyTick,
    MarkToMarket
>;

struct Event {
    engine::Timestamp time{0};
    std::uint64_t sequence{0};  ///< tie-break for deterministic ordering
    EventType type{EventType::StrategyTick};
    EventPayload payload{StrategyTick{}};
};

/// Priority: earlier time first; then lower sequence.
struct EventCompare {
    bool operator()(const Event& a, const Event& b) const
    {
        if (a.time != b.time) {
            return a.time > b.time;  // min-heap via greater
        }
        return a.sequence > b.sequence;
    }
};

} // namespace quantforge::sim
