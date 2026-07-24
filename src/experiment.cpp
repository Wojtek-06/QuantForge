#include "quantforge/experiment/experiment.hpp"

#include "quantforge/strategy/avellaneda_stoikov.hpp"
#include "quantforge/strategy/no_trade.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace quantforge::experiment {

std::unique_ptr<strategy::IStrategy> makeStrategy(const std::string& name)
{
    return makeStrategy(name, strategy::StrategyParams{});
}

std::unique_ptr<strategy::IStrategy> makeStrategy(
    const std::string& name,
    const strategy::StrategyParams& params
)
{
    if (name == "no_trade") {
        return std::make_unique<strategy::NoTradeStrategy>();
    }
    if (name == "symmetric_mm") {
        strategy::SymmetricMarketMaker::Params p;
        p.half_spread = params.half_spread;
        p.quote_size = params.quote_size;
        p.max_inventory = params.max_inventory;
        return std::make_unique<strategy::SymmetricMarketMaker>(p);
    }
    if (name == "avellaneda_stoikov") {
        strategy::AvellanedaStoikovMM::Params p;
        p.gamma = params.gamma;
        p.sigma = params.sigma;
        p.T = params.T;
        p.k = params.k;
        p.quote_size = params.quote_size;
        p.max_inventory = params.max_inventory;
        p.min_half_spread = params.min_half_spread;
        return std::make_unique<strategy::AvellanedaStoikovMM>(p);
    }

    throw std::invalid_argument("Unknown strategy: " + name);
}

ExperimentReport runComparison(const ExperimentConfig& config)
{
    return runComparison(config, config.default_params);
}

ExperimentReport runComparison(
    const ExperimentConfig& config,
    const strategy::StrategyParams& params
)
{
    ExperimentReport report;
    report.experiment_name = config.name;

    for (const auto& strategy_name : config.strategies) {
        sim::Simulator simulator(config.sim);
        simulator.setStrategy(makeStrategy(strategy_name, params));

        StrategyResult row;
        row.strategy_name = strategy_name;
        row.params = params;
        row.simulation = simulator.run();
        report.results.push_back(std::move(row));
    }

    return report;
}

std::string formatReport(const ExperimentReport& report)
{
    std::ostringstream out;

    out << "=== QuantForge Experiment: " << report.experiment_name << " ===\n\n";
    out << std::left
        << std::setw(22) << "Strategy"
        << std::right
        << std::setw(12) << "MTM PnL"
        << std::setw(12) << "Inv"
        << std::setw(12) << "MaxDD"
        << std::setw(12) << "Fills"
        << std::setw(12) << "FillRate"
        << std::setw(14) << "SpreadCap"
        << std::setw(14) << "AdvSel"
        << '\n';
    out << std::string(110, '-') << '\n';

    out << std::fixed << std::setprecision(2);

    for (const auto& row : report.results) {
        const auto& m = row.simulation.metrics;
        const auto& s = row.simulation.final_snapshot;

        out << std::left << std::setw(22) << row.strategy_name
            << std::right
            << std::setw(12) << m.mtm_pnl
            << std::setw(12) << s.inventory
            << std::setw(12) << m.max_drawdown
            << std::setw(12) << m.fills
            << std::setw(12) << m.fill_rate
            << std::setw(14) << m.spread_capture
            << std::setw(14) << m.adverse_selection
            << '\n';
    }

    out << '\n'
        << "Deterministic replay: identical SimulatorConfig.seed yields identical results.\n"
        << "Metrics: MTM = cash + inventory*mid; AdvSel ≈ -signed markout / qty.\n";

    return out.str();
}

} // namespace quantforge::experiment
