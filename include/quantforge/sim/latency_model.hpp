#pragma once

#include "quantforge/engine/types.hpp"

#include <cstdint>

namespace quantforge::sim {

/// Fixed + per-participant latency in simulation time units.
struct LatencyModel {
    engine::Timestamp base_latency{0};
    engine::Timestamp strategy_latency{1};
    engine::Timestamp market_data_latency{0};
    /// Cancel path latency. 0 = immediate cancel (legacy default).
    /// Set > 0 to allow aggressive flow to race resting quotes before cancel lands.
    engine::Timestamp cancel_latency{0};

    engine::Timestamp submitDelay(engine::ParticipantId /*participant*/) const
    {
        return base_latency + strategy_latency;
    }

    engine::Timestamp cancelDelay(engine::ParticipantId /*participant*/) const
    {
        return base_latency + cancel_latency;
    }
};

} // namespace quantforge::sim
