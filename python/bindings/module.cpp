#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "quantforge/engine/order.hpp"
#include "quantforge/engine/order_book.hpp"
#include "quantforge/experiment/config_loader.hpp"
#include "quantforge/experiment/experiment.hpp"
#include "quantforge/experiment/param_search.hpp"
#include "quantforge/experiment/walk_forward.hpp"
#include "quantforge/report/json_export.hpp"
#include "quantforge/sim/simulator.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

quantforge::sim::SimulatorConfig makeSimConfig(
    std::uint64_t seed,
    std::uint64_t horizon,
    std::uint64_t tick_interval,
    std::int64_t exogenous_qty,
    std::int64_t initial_mid,
    double maker_fee_bps,
    double taker_fee_bps,
    std::uint64_t strategy_latency,
    bool enable_risk_gate,
    bool enable_overnight_var
)
{
    quantforge::sim::SimulatorConfig cfg;
    cfg.seed = seed;
    cfg.horizon = horizon;
    cfg.tick_interval = tick_interval;
    cfg.exogenous_qty = static_cast<quantforge::engine::Quantity>(exogenous_qty);
    cfg.initial_mid = initial_mid;
    cfg.fees.maker_fee_bps = maker_fee_bps;
    cfg.fees.taker_fee_bps = taker_fee_bps;
    cfg.latency.strategy_latency = strategy_latency;
    cfg.enable_risk_gate = enable_risk_gate;
    cfg.risk_limits.enable_overnight_var = enable_overnight_var;
    return cfg;
}

py::dict metricsToDict(const quantforge::metrics::MmMetrics& m)
{
    py::dict d;
    d["mtm_pnl"] = m.mtm_pnl;
    d["max_drawdown"] = m.max_drawdown;
    d["fills"] = m.fills;
    d["fill_rate"] = m.fill_rate;
    d["spread_capture"] = m.spread_capture;
    d["adverse_selection"] = m.adverse_selection;
    d["quotes_posted"] = m.quotes_posted;
    d["inventory_pnl"] = m.inventory_pnl;
    return d;
}

py::dict paramsToDict(const quantforge::strategy::StrategyParams& p)
{
    py::dict d;
    d["half_spread"] = p.half_spread;
    d["quote_size"] = p.quote_size;
    d["max_inventory"] = p.max_inventory;
    d["gamma"] = p.gamma;
    d["sigma"] = p.sigma;
    d["T"] = p.T;
    d["k"] = p.k;
    d["min_half_spread"] = p.min_half_spread;
    return d;
}

quantforge::strategy::StrategyParams dictToParams(const py::dict& d)
{
    quantforge::strategy::StrategyParams p;
    if (d.contains("half_spread")) {
        p.half_spread = d["half_spread"].cast<std::int64_t>();
    }
    if (d.contains("quote_size")) {
        p.quote_size = d["quote_size"].cast<std::uint32_t>();
    }
    if (d.contains("max_inventory")) {
        p.max_inventory = d["max_inventory"].cast<std::int64_t>();
    }
    if (d.contains("gamma")) {
        p.gamma = d["gamma"].cast<double>();
    }
    if (d.contains("sigma")) {
        p.sigma = d["sigma"].cast<double>();
    }
    if (d.contains("T")) {
        p.T = d["T"].cast<double>();
    }
    if (d.contains("k")) {
        p.k = d["k"].cast<double>();
    }
    if (d.contains("min_half_spread")) {
        p.min_half_spread = d["min_half_spread"].cast<std::int64_t>();
    }
    return p;
}

py::dict resultToDict(const quantforge::experiment::StrategyResult& row)
{
    py::dict d;
    d["strategy"] = row.strategy_name;
    d["params"] = paramsToDict(row.params);
    d["metrics"] = metricsToDict(row.simulation.metrics);
    d["inventory"] = row.simulation.final_snapshot.inventory;
    d["cash"] = row.simulation.final_snapshot.cash;
    d["risk_killed"] = row.simulation.risk_killed;
    d["risk_reason"] = row.simulation.risk_reason;
    d["equity_curve"] = row.simulation.equity_curve;
    d["var_95"] = row.simulation.overnight_var_es.valid
        ? row.simulation.overnight_var_es.var
        : 0.0;
    d["es_95"] = row.simulation.overnight_var_es.valid
        ? row.simulation.overnight_var_es.expected_shortfall
        : 0.0;
    d["events"] = row.simulation.events_processed;
    return d;
}

py::dict runComparisonPy(
    std::uint64_t seed,
    std::uint64_t horizon,
    const std::vector<std::string>& strategies,
    const py::dict& params_dict,
    bool enable_risk_gate
)
{
    quantforge::experiment::ExperimentConfig cfg;
    cfg.name = "python_comparison";
    cfg.sim = makeSimConfig(
        seed,
        horizon,
        10,
        5,
        10'000,
        -0.2,
        1.0,
        1,
        enable_risk_gate,
        enable_risk_gate
    );
    if (!strategies.empty()) {
        cfg.strategies = strategies;
    }
    const auto params = dictToParams(params_dict);
    const auto report = quantforge::experiment::runComparison(cfg, params);

    py::list results;
    for (const auto& row : report.results) {
        results.append(resultToDict(row));
    }
    py::dict out;
    out["experiment"] = report.experiment_name;
    out["results"] = results;
    return out;
}

py::dict runWalkForwardPy(
    std::uint64_t seed,
    std::size_t is_horizon,
    std::size_t oos_horizon,
    std::size_t step,
    std::size_t max_folds,
    const std::string& strategy,
    bool param_search,
    const std::string& search_method,
    std::size_t max_trials,
    const py::dict& params_dict
)
{
    quantforge::experiment::ExperimentConfig base;
    base.name = "python_wf";
    base.sim = makeSimConfig(
        seed,
        is_horizon + oos_horizon,
        10,
        5,
        10'000,
        -0.2,
        1.0,
        1,
        false,
        false
    );
    base.strategies = {strategy};

    quantforge::experiment::WalkForwardConfig wf;
    wf.is_horizon = is_horizon;
    wf.oos_horizon = oos_horizon;
    wf.step = step;
    wf.max_folds = max_folds;
    wf.strategy = strategy;
    wf.fixed_params = dictToParams(params_dict);
    wf.param_search.enabled = param_search;
    wf.param_search.method = search_method;
    wf.param_search.max_trials = max_trials;

    const auto report = quantforge::experiment::runWalkForward(base, wf);
    // Reuse JSON exporter for a stable schema, then parse via Python? Prefer
    // structured dicts for research ergonomics.
    py::list folds;
    for (const auto& f : report.folds) {
        py::dict row;
        row["fold"] = f.fold_index;
        row["is_seed"] = f.is_seed;
        row["oos_seed"] = f.oos_seed;
        row["is_mtm"] = f.is_result.simulation.metrics.mtm_pnl;
        row["oos_mtm"] = f.oos_result.simulation.metrics.mtm_pnl;
        row["oos_fills"] = f.oos_result.simulation.metrics.fills;
        row["oos_var_95"] = f.oos_var_es.valid ? f.oos_var_es.var : 0.0;
        row["oos_risk_killed"] = f.oos_result.simulation.risk_killed;
        row["is_selection_score"] = f.is_selection_score;
        row["is_trials"] = f.is_trials;
        row["selected_params"] = paramsToDict(f.selected_params);
        row["is_equity_curve"] = f.is_result.simulation.equity_curve;
        row["oos_equity_curve"] = f.oos_result.simulation.equity_curve;
        folds.append(row);
    }

    py::dict out;
    out["experiment"] = report.experiment_name;
    out["strategy"] = report.strategy;
    out["is_mtm_mean"] = report.is_mtm_mean;
    out["oos_mtm_mean"] = report.oos_mtm_mean;
    out["oos_mtm_sum"] = report.oos_mtm_sum;
    out["param_search_enabled"] = report.param_search_enabled;
    out["search_method"] = report.search_method;
    out["folds"] = folds;
    return out;
}

py::dict searchParamsIsOnlyPy(
    std::uint64_t seed,
    std::uint64_t horizon,
    const std::string& strategy,
    const std::string& method,
    std::size_t max_trials
)
{
    quantforge::experiment::ExperimentConfig is_cfg;
    is_cfg.name = "python_is_search";
    is_cfg.sim = makeSimConfig(
        seed, horizon, 10, 5, 10'000, -0.2, 1.0, 1, false, false
    );
    is_cfg.strategies = {strategy};

    quantforge::strategy::ParamSearchConfig search;
    search.enabled = true;
    search.method = method;
    search.max_trials = max_trials;

    std::vector<quantforge::strategy::ParamTrial> trials;
    const auto best = quantforge::experiment::searchBestParamsIsOnly(
        is_cfg,
        strategy,
        search,
        &trials
    );

    py::list trial_list;
    for (const auto& t : trials) {
        py::dict row;
        row["params"] = paramsToDict(t.params);
        row["is_score"] = t.is_score;
        trial_list.append(row);
    }

    py::dict out;
    out["best_params"] = paramsToDict(best);
    out["trials"] = trial_list;
    out["strategy"] = strategy;
    return out;
}

py::dict runSimulationPy(
    const std::string& strategy,
    std::uint64_t seed,
    std::uint64_t horizon,
    const py::dict& params_dict,
    bool enable_risk_gate
)
{
    quantforge::experiment::ExperimentConfig cfg;
    cfg.name = "python_sim";
    cfg.sim = makeSimConfig(
        seed,
        horizon,
        10,
        5,
        10'000,
        -0.2,
        1.0,
        1,
        enable_risk_gate,
        enable_risk_gate
    );
    cfg.strategies = {strategy};
    const auto row = quantforge::experiment::runWithParams(
        cfg,
        strategy,
        dictToParams(params_dict)
    );
    return resultToDict(row);
}

} // namespace

PYBIND11_MODULE(_quantforge, m)
{
    m.doc() = "QuantForge native hot-path bindings (LOB, sim, walk-forward)";

    py::enum_<quantforge::engine::Side>(m, "Side")
        .value("Buy", quantforge::engine::Side::Buy)
        .value("Sell", quantforge::engine::Side::Sell);

    py::enum_<quantforge::engine::OrderType>(m, "OrderType")
        .value("Limit", quantforge::engine::OrderType::Limit)
        .value("Market", quantforge::engine::OrderType::Market)
        .value("Stop", quantforge::engine::OrderType::Stop);

    py::enum_<quantforge::engine::TimeInForce>(m, "TimeInForce")
        .value("GTC", quantforge::engine::TimeInForce::GTC)
        .value("IOC", quantforge::engine::TimeInForce::IOC)
        .value("FOK", quantforge::engine::TimeInForce::FOK);

    py::class_<quantforge::engine::Order>(m, "Order")
        .def(py::init<>())
        .def(py::init<
                 quantforge::engine::OrderId,
                 quantforge::engine::Side,
                 quantforge::engine::OrderType,
                 quantforge::engine::Price,
                 quantforge::engine::Quantity,
                 quantforge::engine::Timestamp,
                 quantforge::engine::TimeInForce,
                 quantforge::engine::ParticipantId>(),
             py::arg("id"),
             py::arg("side"),
             py::arg("type"),
             py::arg("price"),
             py::arg("quantity"),
             py::arg("timestamp"),
             py::arg("tif") = quantforge::engine::TimeInForce::GTC,
             py::arg("participant_id") = 0)
        .def_readwrite("id", &quantforge::engine::Order::id)
        .def_readwrite("side", &quantforge::engine::Order::side)
        .def_readwrite("type", &quantforge::engine::Order::type)
        .def_readwrite("price", &quantforge::engine::Order::price)
        .def_readwrite("quantity", &quantforge::engine::Order::quantity)
        .def_readwrite(
            "remaining_quantity",
            &quantforge::engine::Order::remaining_quantity
        )
        .def_readwrite("timestamp", &quantforge::engine::Order::timestamp)
        .def_readwrite("tif", &quantforge::engine::Order::tif)
        .def_readwrite(
            "participant_id",
            &quantforge::engine::Order::participant_id
        );

    py::class_<quantforge::engine::OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def(
            "add_order",
            [](quantforge::engine::OrderBook& book,
               const quantforge::engine::Order& order) {
                return book.addOrder(order).size();
            },
            py::arg("order"),
            "Add order; returns number of trades generated"
        )
        .def("cancel_order", &quantforge::engine::OrderBook::cancelOrder)
        .def(
            "best_bid",
            [](const quantforge::engine::OrderBook& book) -> py::object {
                const auto v = book.bestBid();
                return v ? py::cast(*v) : py::none();
            }
        )
        .def(
            "best_ask",
            [](const quantforge::engine::OrderBook& book) -> py::object {
                const auto v = book.bestAsk();
                return v ? py::cast(*v) : py::none();
            }
        )
        .def("bid_quantity_at", &quantforge::engine::OrderBook::bidQuantityAt)
        .def("ask_quantity_at", &quantforge::engine::OrderBook::askQuantityAt)
        .def("empty", &quantforge::engine::OrderBook::empty);

    py::class_<quantforge::sim::SimulatorConfig>(m, "SimulatorConfig")
        .def(py::init<>())
        .def_readwrite("seed", &quantforge::sim::SimulatorConfig::seed)
        .def_readwrite("horizon", &quantforge::sim::SimulatorConfig::horizon)
        .def_readwrite(
            "tick_interval",
            &quantforge::sim::SimulatorConfig::tick_interval
        )
        .def_readwrite(
            "exogenous_qty",
            &quantforge::sim::SimulatorConfig::exogenous_qty
        )
        .def_readwrite(
            "initial_mid",
            &quantforge::sim::SimulatorConfig::initial_mid
        )
        .def_readwrite(
            "enable_risk_gate",
            &quantforge::sim::SimulatorConfig::enable_risk_gate
        )
        .def_readwrite(
            "overnight_check_every",
            &quantforge::sim::SimulatorConfig::overnight_check_every
        )
        .def_readwrite(
            "record_replay",
            &quantforge::sim::SimulatorConfig::record_replay
        );

    m.def(
        "run_simulation",
        &runSimulationPy,
        py::arg("strategy") = "symmetric_mm",
        py::arg("seed") = 42,
        py::arg("horizon") = 2000,
        py::arg("params") = py::dict(),
        py::arg("enable_risk_gate") = false,
        "Run a single strategy simulation; returns metrics/equity/risk"
    );

    m.def(
        "run_comparison",
        &runComparisonPy,
        py::arg("seed") = 42,
        py::arg("horizon") = 2000,
        py::arg("strategies") = std::vector<std::string>{
            "no_trade",
            "symmetric_mm",
            "avellaneda_stoikov"
        },
        py::arg("params") = py::dict(),
        py::arg("enable_risk_gate") = false
    );

    m.def(
        "run_walk_forward",
        &runWalkForwardPy,
        py::arg("seed") = 42,
        py::arg("is_horizon") = 800,
        py::arg("oos_horizon") = 400,
        py::arg("step") = 400,
        py::arg("max_folds") = 3,
        py::arg("strategy") = "symmetric_mm",
        py::arg("param_search") = false,
        py::arg("search_method") = "grid",
        py::arg("max_trials") = 12,
        py::arg("params") = py::dict()
    );

    m.def(
        "search_params_is_only",
        &searchParamsIsOnlyPy,
        py::arg("seed") = 42,
        py::arg("horizon") = 800,
        py::arg("strategy") = "symmetric_mm",
        py::arg("method") = "grid",
        py::arg("max_trials") = 12,
        "IS-only parameter search (never uses OOS)"
    );

    m.def(
        "load_config_json",
        [](const std::string& json_text) {
            const auto cfg = quantforge::experiment::loadConfigString(json_text);
            py::dict d;
            d["name"] = cfg.name;
            d["seed"] = cfg.sim.seed;
            d["horizon"] = cfg.sim.horizon;
            d["strategies"] = cfg.strategies;
            d["run_walk_forward"] = cfg.run_walk_forward;
            d["wf_param_search"] = cfg.wf_param_search;
            d["wf_strategy"] = cfg.wf_strategy;
            d["default_params"] = paramsToDict(cfg.default_params);
            return d;
        },
        py::arg("json_text")
    );

    m.attr("__version__") = "0.4.0";
}
