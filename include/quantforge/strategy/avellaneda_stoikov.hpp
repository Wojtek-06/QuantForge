#pragma once

#include "quantforge/strategy/strategy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace quantforge::strategy {

/// Inventory-aware Avellaneda–Stoikov style reservation price + skewed quotes.
///
/// reservation = mid - q * gamma * sigma^2 * T
/// optimal_spread ≈ gamma * sigma^2 * T + (2/gamma) * ln(1 + gamma/k)
class AvellanedaStoikovMM final : public IStrategy {
public:
    struct Params {
        double gamma{0.1};       ///< risk aversion
        double sigma{5.0};       ///< volatility (price units)
        double T{1.0};           ///< remaining horizon fraction
        double k{1.5};           ///< order arrival intensity proxy
        engine::Quantity quote_size{10};
        std::int64_t max_inventory{100};
        engine::Price min_half_spread{2};
    };

    AvellanedaStoikovMM() = default;

    explicit AvellanedaStoikovMM(Params params)
        : params_(params)
    {
    }

    std::string name() const override { return "avellaneda_stoikov"; }

    void reset() override {}

    QuoteIntent onTick(const BookView& book, const PortfolioView& portfolio) override
    {
        if (std::llabs(portfolio.inventory) >= params_.max_inventory) {
            return QuoteIntent{};
        }

        double mid = static_cast<double>(book.fair_price);
        if (const auto m = signals::mid(book.book)) {
            mid = static_cast<double>(*m);
        }
        if (const auto mp = signals::microprice(book.book)) {
            mid = *mp;
        }

        const double sigma =
            book.realized_vol > 0.0 ? book.realized_vol : params_.sigma;

        const double q = static_cast<double>(portfolio.inventory);
        const double variance_term =
            params_.gamma * sigma * sigma * params_.T;

        const double reservation = mid - q * variance_term;

        double half_spread =
            0.5 * variance_term +
            (1.0 / params_.gamma) * std::log(1.0 + params_.gamma / params_.k);

        // Widen when blended book + trade-flow toxicity is elevated.
        const double tox =
            signals::blendedToxicity(book.book, book.trade_toxicity);
        half_spread *= (1.0 + 0.5 * std::abs(tox));

        half_spread = std::max(
            half_spread,
            static_cast<double>(params_.min_half_spread)
        );

        QuoteIntent intent;
        intent.quote = true;
        intent.bid_price = static_cast<engine::Price>(
            std::floor(reservation - half_spread)
        );
        intent.ask_price = static_cast<engine::Price>(
            std::ceil(reservation + half_spread)
        );
        intent.bid_size = params_.quote_size;
        intent.ask_size = params_.quote_size;

        // Inventory skew: reduce size on the overloaded side.
        if (portfolio.inventory > 0) {
            intent.bid_size = std::max<engine::Quantity>(1, intent.bid_size / 2);
        } else if (portfolio.inventory < 0) {
            intent.ask_size = std::max<engine::Quantity>(1, intent.ask_size / 2);
        }

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
