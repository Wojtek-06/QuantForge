#pragma once

#include "quantforge/marketdata/types.hpp"

#include <filesystem>
#include <string>

namespace quantforge::marketdata {

/// CSV schema (header required):
///   timestamp,kind,side,price,quantity
/// kind ∈ {mid, market, limit}
/// side ∈ {buy, sell} (required for market/limit; empty for mid)
/// Lines starting with '#' are comments.
ValidationResult validateSeries(const MarketEventSeries& events);

MarketEventSeries loadCsv(const std::filesystem::path& path);

MarketEventSeries parseCsvString(const std::string& csv_text);

/// Write a synthetic deterministic flow for demos/tests.
void writeSyntheticCsv(
    const std::filesystem::path& path,
    engine::Timestamp horizon,
    engine::Price initial_mid,
    std::uint64_t seed
);

} // namespace quantforge::marketdata
