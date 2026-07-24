#include "quantforge/report/json_export.hpp"

#include <iomanip>
#include <sstream>

namespace quantforge::report {
namespace {

void appendMetrics(std::ostringstream& out, const metrics::MmMetrics& m)
{
    out << std::setprecision(6);
    out << "\"mtm_pnl\":" << m.mtm_pnl << ','
        << "\"max_drawdown\":" << m.max_drawdown << ','
        << "\"fills\":" << m.fills << ','
        << "\"fill_rate\":" << m.fill_rate << ','
        << "\"spread_capture\":" << m.spread_capture << ','
        << "\"adverse_selection\":" << m.adverse_selection << ','
        << "\"quotes_posted\":" << m.quotes_posted << ','
        << "\"inventory_pnl\":" << m.inventory_pnl;
}

void appendResult(std::ostringstream& out, const experiment::StrategyResult& row)
{
    const auto& sim = row.simulation;
    out << '{';
    out << "\"strategy\":\"" << row.strategy_name << "\",";
    out << "\"inventory\":" << sim.final_snapshot.inventory << ',';
    out << "\"cash\":" << sim.final_snapshot.cash << ',';
    out << "\"events\":" << sim.events_processed << ',';
    out << "\"risk_killed\":" << (sim.risk_killed ? "true" : "false") << ',';
    out << "\"risk_reason\":\"" << sim.risk_reason << "\",";
    out << "\"var_95\":"
        << (sim.overnight_var_es.valid ? sim.overnight_var_es.var : 0.0) << ',';
    out << "\"es_95\":"
        << (sim.overnight_var_es.valid ? sim.overnight_var_es.expected_shortfall
                                       : 0.0)
        << ',';
    out << "\"metrics\":{";
    appendMetrics(out, sim.metrics);
    out << "}";

    out << ",\"equity_curve\":[";
    for (std::size_t i = 0; i < sim.equity_curve.size(); ++i) {
        if (i) {
            out << ',';
        }
        out << sim.equity_curve[i];
        // Cap payload size for API responses.
        if (i >= 2000) {
            break;
        }
    }
    out << ']';
    out << '}';
}

} // namespace

std::string formatExperimentJson(const experiment::ExperimentReport& report)
{
    std::ostringstream out;
    out << std::fixed;
    out << "{\"experiment\":\"" << report.experiment_name << "\",\"results\":[";
    for (std::size_t i = 0; i < report.results.size(); ++i) {
        if (i) {
            out << ',';
        }
        appendResult(out, report.results[i]);
    }
    out << "]}";
    return out.str();
}

std::string formatWalkForwardJson(const experiment::WalkForwardReport& report)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << '{'
        << "\"experiment\":\"" << report.experiment_name << "\","
        << "\"strategy\":\"" << report.strategy << "\","
        << "\"is_mtm_mean\":" << report.is_mtm_mean << ','
        << "\"oos_mtm_mean\":" << report.oos_mtm_mean << ','
        << "\"oos_mtm_sum\":" << report.oos_mtm_sum << ','
        << "\"folds\":[";

    for (std::size_t i = 0; i < report.folds.size(); ++i) {
        const auto& f = report.folds[i];
        if (i) {
            out << ',';
        }
        out << '{'
            << "\"fold\":" << f.fold_index << ','
            << "\"is_seed\":" << f.is_seed << ','
            << "\"oos_seed\":" << f.oos_seed << ','
            << "\"is_mtm\":" << f.is_result.simulation.metrics.mtm_pnl << ','
            << "\"oos_mtm\":" << f.oos_result.simulation.metrics.mtm_pnl << ','
            << "\"oos_fills\":" << f.oos_result.simulation.metrics.fills << ','
            << "\"oos_var_95\":"
            << (f.oos_var_es.valid ? f.oos_var_es.var : 0.0) << ','
            << "\"oos_risk_killed\":"
            << (f.oos_result.simulation.risk_killed ? "true" : "false") << ','
            << "\"is_selection_score\":" << f.is_selection_score << ','
            << "\"is_trials\":" << f.is_trials << ','
            << "\"selected_params\":{"
            << "\"half_spread\":" << f.selected_params.half_spread << ','
            << "\"quote_size\":" << f.selected_params.quote_size << ','
            << "\"gamma\":" << f.selected_params.gamma << ','
            << "\"sigma\":" << f.selected_params.sigma << ','
            << "\"T\":" << f.selected_params.T << ','
            << "\"k\":" << f.selected_params.k
            << "}"
            << '}';
    }
    out << "],"
        << "\"param_search_enabled\":"
        << (report.param_search_enabled ? "true" : "false") << ','
        << "\"search_method\":\"" << report.search_method << "\""
        << '}';
    return out.str();
}

std::string formatResearchJson(const ResearchReport& report)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << '{'
        << "\"notes\":\"" << report.notes << "\","
        << "\"experiment\":" << formatExperimentJson(report.experiment) << ','
        << "\"leakage\":[";

    for (std::size_t i = 0; i < report.leakage_cases.size(); ++i) {
        const auto& c = report.leakage_cases[i];
        if (i) {
            out << ',';
        }
        out << '{'
            << "\"title\":\"" << c.title << "\","
            << "\"lob_mtm\":" << c.lob.simulation.metrics.mtm_pnl << ','
            << "\"naive_mtm\":" << c.naive.metrics.mtm_pnl << ','
            << "\"fantasy_fills\":" << c.naive.fantasy_fills
            << '}';
    }
    out << "]}";
    return out.str();
}

} // namespace quantforge::report
