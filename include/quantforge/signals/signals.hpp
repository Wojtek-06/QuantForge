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

/// Toxicity stub: signed pressure proxy in [-1, 1]. Realised later via VPIN/OFI.
inline std::optional<double> toxicityProxy(const BookSnapshot& book)
{
    const auto ofi = orderFlowImbalance(book);
    if (!ofi) {
        return std::nullopt;
    }
    // Soft-clip for a bounded "toxicity" score used by strategies.
    return std::tanh(*ofi);
}

} // namespace quantforge::signals
