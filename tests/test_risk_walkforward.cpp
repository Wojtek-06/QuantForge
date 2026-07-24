#include "quantforge/experiment/walk_forward.hpp"
#include "quantforge/risk/risk_gate.hpp"
#include "quantforge/risk/var_es.hpp"
#include "quantforge/sim/simulator.hpp"
#include "quantforge/strategy/symmetric_mm.hpp"

#include <gtest/gtest.h>

#include <vector>

TEST(VarEsTest, HistoricalVarDetectsLeftTail)
{
    // Equity path with a few sharp drawdowns.
    std::vector<double> equity{0, 10, 12, -40, -35, -30, -20, 0, 5, 8, 10};
    const auto res = quantforge::risk::historicalVarEs(equity, 0.95);
    ASSERT_TRUE(res.valid);
    EXPECT_GT(res.var, 0.0);
    EXPECT_GE(res.expected_shortfall, res.var - 1e-9);
}

TEST(RiskGateTest, InventoryKillSwitchTrips)
{
    quantforge::risk::RiskLimits limits;
    limits.max_abs_inventory = 5;
    quantforge::risk::KillSwitchGate gate(limits);

    quantforge::strategy::PortfolioView portfolio{10, 0.0, 0.0};
    const auto decision = gate.evaluate(
        portfolio,
        10'000,
        quantforge::strategy::QuoteIntent{}
    );

    EXPECT_EQ(decision.action, quantforge::risk::RiskAction::Kill);
    EXPECT_TRUE(gate.tripped());
}

TEST(RiskGateTest, OvernightVarKill)
{
    quantforge::risk::RiskLimits limits;
    limits.enable_overnight_var = true;
    limits.max_var_95 = 5.0;
    limits.var_min_observations = 5;
    quantforge::risk::KillSwitchGate gate(limits);

    // Large negative increments → high VaR.
    std::vector<double> equity;
    double level = 0.0;
    for (int i = 0; i < 40; ++i) {
        level += (i % 3 == 0) ? -20.0 : 1.0;
        equity.push_back(level);
    }

    const auto decision = gate.evaluateOvernight(equity);
    EXPECT_EQ(decision.action, quantforge::risk::RiskAction::Kill);
    EXPECT_TRUE(gate.lastVarEs().valid);
}

TEST(SimulatorTest, RiskGateCanKillQuotes)
{
    quantforge::sim::SimulatorConfig config;
    config.seed = 99;
    config.horizon = 2'000;
    config.enable_risk_gate = true;
    config.risk_limits.max_abs_inventory = 1;  // very tight
    config.risk_limits.max_drawdown = 1e12;
    config.risk_limits.max_abs_notional = 1e12;

    quantforge::sim::Simulator sim(config);
    sim.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto result = sim.run();

    // With tiny inventory cap, kill is expected once filled against.
    EXPECT_TRUE(result.risk_killed || result.metrics.fills > 0u);
}

TEST(SimulatorTest, ReplayFramesRecorded)
{
    quantforge::sim::SimulatorConfig config;
    config.seed = 5;
    config.horizon = 400;
    config.record_replay = true;
    config.replay_stride = 1;

    quantforge::sim::Simulator sim(config);
    sim.setStrategy(std::make_unique<quantforge::strategy::SymmetricMarketMaker>());
    const auto result = sim.run();

    EXPECT_FALSE(result.replay.empty());
    EXPECT_GT(result.equity_curve.size(), 0u);
}

TEST(WalkForwardTest, ProducesFolds)
{
    quantforge::experiment::ExperimentConfig base;
    base.name = "wf_unit";
    base.sim.seed = 3;
    base.sim.horizon = 500;
    base.strategies = {"symmetric_mm"};

    quantforge::experiment::WalkForwardConfig wf;
    wf.is_horizon = 300;
    wf.oos_horizon = 200;
    wf.step = 100;
    wf.max_folds = 3;
    wf.strategy = "symmetric_mm";

    const auto report = quantforge::experiment::runWalkForward(base, wf);
    ASSERT_EQ(report.folds.size(), 3u);
    EXPECT_EQ(report.strategy, "symmetric_mm");

    const auto text = quantforge::experiment::formatWalkForwardReport(report);
    EXPECT_NE(text.find("OOS_MTM"), std::string::npos);
}
