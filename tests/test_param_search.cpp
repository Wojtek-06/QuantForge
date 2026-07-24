#include "quantforge/experiment/config_loader.hpp"
#include "quantforge/experiment/param_search.hpp"
#include "quantforge/experiment/walk_forward.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

TEST(ParamSearchTest, SelectionUsesIsOnlyNotOos)
{
    quantforge::strategy::ParamSearchConfig search;
    search.enabled = true;
    search.method = "grid";
    search.max_trials = 8;
    search.space.half_spreads = {2, 5, 10, 20};
    search.space.quote_sizes = {10};

    quantforge::experiment::ExperimentConfig is_cfg;
    is_cfg.name = "is_only";
    is_cfg.sim.seed = 11;
    is_cfg.sim.horizon = 600;
    is_cfg.strategies = {"symmetric_mm"};

    quantforge::experiment::ExperimentConfig oos_cfg = is_cfg;
    oos_cfg.name = "oos_probe";
    // Deliberately different regime from IS.
    oos_cfg.sim.seed = 10'007;
    oos_cfg.sim.horizon = 600;

    std::vector<quantforge::strategy::ParamTrial> trials;
    const auto selected = quantforge::experiment::searchBestParamsIsOnly(
        is_cfg,
        "symmetric_mm",
        search,
        &trials
    );
    ASSERT_FALSE(trials.empty());

    // Every recorded trial score must equal an IS-only simulation — never OOS.
    quantforge::strategy::StrategyParams best_by_is{};
    double best_is_score = -std::numeric_limits<double>::infinity();
    quantforge::strategy::StrategyParams best_by_oos{};
    double best_oos_score = -std::numeric_limits<double>::infinity();

    for (const auto& trial : trials) {
        const auto is_row = quantforge::experiment::runWithParams(
            is_cfg,
            "symmetric_mm",
            trial.params
        );
        const auto oos_row = quantforge::experiment::runWithParams(
            oos_cfg,
            "symmetric_mm",
            trial.params
        );
        EXPECT_DOUBLE_EQ(trial.is_score, is_row.simulation.metrics.mtm_pnl);
        // Selection score must not silently be the OOS score.
        if (std::abs(oos_row.simulation.metrics.mtm_pnl - is_row.simulation.metrics.mtm_pnl) >
            1e-9) {
            EXPECT_NE(trial.is_score, oos_row.simulation.metrics.mtm_pnl);
        }

        if (is_row.simulation.metrics.mtm_pnl > best_is_score) {
            best_is_score = is_row.simulation.metrics.mtm_pnl;
            best_by_is = trial.params;
        }
        if (oos_row.simulation.metrics.mtm_pnl > best_oos_score) {
            best_oos_score = oos_row.simulation.metrics.mtm_pnl;
            best_by_oos = trial.params;
        }
    }

    EXPECT_EQ(selected.half_spread, best_by_is.half_spread);
    EXPECT_EQ(selected.quote_size, best_by_is.quote_size);

    // When IS and OOS disagree on the winner, selection must follow IS.
    if (best_by_is.half_spread != best_by_oos.half_spread) {
        EXPECT_EQ(selected.half_spread, best_by_is.half_spread);
        EXPECT_NE(selected.half_spread, best_by_oos.half_spread);
    }
}

TEST(ParamSearchTest, WalkForwardFreezesIsWinnerForOos)
{
    quantforge::experiment::ExperimentConfig base;
    base.name = "wf_search";
    base.sim.seed = 21;
    base.sim.horizon = 400;
    base.strategies = {"symmetric_mm"};

    quantforge::experiment::WalkForwardConfig wf;
    wf.is_horizon = 400;
    wf.oos_horizon = 300;
    wf.step = 100;
    wf.max_folds = 2;
    wf.strategy = "symmetric_mm";
    wf.param_search.enabled = true;
    wf.param_search.method = "grid";
    wf.param_search.max_trials = 6;
    wf.param_search.space.half_spreads = {3, 8, 15};
    wf.param_search.space.quote_sizes = {5, 10};

    const auto report = quantforge::experiment::runWalkForward(base, wf);
    ASSERT_EQ(report.folds.size(), 2u);
    EXPECT_TRUE(report.param_search_enabled);

    for (const auto& fold : report.folds) {
        EXPECT_GT(fold.is_trials, 0u);

        quantforge::experiment::ExperimentConfig is_cfg = base;
        is_cfg.sim.seed = fold.is_seed;
        is_cfg.sim.horizon = static_cast<quantforge::engine::Timestamp>(wf.is_horizon);

        const auto expected = quantforge::experiment::searchBestParamsIsOnly(
            is_cfg,
            wf.strategy,
            wf.param_search
        );
        EXPECT_EQ(fold.selected_params.half_spread, expected.half_spread);
        EXPECT_EQ(fold.selected_params.quote_size, expected.quote_size);

        // Frozen params are what actually ran on OOS.
        EXPECT_EQ(
            fold.oos_result.params.half_spread,
            fold.selected_params.half_spread
        );
    }
}

TEST(ParamSearchTest, RandomSearchRespectsMaxTrials)
{
    quantforge::strategy::ParamSearchConfig search;
    search.method = "random";
    search.max_trials = 5;
    search.seed = 3;
    search.space.gammas = {0.05, 0.1, 0.2, 0.4};
    search.space.sigmas = {3.0, 5.0, 8.0};
    search.space.horizons_T = {0.5, 1.0};
    search.space.quote_sizes = {5, 10, 20};

    const auto candidates = quantforge::experiment::enumerateCandidates(
        "avellaneda_stoikov",
        search
    );
    EXPECT_EQ(candidates.size(), 5u);
}

TEST(ConfigLoaderTest, ParsesParamSearchKeys)
{
    const char* json = R"({
      "name": "search_cfg",
      "wf_param_search": true,
      "wf_search_method": "random",
      "wf_search_max_trials": 9,
      "wf_search_seed": 13,
      "mm_half_spreads": [2, 7, 11],
      "mm_quote_sizes": [4, 8],
      "as_gammas": [0.05, 0.2],
      "as_sigmas": [4.0, 6.0],
      "as_horizons_T": [0.5],
      "mm_half_spread": 7,
      "as_gamma": 0.15
    })";

    const auto cfg = quantforge::experiment::loadConfigString(json);
    EXPECT_TRUE(cfg.wf_param_search);
    EXPECT_EQ(cfg.wf_search_method, "random");
    EXPECT_EQ(cfg.wf_search_max_trials, 9u);
    EXPECT_EQ(cfg.wf_search_seed, 13u);
    ASSERT_EQ(cfg.wf_search_space.half_spreads.size(), 3u);
    EXPECT_EQ(cfg.wf_search_space.half_spreads[1], 7);
    ASSERT_EQ(cfg.wf_search_space.quote_sizes.size(), 2u);
    EXPECT_EQ(cfg.default_params.half_spread, 7);
    EXPECT_DOUBLE_EQ(cfg.default_params.gamma, 0.15);
}
