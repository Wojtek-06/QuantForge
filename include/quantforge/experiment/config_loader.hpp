#pragma once

#include "quantforge/experiment/experiment.hpp"

#include <filesystem>
#include <string>

namespace quantforge::experiment {

/// Load a flat experiment JSON (no nested objects required).
/// Supported keys: name, seed, horizon, tick_interval, exogenous_qty,
/// initial_mid, maker_fee_bps, taker_fee_bps, strategy_latency,
/// market_data_csv, enable_risk_gate, max_abs_inventory, max_drawdown,
/// max_abs_notional, max_var_95, enable_overnight_var, overnight_check_every,
/// run_walk_forward, wf_*, wf_param_search, wf_search_*, mm_half_spreads,
/// mm_quote_sizes, as_gammas, as_sigmas, as_horizons_T, mm_half_spread,
/// mm_quote_size, as_gamma, as_sigma, as_T, as_k, strategies (string[]).
ExperimentConfig loadConfigFile(const std::filesystem::path& path);

ExperimentConfig loadConfigString(const std::string& json_text);

} // namespace quantforge::experiment
