# Methodology: why gross spread ≠ profit

## The fantasy-fill problem

A common research mistake:

1. Strategy “buys” at the bar close (or mid).
2. Backtester credits the fill at that price with certainty.
3. Reported Sharpe looks excellent — until live trading.

That path **looks ahead** (uses the same bar’s close for decision and fill) and **ignores queue, latency, partial fills, and adverse selection**.

QuantForge keeps this path explicitly as a **foil**: `NaiveBarBacktester`. Leakage tests fail if the foil cannot be distinguished from the LOB simulator on the same tape.

## Realistic path (LOB sim)

1. Strategy observes **as-of** book / mid only (`AsOfSeries` throws on future access).
2. Quotes are submitted as resting limit orders (subject to latency).
3. Exogenous flow hits the book; matching is **price-time priority**.
4. Fees/rebates apply per trade; accounting marks inventory to mid.
5. Metrics split **spread capture**, **inventory PnL**, **adverse selection**, drawdown, fill rate.

## Inventory-aware quoting

- **Symmetric MM:** quotes `fair ± half_spread` with an inventory cap.
- **Avellaneda–Stoikov:** reservation price `r = mid − q·γ·σ²·T`, optimal spread, size skew; widens on toxicity proxy.

Inventory that drifts one way turns “earned” spread into mark-to-market pain. That is intentional — it is the HF MM risk you must defend in interviews.

## Walk-forward / OOS

`runWalkForward` rolls IS → OOS folds with fresh seeds (regime proxies). Tune only on IS narratives; report **mean OOS MTM**, fills, and VaR. Do not cherry-pick a single lucky fold.

## Overnight risk

Historical VaR/ES on the equity path approximates an overnight stress snapshot. Optional Cross-Asset-Risk-Engine MC values an option book for portfolio-level stress in the research API — complementary to the sim kill switch, not a replacement for pathwise LOB realism.
