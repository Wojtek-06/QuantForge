#pragma once

#include "quantforge/strategy/strategy.hpp"

namespace quantforge::strategy {

/// Baseline: never posts liquidity (for adverse-selection / opportunity cost).
class NoTradeStrategy final : public IStrategy {
public:
    std::string name() const override { return "no_trade"; }

    void reset() override {}

    QuoteIntent onTick(const BookView&, const PortfolioView&) override
    {
        return QuoteIntent{};
    }
};

} // namespace quantforge::strategy
