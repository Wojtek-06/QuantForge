#include "quantforge/experiment/config_loader.hpp"
#include "quantforge/experiment/experiment.hpp"
#include "quantforge/marketdata/csv_loader.hpp"
#include "quantforge/report/comparison_report.hpp"
#include "quantforge/sim/naive_bar_backtester.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0 << " [--config path.json] [--out-dir dir]\n"
        << "  Runs strategy comparison; writes markdown+CSV reports when --out-dir set.\n";
}

} // namespace

int main(int argc, char** argv)
{
    using namespace quantforge;

    std::filesystem::path config_path;
    std::filesystem::path out_dir;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
            continue;
        }
        if (arg == "--out-dir" && i + 1 < argc) {
            out_dir = argv[++i];
            continue;
        }
        std::cerr << "Unknown argument: " << arg << '\n';
        printUsage(argv[0]);
        return 2;
    }

    experiment::ExperimentConfig config;
    if (!config_path.empty()) {
        config = experiment::loadConfigFile(config_path);
    } else {
        config.name = "mm_strategy_comparison";
        config.sim.seed = 42;
        config.sim.horizon = 5'000;
        config.sim.tick_interval = 10;
        config.sim.exogenous_qty = 5;
        config.sim.initial_mid = 10'000;
        config.sim.fees = engine::FeeSchedule{-0.2, 1.0};
        config.sim.latency.strategy_latency = 1;
        config.strategies = {
            "no_trade",
            "symmetric_mm",
            "avellaneda_stoikov"
        };
    }

    const auto report = experiment::runComparison(config);
    std::cout << experiment::formatReport(report);

    report::ResearchReport research;
    research.experiment = report;
    research.notes =
        "Gross spread ≠ profit. Compare LOB fills to naive bar fantasy fills "
        "under identical mid paths when market data is available.";

    // Leakage case study when we have (or can synthesize) a mid path.
    marketdata::MarketEventSeries series;
    if (config.sim.market_data.has_value()) {
        series = *config.sim.market_data;
    } else {
        const auto tmp = std::filesystem::temp_directory_path() /
            "quantforge_demo_flow.csv";
        marketdata::writeSyntheticCsv(
            tmp,
            config.sim.horizon,
            config.sim.initial_mid,
            config.sim.seed
        );
        series = marketdata::loadCsv(tmp);
    }

    sim::NaiveBarBacktester naive({50, config.sim.strategy_participant, true});
    naive.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto naive_result = naive.run(series);

    report::LeakageCaseStudy leak;
    leak.title = "symmetric_mm_naive_bar_vs_lob";
    for (const auto& row : report.results) {
        if (row.strategy_name == "symmetric_mm") {
            leak.lob = row;
            break;
        }
    }
    leak.naive = naive_result;

    std::cout << "\n--- Leakage foil (naive bar vs LOB, symmetric_mm) ---\n";
    std::cout << "LOB MTM:   " << leak.lob.simulation.metrics.mtm_pnl << '\n';
    std::cout << "Naive MTM: " << leak.naive.metrics.mtm_pnl
              << " (fantasy fills=" << leak.naive.fantasy_fills << ")\n";

    research.leakage_cases.push_back(std::move(leak));

    if (!out_dir.empty()) {
        std::filesystem::create_directories(out_dir);
        const auto md = out_dir / (config.name + "_report.md");
        const auto csv = out_dir / (config.name + "_results.csv");
        report::writeReportFiles(research, md, csv);
        std::cout << "\nWrote " << md.string() << '\n'
                  << "Wrote " << csv.string() << '\n';
    }

    return 0;
}
