#include "quantforge/experiment/walk_forward.hpp"

#include "quantforge/risk/var_es.hpp"

#include <iomanip>
#include <sstream>

namespace quantforge::experiment {

WalkForwardReport runWalkForward(
    const ExperimentConfig& base,
    const WalkForwardConfig& wf
)
{
    WalkForwardReport report;
    report.experiment_name = base.name + "_walk_forward";
    report.strategy = wf.strategy;

    if (wf.is_horizon == 0 || wf.oos_horizon == 0 || wf.step == 0 ||
        wf.max_folds == 0) {
        return report;
    }

    double is_sum = 0.0;
    double oos_sum = 0.0;

    for (std::size_t fold = 0; fold < wf.max_folds; ++fold) {
        WalkForwardFold row;
        row.fold_index = fold;
        row.is_seed = base.sim.seed + static_cast<std::uint64_t>(fold) * 2ULL;
        row.oos_seed = row.is_seed + 1ULL;

        ExperimentConfig is_cfg = base;
        is_cfg.name = base.name + "_is_" + std::to_string(fold);
        is_cfg.sim.seed = row.is_seed;
        is_cfg.sim.horizon = static_cast<engine::Timestamp>(wf.is_horizon);
        is_cfg.strategies = {wf.strategy};

        ExperimentConfig oos_cfg = base;
        oos_cfg.name = base.name + "_oos_" + std::to_string(fold);
        oos_cfg.sim.seed = row.oos_seed;
        oos_cfg.sim.horizon = static_cast<engine::Timestamp>(wf.oos_horizon);
        // Emulate regime shift into the OOS window.
        oos_cfg.sim.seed = row.oos_seed + static_cast<std::uint64_t>(fold) *
            static_cast<std::uint64_t>(wf.step);
        oos_cfg.strategies = {wf.strategy};

        const auto is_report = runComparison(is_cfg);
        const auto oos_report = runComparison(oos_cfg);

        if (!is_report.results.empty()) {
            row.is_result = is_report.results.front();
        }
        if (!oos_report.results.empty()) {
            row.oos_result = oos_report.results.front();
            row.oos_var_es = row.oos_result.simulation.overnight_var_es;
        }

        is_sum += row.is_result.simulation.metrics.mtm_pnl;
        oos_sum += row.oos_result.simulation.metrics.mtm_pnl;
        report.folds.push_back(std::move(row));
    }

    const auto n = static_cast<double>(report.folds.size());
    report.is_mtm_mean = n > 0.0 ? is_sum / n : 0.0;
    report.oos_mtm_mean = n > 0.0 ? oos_sum / n : 0.0;
    report.oos_mtm_sum = oos_sum;
    return report;
}

std::string formatWalkForwardReport(const WalkForwardReport& report)
{
    std::ostringstream out;
    out << "=== Walk-forward / OOS: " << report.experiment_name << " ===\n";
    out << "Strategy: " << report.strategy << "\n";
    out << "Folds: " << report.folds.size() << "\n\n";

    out << std::left
        << std::setw(8) << "Fold"
        << std::right
        << std::setw(14) << "IS_MTM"
        << std::setw(14) << "OOS_MTM"
        << std::setw(12) << "OOS_fills"
        << std::setw(12) << "OOS_VaR95"
        << std::setw(10) << "killed"
        << '\n';
    out << std::string(70, '-') << '\n';
    out << std::fixed << std::setprecision(2);

    for (const auto& fold : report.folds) {
        out << std::left << std::setw(8) << fold.fold_index
            << std::right
            << std::setw(14) << fold.is_result.simulation.metrics.mtm_pnl
            << std::setw(14) << fold.oos_result.simulation.metrics.mtm_pnl
            << std::setw(12) << fold.oos_result.simulation.metrics.fills
            << std::setw(12)
            << (fold.oos_var_es.valid ? fold.oos_var_es.var : 0.0)
            << std::setw(10)
            << (fold.oos_result.simulation.risk_killed ? "yes" : "no")
            << '\n';
    }

    out << '\n'
        << "Mean IS MTM:  " << report.is_mtm_mean << '\n'
        << "Mean OOS MTM: " << report.oos_mtm_mean << '\n'
        << "Sum OOS MTM:  " << report.oos_mtm_sum << '\n'
        << "Note: each fold uses a fresh seeded regime (IS then OOS). "
           "Do not tune on OOS.\n";

    return out.str();
}

} // namespace quantforge::experiment
