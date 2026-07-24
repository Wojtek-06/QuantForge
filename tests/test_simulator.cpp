#include "quantforge/experiment/experiment.hpp"
#include "quantforge/signals/signals.hpp"
#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/no_trade.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <gtest/gtest.h>

#include <cmath>

TEST(SignalsTest, MicropriceAndOfi)
{
    quantforge::signals::BookSnapshot book;
    book.best_bid = 100;
    book.best_ask = 102;
    book.bid_size = 30;
    book.ask_size = 10;

    ASSERT_TRUE(quantforge::signals::spread(book).has_value());
    EXPECT_EQ(*quantforge::signals::spread(book), 2);

    ASSERT_TRUE(quantforge::signals::microprice(book).has_value());
    EXPECT_NEAR(*quantforge::signals::microprice(book), 101.5, 1e-9);

    ASSERT_TRUE(quantforge::signals::orderFlowImbalance(book).has_value());
    EXPECT_NEAR(*quantforge::signals::orderFlowImbalance(book), 0.5, 1e-9);
}

TEST(SimulatorTest, DeterministicReplay)
{
    quantforge::sim::SimulatorConfig config;
    config.seed = 7;
    config.horizon = 1'000;

    quantforge::sim::Simulator a(config);
    a.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto ra = a.run();

    quantforge::sim::Simulator b(config);
    b.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto rb = b.run();

    EXPECT_EQ(ra.events_processed, rb.events_processed);
    EXPECT_DOUBLE_EQ(ra.metrics.mtm_pnl, rb.metrics.mtm_pnl);
    EXPECT_EQ(ra.final_snapshot.inventory, rb.final_snapshot.inventory);
    EXPECT_EQ(ra.metrics.fills, rb.metrics.fills);
}

TEST(SimulatorTest, NoTradeDoesNotAccumulateInventory)
{
    quantforge::sim::SimulatorConfig config;
    config.seed = 11;
    config.horizon = 2'000;

    quantforge::sim::Simulator sim(config);
    sim.setStrategy(std::make_unique<quantforge::strategy::NoTradeStrategy>());
    const auto result = sim.run();

    EXPECT_EQ(result.final_snapshot.inventory, 0);
    EXPECT_EQ(result.metrics.fills, 0u);
}

TEST(SimulatorTest, SymmetricMmReceivesFills)
{
    quantforge::sim::SimulatorConfig config;
    config.seed = 42;
    config.horizon = 2'000;

    quantforge::sim::Simulator sim(config);
    sim.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto result = sim.run();

    EXPECT_GT(result.metrics.quotes_posted, 0u);
    EXPECT_GT(result.metrics.fills, 0u);
}

TEST(ExperimentTest, ComparisonRunsAllStrategies)
{
    quantforge::experiment::ExperimentConfig config;
    config.name = "unit";
    config.sim.seed = 3;
    config.sim.horizon = 800;
    config.strategies = {"no_trade", "symmetric_mm", "avellaneda_stoikov"};

    const auto report = quantforge::experiment::runComparison(config);

    ASSERT_EQ(report.results.size(), 3u);
    EXPECT_EQ(report.results[0].strategy_name, "no_trade");
    EXPECT_EQ(report.results[1].strategy_name, "symmetric_mm");
    EXPECT_EQ(report.results[2].strategy_name, "avellaneda_stoikov");

    const auto text = quantforge::experiment::formatReport(report);
    EXPECT_NE(text.find("avellaneda_stoikov"), std::string::npos);
}

TEST(SimulatorTest, CancelLatencyCanChangeFillOutcome)
{
    // Same seed/horizon: delayed cancels leave stale quotes exposed to flow.
    quantforge::sim::SimulatorConfig immediate;
    immediate.seed = 99;
    immediate.horizon = 3'000;
    immediate.latency.strategy_latency = 1;
    immediate.latency.cancel_latency = 0;

    quantforge::sim::SimulatorConfig delayed = immediate;
    delayed.latency.cancel_latency = 4;

    quantforge::sim::Simulator a(immediate);
    a.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto ra = a.run();

    quantforge::sim::Simulator b(delayed);
    b.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto rb = b.run();

    // Deterministic but distinct regimes — delayed cancel must not be a no-op.
    EXPECT_TRUE(
        ra.metrics.fills != rb.metrics.fills ||
        ra.final_snapshot.inventory != rb.final_snapshot.inventory ||
        std::abs(ra.metrics.mtm_pnl - rb.metrics.mtm_pnl) > 1e-9
    );
}
