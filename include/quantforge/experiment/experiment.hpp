#pragma once

#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/strategy.hpp"

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
