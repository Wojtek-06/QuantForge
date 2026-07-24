#pragma once

#include "quantforge/engine/types.hpp"

#include <cmath>
#include <optional>

namespace quantforge::signals {

struct BookSnapshot {
    std::optional<engine::Price> best_bid;
    std::optional<engine::Price> best_ask;
    engine::Quantity bid_size{0};
    engine::Quantity ask_size{0};
};

inline std::optional<engine::Price> mid(const BookSnapshot& book)
{
    if (!book.best_bid || !book.best_ask) {
        return std::nullopt;
    }
    return (*book.best_bid + *book.best_ask) / 2;
}

inline std::optional<engine::Price> spread(const BookSnapshot& book)
{
    if (!book.best_bid || !book.best_ask) {
        return std::nullopt;
    }
    return *book.best_ask - *book.best_bid;
}

/// Size-weighted microprice; falls back to mid if sizes are zero.
inline std::optional<double> microprice(const BookSnapshot& book)
{
    if (!book.best_bid || !book.best_ask) {
        return std::nullopt;
    }

    const double bid = static_cast<double>(*book.best_bid);
    const double ask = static_cast<double>(*book.best_ask);
    const double bs = static_cast<double>(book.bid_size);
    const double as = static_cast<double>(book.ask_size);

    if (bs + as <= 0.0) {
        return (bid + ask) / 2.0;
    }

    return (ask * bs + bid * as) / (bs + as);
}

/// Order-flow imbalance stub: (bid_size - ask_size) / (bid_size + ask_size).
inline std::optional<double> orderFlowImbalance(const BookSnapshot& book)
{
    const double bs = static_cast<double>(book.bid_size);
    const double as = static_cast<double>(book.ask_size);
    if (bs + as <= 0.0) {
        return std::nullopt;
    }
    return (bs - as) / (bs + as);
}

/// Book-pressure toxicity proxy in [-1, 1] from top-of-book OFI.
inline std::optional<double> toxicityProxy(const BookSnapshot& book)
{
    const auto ofi = orderFlowImbalance(book);
    if (!ofi) {
        return std::nullopt;
    }
    return std::tanh(*ofi);
}

/// Blend top-of-book OFI toxicity with recent aggressor trade-flow toxicity.
inline double blendedToxicity(const BookSnapshot& book, double trade_toxicity)
{
    const auto book_tox = toxicityProxy(book);
    const double bt = book_tox ? *book_tox : 0.0;
    const double tt = std::tanh(trade_toxicity);
    return 0.5 * bt + 0.5 * tt;
}

/// EWMA realized vol update from a mid return (price units).
inline double ewmaVolatility(double prev_vol, double mid_return, double alpha = 0.05)
{
    const double r2 = mid_return * mid_return;
    if (prev_vol <= 0.0) {
        return std::sqrt(std::max(r2, 1e-12));
    }
    const double var = (1.0 - alpha) * (prev_vol * prev_vol) + alpha * r2;
    return std::sqrt(std::max(var, 1e-12));
}

} // namespace quantforge::signals
