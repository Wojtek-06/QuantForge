#pragma once

#include "quantforge/engine/types.hpp"

namespace quantforge::engine {

struct Trade {
    OrderId buy_order_id{};
    OrderId sell_order_id{};
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
    ParticipantId buyer_id{0};
    ParticipantId seller_id{0};
    /// Fee charged to the aggressor (taker), in price*qty units (cash).
    double taker_fee{0.0};
    /// Fee charged (or rebate if negative) to the resting maker.
    double maker_fee{0.0};
    bool buyer_is_taker{true};
};

} // namespace quantforge::engine
