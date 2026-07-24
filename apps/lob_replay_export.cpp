#include "quantforge/sim/replay.hpp"
#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/avellaneda_stoikov.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

void printUsage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0
        << " [--strategy symmetric_mm|avellaneda_stoikov]"
        << " [--horizon N] [--out path.json]\n"
        << "Exports LOB replay frames for the animated evidence viewer.\n";
}

} // namespace

int main(int argc, char** argv)
{
    using namespace quantforge;

    std::string strategy_name = "avellaneda_stoikov";
    std::string out_path = "web/replay/sample_replay.json";
    engine::Timestamp horizon = 1'500;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--strategy" && i + 1 < argc) {
            strategy_name = argv[++i];
            continue;
        }
        if (arg == "--horizon" && i + 1 < argc) {
            horizon = static_cast<engine::Timestamp>(std::stoull(argv[++i]));
            continue;
        }
        if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
            continue;
        }
        std::cerr << "Unknown argument: " << arg << '\n';
        printUsage(argv[0]);
        return 2;
    }

    sim::SimulatorConfig config;
    config.seed = 42;
    config.horizon = horizon;
    config.tick_interval = 10;
    config.exogenous_qty = 5;
    config.initial_mid = 10'000;
    config.fees = engine::FeeSchedule{-0.2, 1.0};
    config.latency.strategy_latency = 1;
    config.record_replay = true;
    config.replay_stride = 1;
    config.enable_risk_gate = true;
    config.risk_limits.max_abs_inventory = 80;
    config.risk_limits.enable_overnight_var = true;
    config.risk_limits.max_var_95 = 50'000.0;
    config.overnight_check_every = 25;

    sim::Simulator simulator(config);
    if (strategy_name == "symmetric_mm") {
        simulator.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    } else if (strategy_name == "avellaneda_stoikov") {
        simulator.setStrategy(std::make_unique<strategy::AvellanedaStoikovMM>());
    } else {
        std::cerr << "Unsupported strategy for replay: " << strategy_name << '\n';
        return 2;
    }

    const auto result = simulator.run();
    const auto json = sim::formatReplayJson(result.replay, result.strategy_name);

    std::ofstream out(out_path);
    if (!out) {
        std::cerr << "Failed to open " << out_path << '\n';
        return 1;
    }
    out << json;

    std::cout << "Wrote " << result.replay.size() << " frames to " << out_path
              << "\nstrategy=" << result.strategy_name
              << " fills=" << result.metrics.fills
              << " mtm=" << result.metrics.mtm_pnl
              << " risk_killed=" << (result.risk_killed ? "yes" : "no")
              << '\n';
    return 0;
}
