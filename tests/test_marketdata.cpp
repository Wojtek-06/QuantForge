#include "quantforge/experiment/config_loader.hpp"
#include "quantforge/marketdata/as_of.hpp"
#include "quantforge/marketdata/csv_loader.hpp"
#include "quantforge/risk/risk_gate.hpp"
#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <gtest/gtest.h>

#include <filesystem>

using namespace quantforge;

TEST(MarketDataTest, ParseAndValidateCsv)
{
    const std::string csv =
        "timestamp,kind,side,price,quantity\n"
        "0,mid,,10000,0\n"
        "10,market,buy,0,5\n"
        "10,limit,sell,10020,8\n"
        "20,mid,,10005,0\n";

    const auto series = marketdata::parseCsvString(csv);
    ASSERT_EQ(series.size(), 4u);
    EXPECT_EQ(series[0].kind, marketdata::EventKind::Mid);
    EXPECT_EQ(series[1].kind, marketdata::EventKind::Market);
    ASSERT_TRUE(series[1].side.has_value());
    EXPECT_EQ(*series[1].side, engine::Side::Buy);

    const auto ok = marketdata::validateSeries(series);
    EXPECT_TRUE(ok.ok);
}

TEST(MarketDataTest, RejectsUnsortedTimestamps)
{
    marketdata::MarketEventSeries bad{
        {10, marketdata::EventKind::Mid, std::nullopt, 100, 0},
        {5, marketdata::EventKind::Mid, std::nullopt, 101, 0},
    };
    const auto v = marketdata::validateSeries(bad);
    EXPECT_FALSE(v.ok);
}

TEST(AsOfTest, MidAsOfBlocksFutureAccess)
{
    marketdata::MarketEventSeries events{
        {0, marketdata::EventKind::Mid, std::nullopt, 10000, 0},
        {50, marketdata::EventKind::Mid, std::nullopt, 10100, 0},
    };
    marketdata::AsOfSeries series(events);
    series.advanceTo(10);

    ASSERT_TRUE(series.midAsOf(10).has_value());
    EXPECT_EQ(*series.midAsOf(10), 10000);

    EXPECT_THROW(series.midAsOf(50), marketdata::LookAheadError);
}

TEST(AsOfTest, ClockCannotMoveBackwards)
{
    marketdata::AsOfClock clock;
    clock.advanceTo(20);
    EXPECT_THROW(clock.advanceTo(10), std::invalid_argument);
}

TEST(SimulatorMarketDataTest, ReplaysCsvDeterministically)
{
    const auto tmp = std::filesystem::temp_directory_path() /
        "quantforge_test_flow.csv";
    marketdata::writeSyntheticCsv(tmp, 500, 10000, 99);
    auto series = marketdata::loadCsv(tmp);

    sim::SimulatorConfig config;
    config.seed = 99;
    config.horizon = 500;
    config.flow_mode = sim::FlowMode::MarketData;
    config.market_data = series;

    sim::Simulator a(config);
    a.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto ra = a.run();

    sim::Simulator b(config);
    b.setStrategy(std::make_unique<strategy::SymmetricMarketMaker>());
    const auto rb = b.run();

    EXPECT_EQ(ra.events_processed, rb.events_processed);
    EXPECT_DOUBLE_EQ(ra.metrics.mtm_pnl, rb.metrics.mtm_pnl);
    EXPECT_EQ(ra.metrics.fills, rb.metrics.fills);
}

TEST(RiskGateTest, TripsOnInventory)
{
    risk::RiskLimits limits;
    limits.max_abs_inventory = 5;
    risk::KillSwitchGate gate(limits);

    strategy::PortfolioView portfolio{6, 0.0, 0.0};
    strategy::QuoteIntent intent;
    intent.quote = true;

    const auto d = gate.evaluate(portfolio, 10000, intent);
    EXPECT_EQ(d.action, risk::RiskAction::Kill);
    EXPECT_TRUE(gate.tripped());
}

TEST(ConfigLoaderTest, ParsesFlatJson)
{
    const std::string json = R"({
      "name": "unit_cfg",
      "seed": 7,
      "horizon": 123,
      "strategies": ["no_trade", "symmetric_mm"],
      "enable_risk_gate": true,
      "max_abs_inventory": 25
    })";

    const auto cfg = experiment::loadConfigString(json);
    EXPECT_EQ(cfg.name, "unit_cfg");
    EXPECT_EQ(cfg.sim.seed, 7u);
    EXPECT_EQ(cfg.sim.horizon, 123u);
    ASSERT_EQ(cfg.strategies.size(), 2u);
    EXPECT_TRUE(cfg.sim.enable_risk_gate);
    EXPECT_EQ(cfg.sim.risk_limits.max_abs_inventory, 25);
}
