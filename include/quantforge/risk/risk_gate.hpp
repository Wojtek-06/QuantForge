#pragma once

#include "quantforge/risk/var_es.hpp"
#include "quantforge/strategy/strategy.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace quantforge::risk {

/// Kill-switch concepts aligned with Cross-Asset-Risk-Engine portfolio limits.
/// C++ gate enforces inventory / notional / drawdown / overnight VaR inside the
/// sim; Python `risk_bridge` optionally layers pybind MC stress on top.
enum class RiskAction {
    Allow,
    BlockQuotes,
    Flatten,
    Kill
};

struct RiskLimits {
    std::int64_t max_abs_inventory{100};
    double max_abs_notional{1'000'000.0};
    double max_drawdown{50'000.0};
    /// Overnight historical VaR (95%) kill threshold (loss magnitude).
    double max_var_95{25'000.0};
    /// Minimum equity samples before VaR kill is armed.
    std::size_t var_min_observations{30};
    bool enable_overnight_var{false};
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
    virtual RiskDecision evaluateOvernight(
        const std::vector<double>& equity_path
    ) const = 0;
    virtual void reset() = 0;
    virtual std::string name() const = 0;
};

/// Inventory / notional / drawdown / overnight-VaR kill switch.
class KillSwitchGate final : public IRiskGate {
public:
    explicit KillSwitchGate(RiskLimits limits = {})
        : limits_(limits)
    {
    }

    std::string name() const override { return "kill_switch"; }

    void reset() override
    {
        tripped_ = false;
        last_var_ = {};
    }

    bool tripped() const { return tripped_; }

    const VarEsResult& lastVarEs() const { return last_var_; }

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

        if (portfolio.mtm_pnl < -limits_.max_drawdown) {
            tripped_ = true;
            return {
                RiskAction::Kill,
                "drawdown kill (" + std::to_string(portfolio.mtm_pnl) + ")"
            };
        }

        return {RiskAction::Allow, "ok"};
    }

    RiskDecision evaluateOvernight(
        const std::vector<double>& equity_path
    ) const override
    {
        if (!limits_.enabled || !limits_.enable_overnight_var) {
            return {RiskAction::Allow, "overnight var disabled"};
        }
        if (tripped_) {
            return {RiskAction::Kill, "kill switch already tripped"};
        }

        last_var_ = historicalVarEs(equity_path, 0.95);
        if (!last_var_.valid ||
            last_var_.observations < limits_.var_min_observations) {
            return {RiskAction::Allow, "insufficient equity history for VaR"};
        }

        if (last_var_.var > limits_.max_var_95) {
            tripped_ = true;
            return {
                RiskAction::Kill,
                "overnight VaR95 kill (" + std::to_string(last_var_.var) +
                    " > " + std::to_string(limits_.max_var_95) + ")"
            };
        }

        return {RiskAction::Allow, "overnight var ok"};
    }

    const RiskLimits& limits() const { return limits_; }

private:
    RiskLimits limits_{};
    mutable bool tripped_{false};
    mutable VarEsResult last_var_{};
};

} // namespace quantforge::risk
