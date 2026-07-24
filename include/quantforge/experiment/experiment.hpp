#pragma once

#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/strategy.hpp"
#include "quantforge/strategy/strategy_params.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace quantforge::experiment {

struct ExperimentConfig {
    std::string name{"default"};
    sim::SimulatorConfig sim{};
    std::vector<std::string> strategies{
        "no_trade",
        "symmetric_mm",
        "avellaneda_stoikov"
    };
    /// Optional path recorded when loaded from JSON (for reports).
    std::string market_data_csv;
    bool run_walk_forward{false};
    std::size_t wf_is_horizon{1'000};
    std::size_t wf_oos_horizon{500};
    std::size_t wf_step{500};
    std::size_t wf_max_folds{4};
    std::string wf_strategy{"symmetric_mm"};
    /// When true, walk-forward searches params on each IS fold only, then freezes
    /// the winner for that fold's OOS evaluation.
    bool wf_param_search{false};
    std::string wf_search_method{"grid"};
    std::size_t wf_search_max_trials{16};
    std::uint64_t wf_search_seed{7};
    strategy::ParamSearchSpace wf_search_space{};
    /// Default params used when param search is disabled.
    strategy::StrategyParams default_params{};
};

struct StrategyResult {
    std::string strategy_name;
    sim::SimulationResult simulation;
    strategy::StrategyParams params{};
};

struct ExperimentReport {
    std::string experiment_name;
    std::vector<StrategyResult> results;
};

std::unique_ptr<strategy::IStrategy> makeStrategy(const std::string& name);

std::unique_ptr<strategy::IStrategy> makeStrategy(
    const std::string& name,
    const strategy::StrategyParams& params
);

ExperimentReport runComparison(const ExperimentConfig& config);

ExperimentReport runComparison(
    const ExperimentConfig& config,
    const strategy::StrategyParams& params
);

std::string formatReport(const ExperimentReport& report);

} // namespace quantforge::experiment
