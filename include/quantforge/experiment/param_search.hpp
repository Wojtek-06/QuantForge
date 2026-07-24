#pragma once

#include "quantforge/experiment/experiment.hpp"
#include "quantforge/strategy/strategy_params.hpp"

#include <string>
#include <vector>

namespace quantforge::experiment {

/// Enumerate / sample candidates from the search space for a strategy family.
std::vector<strategy::StrategyParams> enumerateCandidates(
    const std::string& strategy,
    const strategy::ParamSearchConfig& search
);

/// Score candidates on the provided IS-only experiment config.
/// Never touches OOS horizons/seeds — caller must pass an IS fold config.
strategy::StrategyParams searchBestParamsIsOnly(
    const ExperimentConfig& is_config,
    const std::string& strategy,
    const strategy::ParamSearchConfig& search,
    std::vector<strategy::ParamTrial>* trials_out = nullptr
);

/// Convenience: run one simulation with explicit strategy params.
StrategyResult runWithParams(
    const ExperimentConfig& config,
    const std::string& strategy,
    const strategy::StrategyParams& params
);

} // namespace quantforge::experiment
