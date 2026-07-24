#pragma once

#include "quantforge/engine/types.hpp"
#include "quantforge/signals/signals.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace quantforge::strategy {

struct BookView {
    signals::BookSnapshot book{};
    engine::Price fair_price{0};
    engine::Timestamp now{0};
    /// EWMA mid-return volatility (price units); 0 if not yet estimated.
    double realized_vol{0.0};
    /// EWMA signed aggressor flow (buy +qty / sell -qty), used for toxicity.
    double trade_toxicity{0.0};
};

struct PortfolioView {
    std::int64_t inventory{0};
    double cash{0.0};
    double mtm_pnl{0.0};
};

struct QuoteIntent {
    bool quote{false};
    engine::Price bid_price{0};
    engine::Price ask_price{0};
    engine::Quantity bid_size{0};
    engine::Quantity ask_size{0};
};

class IStrategy {
public:
    virtual ~IStrategy() = default;

    virtual std::string name() const = 0;
    virtual void reset() = 0;
    virtual QuoteIntent onTick(const BookView& book, const PortfolioView& portfolio) = 0;
};

} // namespace quantforge::strategy
