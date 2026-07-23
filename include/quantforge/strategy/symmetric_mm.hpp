#pragma once

#include "quantforge/strategy/strategy.hpp"

#include <cstdlib>

namespace quantforge::strategy {

/// Symmetric two-sided market maker around fair / mid.
class SymmetricMarketMaker final : public IStrategy {
public:
    struct Params {
        engine::Price half_spread{5};
        engine::Quantity quote_size{10};
        std::int64_t max_inventory{100};
    };

    SymmetricMarketMaker() = default;

    explicit SymmetricMarketMaker(Params params)
        : params_(params)
    {
    }

    std::string name() const override { return "symmetric_mm"; }

    void reset() override {}

    QuoteIntent onTick(const BookView& book, const PortfolioView& portfolio) override
    {
        if (std::llabs(portfolio.inventory) >= params_.max_inventory) {
            return QuoteIntent{};
        }

        engine::Price center = book.fair_price;
        if (const auto m = signals::mid(book.book)) {
            center = *m;
        }

        QuoteIntent intent;
        intent.quote = true;
        intent.bid_price = center - params_.half_spread;
        intent.ask_price = center + params_.half_spread;
        intent.bid_size = params_.quote_size;
        intent.ask_size = params_.quote_size;

        if (intent.bid_price <= 0) {
            intent.bid_price = 1;
        }
        if (intent.ask_price <= intent.bid_price) {
            intent.ask_price = intent.bid_price + 1;
        }

        return intent;
    }

private:
    Params params_;
};

} // namespace quantforge::strategy
