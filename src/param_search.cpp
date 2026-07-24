#include "quantforge/experiment/param_search.hpp"

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>

namespace quantforge::experiment {
namespace {

strategy::StrategyParams applySymmetric(
    engine::Price half_spread,
    engine::Quantity quote_size
)
{
    strategy::StrategyParams p;
    p.half_spread = half_spread;
    p.quote_size = quote_size;
    return p;
}

strategy::StrategyParams applyAvellaneda(
    double gamma,
    double sigma,
    double T,
    engine::Quantity quote_size
)
{
    strategy::StrategyParams p;
    p.gamma = gamma;
    p.sigma = sigma;
    p.T = T;
    p.quote_size = quote_size;
    return p;
}

} // namespace

std::vector<strategy::StrategyParams> enumerateCandidates(
    const std::string& strategy,
    const strategy::ParamSearchConfig& search
)
{
    std::vector<strategy::StrategyParams> out;
    const auto& space = search.space;

    if (strategy == "no_trade") {
        out.push_back(strategy::StrategyParams{});
        return out;
    }

    if (strategy == "symmetric_mm") {
        const auto spreads = space.half_spreads.empty()
            ? std::vector<engine::Price>{5}
            : space.half_spreads;
        const auto sizes = space.quote_sizes.empty()
            ? std::vector<engine::Quantity>{10}
            : space.quote_sizes;

        if (search.method == "random") {
            std::mt19937_64 rng(search.seed);
            std::uniform_int_distribution<std::size_t> ds(0, spreads.size() - 1);
            std::uniform_int_distribution<std::size_t> dq(0, sizes.size() - 1);
            const auto n = std::max<std::size_t>(1, search.max_trials);
            out.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                out.push_back(applySymmetric(spreads[ds(rng)], sizes[dq(rng)]));
            }
            return out;
        }

        for (const auto hs : spreads) {
            for (const auto qs : sizes) {
                out.push_back(applySymmetric(hs, qs));
                if (out.size() >= search.max_trials) {
                    return out;
                }
            }
        }
        return out;
    }

    if (strategy == "avellaneda_stoikov") {
        const auto gammas = space.gammas.empty()
            ? std::vector<double>{0.1}
            : space.gammas;
        const auto sigmas = space.sigmas.empty()
            ? std::vector<double>{5.0}
            : space.sigmas;
        const auto Ts = space.horizons_T.empty()
            ? std::vector<double>{1.0}
            : space.horizons_T;
        const auto sizes = space.quote_sizes.empty()
            ? std::vector<engine::Quantity>{10}
            : space.quote_sizes;

        if (search.method == "random") {
            std::mt19937_64 rng(search.seed);
            std::uniform_int_distribution<std::size_t> dg(0, gammas.size() - 1);
            std::uniform_int_distribution<std::size_t> dsig(0, sigmas.size() - 1);
            std::uniform_int_distribution<std::size_t> dT(0, Ts.size() - 1);
            std::uniform_int_distribution<std::size_t> dq(0, sizes.size() - 1);
            const auto n = std::max<std::size_t>(1, search.max_trials);
            out.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                out.push_back(applyAvellaneda(
                    gammas[dg(rng)],
                    sigmas[dsig(rng)],
                    Ts[dT(rng)],
                    sizes[dq(rng)]
                ));
            }
            return out;
        }

        for (const auto g : gammas) {
            for (const auto s : sigmas) {
                for (const auto t : Ts) {
                    for (const auto qs : sizes) {
                        out.push_back(applyAvellaneda(g, s, t, qs));
                        if (out.size() >= search.max_trials) {
                            return out;
                        }
                    }
                }
            }
        }
        return out;
    }

    throw std::invalid_argument("Unknown strategy for param search: " + strategy);
}

StrategyResult runWithParams(
    const ExperimentConfig& config,
    const std::string& strategy,
    const strategy::StrategyParams& params
)
{
    sim::Simulator simulator(config.sim);
    simulator.setStrategy(makeStrategy(strategy, params));

    StrategyResult row;
    row.strategy_name = strategy;
    row.params = params;
    row.simulation = simulator.run();
    return row;
}

strategy::StrategyParams searchBestParamsIsOnly(
    const ExperimentConfig& is_config,
    const std::string& strategy,
    const strategy::ParamSearchConfig& search,
    std::vector<strategy::ParamTrial>* trials_out
)
{
    auto candidates = enumerateCandidates(strategy, search);
    if (candidates.empty()) {
        return strategy::StrategyParams{};
    }

    strategy::StrategyParams best = candidates.front();
    double best_score = -std::numeric_limits<double>::infinity();

    ExperimentConfig cfg = is_config;
    cfg.strategies = {strategy};

    for (const auto& candidate : candidates) {
        const auto row = runWithParams(cfg, strategy, candidate);
        const double score = row.simulation.metrics.mtm_pnl;
        if (trials_out != nullptr) {
            trials_out->push_back(strategy::ParamTrial{candidate, score});
        }
        if (score > best_score) {
            best_score = score;
            best = candidate;
        }
    }

    return best;
}

} // namespace quantforge::experiment
