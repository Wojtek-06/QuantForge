#pragma once

#include "quantforge/experiment/experiment.hpp"
#include "quantforge/risk/var_es.hpp"
#include "quantforge/strategy/strategy_params.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace quantforge::experiment {

struct WalkForwardConfig {
    std::size_t is_horizon{1'000};   ///< in-sample window length
    std::size_t oos_horizon{500};    ///< out-of-sample window length
    std::size_t step{500};           ///< roll forward by this many units
    std::size_t max_folds{5};
    std::string strategy{"symmetric_mm"};
    strategy::ParamSearchConfig param_search{};
    /// Used when param_search.enabled == false.
    strategy::StrategyParams fixed_params{};
};

struct WalkForwardFold {
    std::size_t fold_index{0};
    std::uint64_t is_seed{0};
    std::uint64_t oos_seed{0};
    StrategyResult is_result{};
    StrategyResult oos_result{};
    risk::VarEsResult oos_var_es{};
    strategy::StrategyParams selected_params{};
    double is_selection_score{0.0};
    std::size_t is_trials{0};
};

struct WalkForwardReport {
    std::string experiment_name;
    std::string strategy;
    std::vector<WalkForwardFold> folds;
    /// Aggregate OOS MTM across folds (sum).
    double oos_mtm_sum{0.0};
    double oos_mtm_mean{0.0};
    double is_mtm_mean{0.0};
    bool param_search_enabled{false};
    std::string search_method;
};

WalkForwardReport runWalkForward(
    const ExperimentConfig& base,
    const WalkForwardConfig& wf
);

std::string formatWalkForwardReport(const WalkForwardReport& report);

} // namespace quantforge::experiment
