#include "quantforge/experiment/walk_forward.hpp"

#include "quantforge/experiment/param_search.hpp"
#include "quantforge/risk/var_es.hpp"

#include <algorithm>
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
    report.param_search_enabled = wf.param_search.enabled;
    report.search_method = wf.param_search.method;

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
        row.oos_seed = row.is_seed + 1ULL +
            static_cast<std::uint64_t>(fold) * static_cast<std::uint64_t>(wf.step);

        ExperimentConfig is_cfg = base;
        is_cfg.name = base.name + "_is_" + std::to_string(fold);
        is_cfg.sim.seed = row.is_seed;
        is_cfg.sim.horizon = static_cast<engine::Timestamp>(wf.is_horizon);
        is_cfg.strategies = {wf.strategy};

        ExperimentConfig oos_cfg = base;
        oos_cfg.name = base.name + "_oos_" + std::to_string(fold);
        oos_cfg.sim.seed = row.oos_seed;
        oos_cfg.sim.horizon = static_cast<engine::Timestamp>(wf.oos_horizon);
        oos_cfg.strategies = {wf.strategy};

        strategy::StrategyParams selected = wf.fixed_params;
        std::vector<strategy::ParamTrial> trials;

        if (wf.param_search.enabled) {
            // Selection uses the IS fold only — OOS config is never passed here.
            selected = searchBestParamsIsOnly(
                is_cfg,
                wf.strategy,
                wf.param_search,
                &trials
            );
            row.is_trials = trials.size();
            if (!trials.empty()) {
                auto best_it = std::max_element(
                    trials.begin(),
                    trials.end(),
                    [](const strategy::ParamTrial& a, const strategy::ParamTrial& b) {
                        return a.is_score < b.is_score;
                    }
                );
                row.is_selection_score = best_it->is_score;
            }
        }

        row.selected_params = selected;
        row.is_result = runWithParams(is_cfg, wf.strategy, selected);
        row.oos_result = runWithParams(oos_cfg, wf.strategy, selected);
        row.oos_var_es = row.oos_result.simulation.overnight_var_es;

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
    out << "Folds: " << report.folds.size() << "\n";
    if (report.param_search_enabled) {
        out << "IS param search: " << report.search_method
            << " (frozen for OOS)\n";
    }
    out << '\n';

    out << std::left
        << std::setw(8) << "Fold"
        << std::right
        << std::setw(14) << "IS_MTM"
        << std::setw(14) << "OOS_MTM"
        << std::setw(12) << "OOS_fills"
        << std::setw(12) << "OOS_VaR95"
        << std::setw(10) << "killed"
        << std::setw(10) << "hs/g"
        << '\n';
    out << std::string(92, '-') << '\n';
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
            << (fold.oos_result.simulation.risk_killed ? "yes" : "no");
        if (report.strategy == "symmetric_mm") {
            out << std::setw(10) << fold.selected_params.half_spread;
        } else {
            out << std::setw(10) << fold.selected_params.gamma;
        }
        out << '\n';
    }

    out << '\n'
        << "Mean IS MTM:  " << report.is_mtm_mean << '\n'
        << "Mean OOS MTM: " << report.oos_mtm_mean << '\n'
        << "Sum OOS MTM:  " << report.oos_mtm_sum << '\n'
        << "Note: parameter selection uses IS folds only; OOS is evaluation.\n";

    return out.str();
}

} // namespace quantforge::experiment
