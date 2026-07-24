#pragma once

#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/strategy.hpp"

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
};

struct StrategyResult {
    std::string strategy_name;
    sim::SimulationResult simulation;
};

struct ExperimentReport {
    std::string experiment_name;
    std::vector<StrategyResult> results;
};

std::unique_ptr<strategy::IStrategy> makeStrategy(const std::string& name);

ExperimentReport runComparison(const ExperimentConfig& config);

std::string formatReport(const ExperimentReport& report);

} // namespace quantforge::experiment
