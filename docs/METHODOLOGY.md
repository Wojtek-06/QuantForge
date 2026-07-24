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

`runWalkForward` rolls IS → OOS folds with fresh seeds (regime proxies). Report **mean OOS MTM**, fills, and VaR. Do not cherry-pick a single lucky fold.

### IS-only parameter search

When `wf_param_search` is enabled, each fold:

1. Enumerates a grid (or random sample) of strategy params on the **IS** window only.
2. Freezes the IS winner (`half_spread` / `quote_size` for symmetric MM; `gamma` / `sigma` / `T` for Avellaneda–Stoikov).
3. Evaluates the **OOS** window with those frozen params.

OOS never enters the selection objective. Tests in `tests/test_param_search.cpp` assert trial scores match IS simulations and that walk-forward freezes the IS winner.

## Overnight risk

Historical VaR/ES on the equity path approximates an overnight stress snapshot inside the C++ kill switch. The Python `risk_bridge` adds an overnight **scenario grid** (spot/vol shocks) using Cross-Asset-Risk-Engine MC/Greeks when `_core_risk_engine` is installed, otherwise a Black–Scholes fallback — complementary to the sim kill switch, not a replacement for pathwise LOB realism.
