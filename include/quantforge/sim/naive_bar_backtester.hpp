#pragma once

#include "quantforge/engine/types.hpp"
#include "quantforge/marketdata/types.hpp"
#include "quantforge/metrics/accounting.hpp"
#include "quantforge/strategy/strategy.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace quantforge::sim {

/// One OHLC-style bar aggregated from mid prints (high/low = max/min mid).
struct Bar {
    engine::Timestamp start{0};
    engine::Timestamp end{0};
    engine::Price open{0};
    engine::Price high{0};
    engine::Price low{0};
    engine::Price close{0};
};

struct NaiveBarConfig {
    engine::Timestamp bar_width{50};
    engine::ParticipantId strategy_participant{1};
    /// If true, decide quotes using the *same* bar close used for fills (leakage).
    bool leak_same_bar_close{true};
};

struct NaiveBarResult {
    std::string strategy_name;
    metrics::PortfolioSnapshot final_snapshot{};
    metrics::MmMetrics metrics{};
    std::uint64_t bars{0};
    std::uint64_t fantasy_fills{0};
    bool used_look_ahead{false};
};

/// Fantasy bar backtester: fills at bar close when high/low "touches" the quote.
/// Intentionally unrealistic — used as a leakage / methodology foil vs LOB sim.
class NaiveBarBacktester {
public:
    explicit NaiveBarBacktester(NaiveBarConfig config);

    void setStrategy(std::unique_ptr<strategy::IStrategy> strategy);

    static std::vector<Bar> buildBars(
        const marketdata::MarketEventSeries& events,
        engine::Timestamp bar_width
    );

    NaiveBarResult run(const marketdata::MarketEventSeries& events);

private:
    NaiveBarConfig config_;
    std::unique_ptr<strategy::IStrategy> strategy_;
};

} // namespace quantforge::sim
