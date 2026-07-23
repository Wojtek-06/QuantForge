#pragma once

#include "quantforge/engine/types.hpp"

namespace quantforge::engine {

struct Order {
    OrderId id{};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{0};
    Quantity quantity{0};
    Quantity remaining_quantity{0};
    Timestamp timestamp{0};
    TimeInForce tif{TimeInForce::GTC};
    ParticipantId participant_id{0};

    Order() = default;

    Order(
        OrderId id_,
        Side side_,
        OrderType type_,
        Price price_,
        Quantity quantity_,
        Timestamp timestamp_,
        TimeInForce tif_ = TimeInForce::GTC,
        ParticipantId participant_id_ = 0
    )
        : id(id_),
          side(side_),
          type(type_),
          price(price_),
          quantity(quantity_),
          remaining_quantity(quantity_),
          timestamp(timestamp_),
          tif(tif_),
          participant_id(participant_id_)
    {
    }
};

} // namespace quantforge::engine
