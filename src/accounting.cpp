#include "quantforge/metrics/accounting.hpp"

namespace quantforge::metrics {

Accounting::Accounting(engine::ParticipantId participant)
    : participant_(participant)
{
}

void Accounting::onTrade(const engine::Trade& trade, bool we_are_taker)
{
    const bool we_bought =
        (trade.buyer_id == participant_);
    const bool we_sold =
        (trade.seller_id == participant_);

    if (!we_bought && !we_sold) {
        return;
    }

    ++mm_.fills;
    const double notional =
        static_cast<double>(trade.price) * static_cast<double>(trade.quantity);
    const double our_fee = we_are_taker ? trade.taker_fee : trade.maker_fee;

    state_.fees_paid += our_fee;

    if (we_bought) {
        state_.cash -= notional + our_fee;
        state_.inventory += static_cast<std::int64_t>(trade.quantity);

        // Spread capture heuristic: buying below mid is positive.
        if (state_.last_mid > 0) {
            const double edge =
                static_cast<double>(state_.last_mid - trade.price) *
                static_cast<double>(trade.quantity);
            mm_.spread_capture += edge;
            signed_markout_ += edge;
        }
    } else {
        state_.cash += notional - our_fee;
        state_.inventory -= static_cast<std::int64_t>(trade.quantity);

        if (state_.last_mid > 0) {
            const double edge =
                static_cast<double>(trade.price - state_.last_mid) *
                static_cast<double>(trade.quantity);
            mm_.spread_capture += edge;
            signed_markout_ += edge;
        }
    }

    abs_filled_qty_ += static_cast<std::int64_t>(trade.quantity);
    markToMarket(state_.last_mid);
}

void Accounting::markToMarket(engine::Price mid)
{
    if (mid > 0) {
        state_.last_mid = mid;
    }

    const double inventory_value =
        static_cast<double>(state_.inventory) *
        static_cast<double>(state_.last_mid);

    state_.mtm_pnl = state_.cash + inventory_value;
    mm_.mtm_pnl = state_.mtm_pnl;
    mm_.inventory_pnl = inventory_value;

    if (state_.mtm_pnl > state_.peak_equity) {
        state_.peak_equity = state_.mtm_pnl;
    }

    const double drawdown = state_.peak_equity - state_.mtm_pnl;
    if (drawdown > state_.max_drawdown) {
        state_.max_drawdown = drawdown;
    }
    mm_.max_drawdown = state_.max_drawdown;

    if (abs_filled_qty_ > 0) {
        mm_.adverse_selection = -signed_markout_ /
            static_cast<double>(abs_filled_qty_);
    }

    equity_path_.push_back(state_.mtm_pnl);
}

void Accounting::recordQuoteAttempt()
{
    ++mm_.quote_attempts;
}

void Accounting::recordQuotePosted()
{
    ++mm_.quotes_posted;
}

const PortfolioSnapshot& Accounting::snapshot() const
{
    return state_;
}

MmMetrics Accounting::metrics() const
{
    MmMetrics out = mm_;
    if (mm_.quote_attempts > 0) {
        out.fill_rate =
            static_cast<double>(mm_.fills) /
            static_cast<double>(mm_.quote_attempts);
    }
    return out;
}

const std::vector<double>& Accounting::equityPath() const
{
    return equity_path_;
}

} // namespace quantforge::metrics
