#include "quantforge/marketdata/csv_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace quantforge::marketdata {
namespace {

std::string trim(std::string s)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string toLower(std::string s)
{
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            fields.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    fields.push_back(trim(cur));
    return fields;
}

EventKind parseKind(const std::string& raw)
{
    const auto k = toLower(raw);
    if (k == "mid") {
        return EventKind::Mid;
    }
    if (k == "market") {
        return EventKind::Market;
    }
    if (k == "limit") {
        return EventKind::Limit;
    }
    throw std::invalid_argument("Unknown market-data kind: " + raw);
}

std::optional<engine::Side> parseSide(const std::string& raw)
{
    if (raw.empty()) {
        return std::nullopt;
    }
    const auto s = toLower(raw);
    if (s == "buy" || s == "b") {
        return engine::Side::Buy;
    }
    if (s == "sell" || s == "s") {
        return engine::Side::Sell;
    }
    throw std::invalid_argument("Unknown side: " + raw);
}

} // namespace

ValidationResult validateSeries(const MarketEventSeries& events)
{
    if (events.empty()) {
        return {false, "empty market-data series"};
    }

    engine::Timestamp prev = 0;
    bool first = true;
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        if (!first && e.timestamp < prev) {
            return {
                false,
                "timestamps not sorted non-decreasing at index " +
                    std::to_string(i)
            };
        }
        first = false;
        prev = e.timestamp;

        if (e.kind == EventKind::Mid) {
            if (e.price <= 0) {
                return {false, "mid event requires positive price"};
            }
            continue;
        }

        if (!e.side.has_value()) {
            return {false, "order event requires side"};
        }
        if (e.quantity == 0) {
            return {false, "order event requires positive quantity"};
        }
        if (e.kind == EventKind::Limit && e.price <= 0) {
            return {false, "limit event requires positive price"};
        }
    }

    return {true, "ok"};
}

MarketEventSeries parseCsvString(const std::string& csv_text)
{
    std::istringstream in(csv_text);
    std::string line;
    bool saw_header = false;
    MarketEventSeries events;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto fields = splitCsvLine(line);
        if (fields.size() < 5) {
            throw std::invalid_argument(
                "CSV row needs timestamp,kind,side,price,quantity"
            );
        }

        if (!saw_header) {
            const auto h0 = toLower(fields[0]);
            const auto h1 = toLower(fields[1]);
            if (h0 == "timestamp" && h1 == "kind") {
                saw_header = true;
                continue;
            }
            throw std::invalid_argument(
                "CSV must start with header: timestamp,kind,side,price,quantity"
            );
        }

        MarketEvent e;
        e.timestamp = static_cast<engine::Timestamp>(std::stoull(fields[0]));
        e.kind = parseKind(fields[1]);
        e.side = parseSide(fields[2]);
        e.price = static_cast<engine::Price>(std::stoll(fields[3]));
        e.quantity = static_cast<engine::Quantity>(std::stoul(fields[4]));
        events.push_back(e);
    }

    if (!saw_header) {
        throw std::invalid_argument("CSV missing header row");
    }

    const auto v = validateSeries(events);
    if (!v.ok) {
        throw std::invalid_argument("invalid market-data series: " + v.message);
    }

    return events;
}

MarketEventSeries loadCsv(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open market-data CSV: " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseCsvString(ss.str());
}

void writeSyntheticCsv(
    const std::filesystem::path& path,
    engine::Timestamp horizon,
    engine::Price initial_mid,
    std::uint64_t seed
)
{
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<engine::Timestamp> gap(1, 5);
    std::bernoulli_distribution buy_side(0.5);
    std::bernoulli_distribution jump(0.02);
    std::uniform_int_distribution<int> jump_size(-20, 20);
    std::bernoulli_distribution aggressive(0.65);

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to write synthetic CSV: " + path.string());
    }

    out << "timestamp,kind,side,price,quantity\n";
    out << "0,mid,," << initial_mid << ",0\n";

    engine::Price local_fair = initial_mid;
    engine::Timestamp t = 1;
    const engine::Quantity qty = 5;

    while (t < horizon) {
        if (jump(rng)) {
            local_fair += jump_size(rng);
            if (local_fair < 100) {
                local_fair = 100;
            }
        }

        out << t << ",mid,," << local_fair << ",0\n";

        const char* side = buy_side(rng) ? "buy" : "sell";
        if (aggressive(rng)) {
            out << t << ",market," << side << ",0," << qty << '\n';
        } else {
            const engine::Price limit_px =
                (std::string(side) == "buy") ? local_fair + 8 : local_fair - 8;
            out << t << ",limit," << side << ','
                << (limit_px > 0 ? limit_px : 1) << ',' << qty << '\n';
        }

        t += gap(rng);
    }
}

} // namespace quantforge::marketdata
