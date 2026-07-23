#include "quantforge/marketdata/as_of.hpp"
#include "quantforge/marketdata/csv_loader.hpp"
#include "quantforge/report/comparison_report.hpp"
#include "quantforge/sim/naive_bar_backtester.hpp"
#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace quantforge;

namespace {

marketdata::MarketEventSeries trendingSeries()
{
    // Rising mid path so same-bar-close leakage can mint optimistic fills.
    std::string csv =
        "timestamp,kind,side,price,quantity\n"
        "0,mid,,10000,0\n";
    for (int t = 10; t <= 400; t += 10) {
        const int mid = 10000 + t;
        csv += std::to_string(t) + ",mid,," + std::to_string(mid) + ",0\n";
        csv += std::to_string(t) + ",market,buy,0,5\n";
        csv += std::to_string(t) + ",market,sell,0,5\n";
    }
    return marketdata::parseCsvString(csv);
}

} // namespace

TEST(LeakageTest, GuardFailsWhenFutureMidRequested)
{
    auto series = marketdata::AsOfSeries(trendingSeries());
    series.advanceTo(100);

    // Unguarded helper can see future — this is what a leaking research script does.
    ASSERT_TRUE(series.midUnguarded(300).has_value());
    EXPECT_GT(*series.midUnguarded(300), *series.midAsOf(100));

    // Guarded API must fail closed.
    EXPECT_THROW(
        {
            try {
                (void)series.midAsOf(300);
            } catch (const marketdata::LookAheadError&) {
                throw;
            }
        },
        marketdata::LookAheadError
    );
}

TEST(LeakageTest, SimulatorDoesNotApplyFutureMidBeforeClock)
{
    marketdata::MarketEventSeries events{
        {0, marketdata::EventKind::Mid, std::nullopt, 10000, 0},
        {100, marketdata::EventKind::Mid, std::nullopt, 12000, 0},
        {100, marketdata::EventKind::Market, engine::Side::Buy, 0, 5},
    };

    marketdata::AsOfSeries series(events);
    series.advanceTo(50);
    ASSERT_TRUE(series.midAsOf(50).has_value());
    EXPECT_EQ(*series.midAsOf(50), 10000);

    // Explicit failure mode for research code that forgets as-of.
    EXPECT_THROW(series.clock().guard(100, "unit_test_future"), marketdata::LookAheadError);
}

TEST(LeakageTest, NaiveBarFantasyFillsDifferFromLob)
{
    const auto series = trendingSeries();

    sim::SimulatorConfig lob_cfg;
    lob_cfg.horizon = 400;
    lob_cfg.tick_interval = 10;
    lob_cfg.flow_mode = sim::FlowMode::MarketData;
    lob_cfg.market_data = series;
    lob_cfg.latency.strategy_latency = 1;
    lob_cfg.fees = engine::FeeSchedule{-0.2, 1.0};

    sim::Simulator lob(lob_cfg);
    lob.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto lob_result = lob.run();

    sim::NaiveBarBacktester naive({50, 1, true});
    naive.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto naive_result = naive.run(series);

    EXPECT_TRUE(naive_result.used_look_ahead);
    EXPECT_GT(naive_result.fantasy_fills, 0u);
    EXPECT_GT(lob_result.metrics.fills, 0u);

    // Methodology check: paths must not be identical (fantasy ≠ matching).
    const bool pnl_differs =
        std::abs(naive_result.metrics.mtm_pnl - lob_result.metrics.mtm_pnl) > 1e-6;
    const bool fills_differ =
        naive_result.fantasy_fills != lob_result.metrics.fills;
    EXPECT_TRUE(pnl_differs || fills_differ)
        << "naive bar and LOB produced identical outcomes — leakage foil broken";
}

TEST(LeakageTest, SameBarCloseLeakIsDocumentedInReport)
{
    const auto series = trendingSeries();

    sim::SimulatorConfig lob_cfg;
    lob_cfg.horizon = 400;
    lob_cfg.flow_mode = sim::FlowMode::MarketData;
    lob_cfg.market_data = series;

    sim::Simulator lob(lob_cfg);
    lob.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto lob_result = lob.run();

    sim::NaiveBarBacktester naive({50, 1, true});
    naive.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto naive_result = naive.run(series);

    report::ResearchReport research;
    research.experiment.experiment_name = "leakage_unit";
    experiment::StrategyResult row;
    row.strategy_name = "symmetric_mm";
    row.simulation = lob_result;
    research.experiment.results.push_back(row);

    report::LeakageCaseStudy cs;
    cs.title = "unit";
    cs.lob = row;
    cs.naive = naive_result;
    research.leakage_cases.push_back(cs);

    const auto md = report::formatMarkdown(research);
    EXPECT_NE(md.find("same-bar close leakage"), std::string::npos);
    EXPECT_NE(md.find("naive bar vs LOB"), std::string::npos);
}

TEST(LeakageTest, SyntheticCsvRoundTripPreservesAsOfOrdering)
{
    const auto path = std::filesystem::temp_directory_path() /
        "quantforge_leakage_synth.csv";
    marketdata::writeSyntheticCsv(path, 200, 10000, 3);
    const auto series = marketdata::loadCsv(path);

    marketdata::AsOfSeries asof(series);
    asof.advanceTo(100);
    const auto mid = asof.midAsOf(100);
    ASSERT_TRUE(mid.has_value());

    // Any mid printed after clock must remain inaccessible.
    bool saw_future_mid = false;
    for (const auto& e : series) {
        if (e.kind == marketdata::EventKind::Mid && e.timestamp > 100) {
            saw_future_mid = true;
            EXPECT_THROW(asof.midAsOf(e.timestamp), marketdata::LookAheadError);
            break;
        }
    }
    EXPECT_TRUE(saw_future_mid);
}
