#pragma once

#include "quantforge/engine/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace quantforge::strategy {

/// Tunable knobs shared across MM strategies (unused fields ignored per strategy).
struct StrategyParams {
    engine::Price half_spread{5};
    engine::Quantity quote_size{10};
    std::int64_t max_inventory{100};

    double gamma{0.1};
    double sigma{5.0};
    double T{1.0};
    double k{1.5};
    engine::Price min_half_spread{2};

    bool operator==(const StrategyParams& other) const
    {
        return half_spread == other.half_spread &&
               quote_size == other.quote_size &&
               max_inventory == other.max_inventory &&
               gamma == other.gamma &&
               sigma == other.sigma &&
               T == other.T &&
               k == other.k &&
               min_half_spread == other.min_half_spread;
    }

    bool operator!=(const StrategyParams& other) const
    {
        return !(*this == other);
    }
};

struct ParamSearchSpace {
    std::vector<engine::Price> half_spreads{2, 5, 10, 15};
    std::vector<engine::Quantity> quote_sizes{5, 10, 20};
    std::vector<double> gammas{0.05, 0.1, 0.2};
    std::vector<double> sigmas{3.0, 5.0, 8.0};
    std::vector<double> horizons_T{0.5, 1.0};
};

struct ParamSearchConfig {
    bool enabled{false};
    /// "grid" enumerates the cartesian product (capped by max_trials).
    /// "random" samples up to max_trials candidates from the space.
    std::string method{"grid"};
    std::size_t max_trials{16};
    std::uint64_t seed{7};
    ParamSearchSpace space{};
};

struct ParamTrial {
    StrategyParams params{};
    double is_score{0.0};  ///< IS MTM used for selection
};

} // namespace quantforge::strategy
