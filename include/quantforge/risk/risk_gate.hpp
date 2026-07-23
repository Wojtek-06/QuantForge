#pragma once

#include "quantforge/strategy/strategy.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

namespace quantforge::risk {

/// Stub interface aligned with Cross-Asset-Risk-Engine kill-switch concepts.
/// Full pybind integration is deferred; this gate is enforceable inside the sim.
enum class RiskAction {
    Allow,
    BlockQuotes,
    Flatten,   ///< future: force reduce-only / cancel
    Kill       ///< hard stop — no new quotes
};

struct RiskLimits {
    std::int64_t max_abs_inventory{100};
    double max_abs_notional{1'000'000.0};
    double max_drawdown{50'000.0};
    bool enabled{true};
};

struct RiskDecision {
    RiskAction action{RiskAction::Allow};
    std::string reason;
};

class IRiskGate {
public:
    virtual ~IRiskGate() = default;
    virtual RiskDecision evaluate(
        const strategy::PortfolioView& portfolio,
        engine::Price mid,
        const strategy::QuoteIntent& intent
    ) const = 0;
    virtual void reset() = 0;
    virtual std::string name() const = 0;
};

/// Inventory / drawdown kill-switch stub (C++ side; Risk Engine hooks later).
class KillSwitchGate final : public IRiskGate {
public:
    explicit KillSwitchGate(RiskLimits limits = {})
        : limits_(limits)
    {
    }

    std::string name() const override { return "kill_switch"; }

    void reset() override { tripped_ = false; }

    bool tripped() const { return tripped_; }

    RiskDecision evaluate(
        const strategy::PortfolioView& portfolio,
        engine::Price mid,
        const strategy::QuoteIntent& /*intent*/
    ) const override
    {
        if (!limits_.enabled) {
            return {RiskAction::Allow, "disabled"};
        }

        if (tripped_) {
            return {RiskAction::Kill, "kill switch already tripped"};
        }

        if (std::llabs(portfolio.inventory) > limits_.max_abs_inventory) {
            tripped_ = true;
            return {
                RiskAction::Kill,
                "inventory limit breached (|q|=" +
                    std::to_string(portfolio.inventory) + ")"
            };
        }

        const double notional =
            std::llabs(portfolio.inventory) * static_cast<double>(mid);
        if (notional > limits_.max_abs_notional) {
            tripped_ = true;
            return {
                RiskAction::Kill,
                "notional limit breached (" + std::to_string(notional) + ")"
            };
        }

        // Drawdown is tracked as negative MTM vs peak in accounting; callers
        // may pass mtm_pnl as a proxy until Risk Engine VaR/ES hooks land.
        if (portfolio.mtm_pnl < -limits_.max_drawdown) {
            tripped_ = true;
            return {
                RiskAction::Kill,
                "drawdown kill (" + std::to_string(portfolio.mtm_pnl) + ")"
            };
        }

        return {RiskAction::Allow, "ok"};
    }

    const RiskLimits& limits() const { return limits_; }

private:
    RiskLimits limits_{};
    mutable bool tripped_{false};
};

} // namespace quantforge::risk
