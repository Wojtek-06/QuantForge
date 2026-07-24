#include "quantforge/experiment/config_loader.hpp"

#include "quantforge/marketdata/csv_loader.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace quantforge::experiment {
namespace {

std::string trim(std::string s)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string stripQuotes(std::string s)
{
    s = trim(s);
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool extractBool(const std::string& json, const std::string& key, bool& out)
{
    const auto needle = "\"" + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }
    auto i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
    if (json.compare(i, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (json.compare(i, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool extractNumber(const std::string& json, const std::string& key, double& out)
{
    const auto needle = "\"" + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }
    auto i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
    try {
        std::size_t eaten = 0;
        out = std::stod(json.substr(i), &eaten);
        return eaten > 0;
    } catch (...) {
        return false;
    }
}

bool extractString(const std::string& json, const std::string& key, std::string& out)
{
    const auto needle = "\"" + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }
    const auto q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) {
        return false;
    }
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) {
        return false;
    }
    out = json.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

std::vector<std::string> extractStringArray(
    const std::string& json,
    const std::string& key
)
{
    std::vector<std::string> out;
    const auto needle = "\"" + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) {
        return out;
    }
    const auto lb = json.find('[', pos + needle.size());
    if (lb == std::string::npos) {
        return out;
    }
    const auto rb = json.find(']', lb + 1);
    if (rb == std::string::npos) {
        return out;
    }
    const auto body = json.substr(lb + 1, rb - lb - 1);
    std::size_t i = 0;
    while (i < body.size()) {
        const auto q1 = body.find('"', i);
        if (q1 == std::string::npos) {
            break;
        }
        const auto q2 = body.find('"', q1 + 1);
        if (q2 == std::string::npos) {
            break;
        }
        out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return out;
}

} // namespace

ExperimentConfig loadConfigString(const std::string& json_text)
{
    ExperimentConfig config;
    double number = 0.0;
    std::string str;
    bool flag = false;

    if (extractString(json_text, "name", str)) {
        config.name = str;
    }
    if (extractNumber(json_text, "seed", number)) {
        config.sim.seed = static_cast<std::uint64_t>(number);
    }
    if (extractNumber(json_text, "horizon", number)) {
        config.sim.horizon = static_cast<engine::Timestamp>(number);
    }
    if (extractNumber(json_text, "tick_interval", number)) {
        config.sim.tick_interval = static_cast<engine::Timestamp>(number);
    }
    if (extractNumber(json_text, "exogenous_qty", number)) {
        config.sim.exogenous_qty = static_cast<engine::Quantity>(number);
    }
    if (extractNumber(json_text, "initial_mid", number)) {
        config.sim.initial_mid = static_cast<engine::Price>(number);
    }
    if (extractNumber(json_text, "maker_fee_bps", number)) {
        config.sim.fees.maker_fee_bps = number;
    }
    if (extractNumber(json_text, "taker_fee_bps", number)) {
        config.sim.fees.taker_fee_bps = number;
    }
    if (extractNumber(json_text, "strategy_latency", number)) {
        config.sim.latency.strategy_latency =
            static_cast<engine::Timestamp>(number);
    }
    if (extractBool(json_text, "enable_risk_gate", flag)) {
        config.sim.enable_risk_gate = flag;
    }
    if (extractNumber(json_text, "max_abs_inventory", number)) {
        config.sim.risk_limits.max_abs_inventory =
            static_cast<std::int64_t>(number);
    }
    if (extractNumber(json_text, "max_drawdown", number)) {
        config.sim.risk_limits.max_drawdown = number;
    }
    if (extractNumber(json_text, "max_abs_notional", number)) {
        config.sim.risk_limits.max_abs_notional = number;
    }
    if (extractNumber(json_text, "max_var_95", number)) {
        config.sim.risk_limits.max_var_95 = number;
    }
    if (extractBool(json_text, "enable_overnight_var", flag)) {
        config.sim.risk_limits.enable_overnight_var = flag;
    }
    if (extractNumber(json_text, "overnight_check_every", number)) {
        config.sim.overnight_check_every =
            static_cast<std::uint64_t>(number);
    }
    if (extractBool(json_text, "run_walk_forward", flag)) {
        config.run_walk_forward = flag;
    }
    if (extractNumber(json_text, "wf_is_horizon", number)) {
        config.wf_is_horizon = static_cast<std::size_t>(number);
    }
    if (extractNumber(json_text, "wf_oos_horizon", number)) {
        config.wf_oos_horizon = static_cast<std::size_t>(number);
    }
    if (extractNumber(json_text, "wf_step", number)) {
        config.wf_step = static_cast<std::size_t>(number);
    }
    if (extractNumber(json_text, "wf_max_folds", number)) {
        config.wf_max_folds = static_cast<std::size_t>(number);
    }
    if (extractString(json_text, "wf_strategy", str)) {
        config.wf_strategy = str;
    }

    auto strategies = extractStringArray(json_text, "strategies");
    if (!strategies.empty()) {
        config.strategies = std::move(strategies);
    }

    if (extractString(json_text, "market_data_csv", str)) {
        config.market_data_csv = stripQuotes(str);
        config.sim.flow_mode = sim::FlowMode::MarketData;
        // Path resolution / load happens in loadConfigFile when possible.
        if (std::filesystem::exists(config.market_data_csv)) {
            config.sim.market_data = marketdata::loadCsv(config.market_data_csv);
        }
    }

    return config;
}

ExperimentConfig loadConfigFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open experiment config: " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    auto config = loadConfigString(ss.str());
    if (config.name.empty()) {
        config.name = path.stem().string();
    }

    // Resolve relative CSV paths against the config file directory.
    if (!config.market_data_csv.empty()) {
        std::filesystem::path csv_path(config.market_data_csv);
        if (!csv_path.is_absolute()) {
            csv_path = (path.parent_path() / csv_path).lexically_normal();
        }
        if (!std::filesystem::exists(csv_path)) {
            throw std::runtime_error(
                "market_data_csv not found: " + csv_path.string()
            );
        }
        config.market_data_csv = csv_path.string();
        config.sim.market_data = marketdata::loadCsv(csv_path);
        config.sim.flow_mode = sim::FlowMode::MarketData;
    }

    return config;
}

} // namespace quantforge::experiment
