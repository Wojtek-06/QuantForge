#include "quantforge/experiment/experiment.hpp"

#include <iostream>

int main()
{
    using namespace quantforge;

    experiment::ExperimentConfig config;
    config.name = "mm_strategy_comparison";
    config.sim.seed = 42;
    config.sim.horizon = 5'000;
    config.sim.tick_interval = 10;
    config.sim.exogenous_qty = 5;
    config.sim.initial_mid = 10'000;
    config.sim.fees = engine::FeeSchedule{-0.2, 1.0};  // maker rebate, taker fee
    config.sim.latency.strategy_latency = 1;
    config.strategies = {
        "no_trade",
        "symmetric_mm",
        "avellaneda_stoikov"
    };

    const auto report = experiment::runComparison(config);
    std::cout << experiment::formatReport(report);

    return 0;
}
