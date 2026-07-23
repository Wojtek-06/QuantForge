#pragma once

#include "quantforge/engine/trade.hpp"
#include "quantforge/engine/types.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace quantforge::metrics {

struct PortfolioSnapshot {
    std::int64_t inventory{0};
    double cash{0.0};
    double fees_paid{0.0};
    double realised_pnl{0.0};
    double mtm_pnl{0.0};
    double peak_equity{0.0};
    double max_drawdown{0.0};
    engine::Price last_mid{0};
};

struct MmMetrics {
    double spread_capture{0.0};
    double inventory_pnl{0.0};
    double mtm_pnl{0.0};
    double max_drawdown{0.0};
    double fill_rate{0.0};
    double adverse_selection{0.0};
    std::uint64_t quotes_posted{0};
    std::uint64_t fills{0};
    std::uint64_t quote_attempts{0};
};

class Accounting {
public:
    explicit Accounting(engine::ParticipantId participant);

    void onTrade(const engine::Trade& trade, bool we_are_taker);
    void markToMarket(engine::Price mid);
    void recordQuoteAttempt();
    void recordQuotePosted();

    const PortfolioSnapshot& snapshot() const;
    MmMetrics metrics() const;

private:
    engine::ParticipantId participant_;
    PortfolioSnapshot state_{};
    MmMetrics mm_{};

    double volume_weighted_entry_{0.0};
    std::int64_t abs_filled_qty_{0};
    double signed_markout_{0.0};
};

} // namespace quantforge::metrics
