#include "quantforge/experiment/config_loader.hpp"
#include "quantforge/experiment/experiment.hpp"
#include "quantforge/experiment/walk_forward.hpp"
#include "quantforge/marketdata/csv_loader.hpp"
#include "quantforge/report/comparison_report.hpp"
#include "quantforge/report/json_export.hpp"
#include "quantforge/sim/naive_bar_backtester.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0
        << " [--config path.json] [--out-dir dir] [--json-out path.json]"
        << " [--walk-forward]\n"
        << "  Runs strategy comparison (+ optional walk-forward OOS).\n"
        << "  Writes markdown/CSV/JSON research artefacts when paths are set.\n";
}

} // namespace

int main(int argc, char** argv)
{
    using namespace quantforge;

    std::filesystem::path config_path;
    std::filesystem::path out_dir;
    std::filesystem::path json_out;
    bool force_walk_forward = false;

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
        if (arg == "--json-out" && i + 1 < argc) {
            json_out = argv[++i];
            continue;
        }
        if (arg == "--walk-forward") {
            force_walk_forward = true;
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

    if (force_walk_forward) {
        config.run_walk_forward = true;
    }

    const auto report = experiment::runComparison(config);
    std::cout << experiment::formatReport(report);

    report::ResearchReport research;
    research.experiment = report;
    research.notes =
        "Gross spread ≠ profit. Compare LOB fills to naive bar fantasy fills "
        "under identical mid paths when market data is available.";

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

    for (const auto& row : report.results) {
        if (row.simulation.overnight_var_es.valid) {
            std::cout << "VaR95[" << row.strategy_name << "]="
                      << row.simulation.overnight_var_es.var
                      << " ES95="
                      << row.simulation.overnight_var_es.expected_shortfall
                      << '\n';
        }
    }

    research.leakage_cases.push_back(std::move(leak));

    experiment::WalkForwardReport wf_report;
    if (config.run_walk_forward) {
        experiment::WalkForwardConfig wf;
        wf.is_horizon = config.wf_is_horizon;
        wf.oos_horizon = config.wf_oos_horizon;
        wf.step = config.wf_step;
        wf.max_folds = config.wf_max_folds;
        wf.strategy = config.wf_strategy;
        wf.fixed_params = config.default_params;
        wf.param_search.enabled = config.wf_param_search;
        wf.param_search.method = config.wf_search_method;
        wf.param_search.max_trials = config.wf_search_max_trials;
        wf.param_search.seed = config.wf_search_seed;
        wf.param_search.space = config.wf_search_space;
        wf_report = experiment::runWalkForward(config, wf);
        std::cout << '\n' << experiment::formatWalkForwardReport(wf_report);
    }

    if (!out_dir.empty()) {
        std::filesystem::create_directories(out_dir);
        const auto md = out_dir / (config.name + "_report.md");
        const auto csv = out_dir / (config.name + "_results.csv");
        report::writeReportFiles(research, md, csv);
        std::cout << "\nWrote " << md.string() << '\n'
                  << "Wrote " << csv.string() << '\n';

        if (config.run_walk_forward) {
            const auto wf_path = out_dir / (config.name + "_walk_forward.json");
            std::ofstream wf_out(wf_path);
            wf_out << report::formatWalkForwardJson(wf_report);
            std::cout << "Wrote " << wf_path.string() << '\n';
        }
    }

    if (!json_out.empty()) {
        if (json_out.has_parent_path()) {
            std::filesystem::create_directories(json_out.parent_path());
        }
        std::ofstream out(json_out);
        if (config.run_walk_forward) {
            out << "{\"comparison\":" << report::formatResearchJson(research)
                << ",\"walk_forward\":"
                << report::formatWalkForwardJson(wf_report) << '}';
        } else {
            out << report::formatResearchJson(research);
        }
        std::cout << "Wrote " << json_out.string() << '\n';
    }

    return 0;
}
