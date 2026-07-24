#include "quantforge/sim/replay.hpp"

#include <iomanip>
#include <sstream>

namespace quantforge::sim {
namespace {

void appendOptionalPrice(
    std::ostringstream& out,
    const char* key,
    const std::optional<engine::Price>& price
)
{
    out << '"' << key << "\":";
    if (price) {
        out << *price;
    } else {
        out << "null";
    }
}

} // namespace

std::string formatReplayJson(
    const std::vector<ReplayFrame>& frames,
    const std::string& strategy_name
)
{
    std::ostringstream out;
    out << std::fixed;
    out << "{\n  \"strategy\": \"" << strategy_name << "\",\n";
    out << "  \"frame_count\": " << frames.size() << ",\n";
    out << "  \"frames\": [\n";

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        out << "    {";
        out << "\"t\":" << f.time << ',';
        out << "\"mid\":" << f.mid << ',';
        appendOptionalPrice(out, "best_bid", f.best_bid);
        out << ',';
        appendOptionalPrice(out, "best_ask", f.best_ask);
        out << ',';
        out << "\"bid_qty\":" << f.bid_qty << ',';
        out << "\"ask_qty\":" << f.ask_qty << ',';
        out << "\"inventory\":" << f.inventory << ',';
        out << std::setprecision(4) << "\"mtm\":" << f.mtm_pnl << ',';
        out << "\"quoting\":" << (f.quoting ? "true" : "false") << ',';
        out << "\"quote_bid\":" << f.quote_bid << ',';
        out << "\"quote_ask\":" << f.quote_ask << ',';
        out << "\"quote_bid_size\":" << f.quote_bid_size << ',';
        out << "\"quote_ask_size\":" << f.quote_ask_size;
        out << '}';
        if (i + 1 < frames.size()) {
            out << ',';
        }
        out << '\n';
    }

    out << "  ]\n}\n";
    return out.str();
}

} // namespace quantforge::sim
