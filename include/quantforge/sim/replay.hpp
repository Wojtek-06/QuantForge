#pragma once

#include "quantforge/engine/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace quantforge::sim {

/// One LOB / strategy snapshot for animated replay evidence packs.
struct ReplayFrame {
    engine::Timestamp time{0};
    engine::Price mid{0};
    std::optional<engine::Price> best_bid;
    std::optional<engine::Price> best_ask;
    engine::Quantity bid_qty{0};
    engine::Quantity ask_qty{0};
    std::int64_t inventory{0};
    double mtm_pnl{0.0};
    bool quoting{false};
    engine::Price quote_bid{0};
    engine::Price quote_ask{0};
    engine::Quantity quote_bid_size{0};
    engine::Quantity quote_ask_size{0};
};

std::string formatReplayJson(
    const std::vector<ReplayFrame>& frames,
    const std::string& strategy_name
);

} // namespace quantforge::sim
