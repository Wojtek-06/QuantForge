# QuantForge architecture

## Thesis

Naïve bar-close backtests invent fills. QuantForge routes every strategy decision through a **C++20 price-time limit order book**, an **event clock**, and **accounting + risk gates**, so market-making ideas are stress-tested under latency, fees, queue priority, and inventory risk.

## Layer map

```
CSV / synthetic tape
        ↓
 marketdata (csv_loader, as_of guards)
        ↓
 engine::types → order / trade → order_book
        ↓
 sim::Simulator (event queue, latency, replay frames)
        ↓
 strategy plugins + signals
        ↓
 metrics::Accounting  +  risk::KillSwitchGate (+ VaR/ES)
        ↓
 experiment / walk_forward / report / json_export
        ↓
 apps: quantforge · lob_demo · lob_replay_export
        ↓
 python FastAPI research façade + web replay viewer
```

| Directory | Responsibility |
|-----------|----------------|
| `include/quantforge/engine` | Venue primitives: orders, trades, LOB matching |
| `include/quantforge/marketdata` | CSV ingest, as-of / look-ahead guards |
| `include/quantforge/sim` | Event clock, latency, simulator, naive-bar foil, replay |
| `include/quantforge/strategy` | Strategy interface + MM implementations |
| `include/quantforge/signals` | Spread, microprice, OFI, toxicity proxies |
| `include/quantforge/metrics` | Cash/inventory ledger + MM scorecard |
| `include/quantforge/risk` | Kill switch + historical VaR/ES overnight stress |
| `include/quantforge/experiment` | Comparison runner, JSON config, walk-forward |
| `include/quantforge/report` | Markdown/CSV/JSON research artefacts |
| `python/quantforge_research` | FastAPI façade + Risk Engine bridge |
| `web/replay` | Animated LOB evidence viewer |

## Determinism

Identical `SimulatorConfig.seed` + config ⇒ identical event order, fills, and metrics. Walk-forward folds use explicit seed offsets so OOS regimes are reproducible but not identical to IS.

## Risk path

1. **In-sim (C++):** `KillSwitchGate` blocks/kills on inventory, notional, drawdown, and optional overnight historical VaR on the equity path.
2. **Research (Python):** `risk_bridge` computes VaR/ES on returned equity curves and optionally calls Cross-Asset-Risk-Engine (`_core_risk_engine`) for option portfolio MC stress.

## Absorbed repos

| Source | Role here |
|--------|-----------|
| cpp-exchange-engine | Core LOB (ported + IOC/FOK/fees/queue) |
| Cross-Asset-Risk-Engine | Kill-switch semantics + optional pybind stress |
| Portfolio-Analyser | Report / dashboard interaction patterns |
