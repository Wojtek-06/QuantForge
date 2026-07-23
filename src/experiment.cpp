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
    if (name == "no_trade") {
        return std::make_unique<strategy::NoTradeStrategy>();
    }
    if (name == "symmetric_mm") {
        return std::make_unique<strategy::SymmetricMarketMaker>();
    }
    if (name == "avellaneda_stoikov") {
        return std::make_unique<strategy::AvellanedaStoikovMM>();
    }

    throw std::invalid_argument("Unknown strategy: " + name);
}

ExperimentReport runComparison(const ExperimentConfig& config)
{
    ExperimentReport report;
    report.experiment_name = config.name;

    for (const auto& strategy_name : config.strategies) {
        sim::Simulator simulator(config.sim);
        simulator.setStrategy(makeStrategy(strategy_name));

        StrategyResult row;
        row.strategy_name = strategy_name;
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
