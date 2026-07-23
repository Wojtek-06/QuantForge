#pragma once

#include "quantforge/engine/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace quantforge::marketdata {

enum class EventKind {
    Mid,     ///< Fair / mid update (price field used; side/qty ignored)
    Market,  ///< Aggressive market order
    Limit    ///< Passive/aggressive limit order
};

struct MarketEvent {
    engine::Timestamp timestamp{0};
    EventKind kind{EventKind::Mid};
    std::optional<engine::Side> side{};
    engine::Price price{0};
    engine::Quantity quantity{0};
};

using MarketEventSeries = std::vector<MarketEvent>;

struct ValidationResult {
    bool ok{true};
    std::string message;
};

} // namespace quantforge::marketdata
