#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace quantforge::risk {

/// Historical-simulation VaR / Expected Shortfall on P&L increments.
/// Aligned with overnight stress hooks used by Cross-Asset-Risk-Engine style
/// portfolio risk (here applied to MM equity path rather than option books).
struct VarEsResult {
    double var{0.0};              ///< positive number = loss magnitude at α
    double expected_shortfall{0.0};
    double confidence{0.95};
    std::size_t observations{0};
    bool valid{false};
};

inline std::vector<double> pnlIncrements(const std::vector<double>& equity_path)
{
    std::vector<double> pnl;
    if (equity_path.size() < 2) {
        return pnl;
    }
    pnl.reserve(equity_path.size() - 1);
    for (std::size_t i = 1; i < equity_path.size(); ++i) {
        pnl.push_back(equity_path[i] - equity_path[i - 1]);
    }
    return pnl;
}

/// One-sided historical VaR at `confidence` (e.g. 0.95 → 5% worst left tail).
inline VarEsResult historicalVarEs(
    const std::vector<double>& equity_path,
    double confidence = 0.95
)
{
    VarEsResult out;
    out.confidence = confidence;

    auto pnl = pnlIncrements(equity_path);
    out.observations = pnl.size();
    if (pnl.size() < 5 || confidence <= 0.0 || confidence >= 1.0) {
        return out;
    }

    std::sort(pnl.begin(), pnl.end());  // ascending: worst losses first
    const double alpha = 1.0 - confidence;
    const auto idx = static_cast<std::size_t>(
        std::floor(alpha * static_cast<double>(pnl.size()))
    );
    const auto safe_idx = std::min(idx, pnl.size() - 1);

    // VaR as positive loss magnitude (negate the left-tail P&L).
    out.var = std::max(0.0, -pnl[safe_idx]);

    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i <= safe_idx; ++i) {
        sum += pnl[i];
        ++count;
    }
    if (count > 0) {
        out.expected_shortfall = std::max(0.0, -(sum / static_cast<double>(count)));
    }
    out.valid = true;
    return out;
}

} // namespace quantforge::risk
