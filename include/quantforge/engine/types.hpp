#pragma once

#include <cstdint>

namespace quantforge::engine {

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint32_t;
using Timestamp = std::uint64_t;
using ParticipantId = std::uint32_t;

enum class Side {
    Buy,
    Sell
};

enum class OrderType {
    Limit,
    Market,
    /// Resting stop: `price` is the trigger. Becomes a market order when last trade crosses.
    /// Buy stop triggers when last >= trigger; sell stop when last <= trigger.
    Stop
};

/// Time-in-force semantics for limit orders.
enum class TimeInForce {
    GTC,  ///< Good-til-cancelled (rest remainder)
    IOC,  ///< Immediate-or-cancel (fill what you can, cancel rest)
    FOK   ///< Fill-or-kill (all-or-nothing; else no trade)
};

/// Fee model in basis points of notional (price * quantity).
/// Positive = fee paid; negative rebate = credit to maker.
struct FeeSchedule {
    double maker_fee_bps{0.0};
    double taker_fee_bps{1.0};  ///< default 1 bp taker
};

} // namespace quantforge::engine
