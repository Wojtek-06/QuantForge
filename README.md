# QuantForge

Event-driven **limit-order-book market-making & backtesting lab** in C++20.

QuantForge absorbs and extends [cpp-exchange-engine](https://github.com/Wojtek-06/cpp-exchange-engine): a price-time priority matching venue becomes the core of a reproducible microstructure simulator with inventory-aware quoting strategies, fees, latency, CSV market-data replay, look-ahead guards, and MM metrics.

> Placement pitch: *I can build and stress-test HF-style MM ideas with a realistic sim—not bar-close fantasy fills—and measure spread capture, inventory risk, and adverse selection.*

CI: build + GoogleTest (including leakage suite) on every push/PR to `main`.

---

## What you get

| Layer | Capability |
|-------|------------|
| **Venue** | C++20 LOB: market/limit/cancel, partial fills, **IOC/FOK**, maker/taker fees & rebates, **queue position** |
| **Market data** | Historical/synthetic **CSV ingest**, validation, deterministic replay |
| **As-of guards** | `AsOfClock` / `AsOfSeries` fail closed on future mid / event access |
| **Simulator** | Priority event queue, synthetic *or* CSV flow, fixed latency, risk kill-switch hook |
| **Strategies** | `no_trade` · symmetric two-sided MM · Avellaneda–Stoikov inventory-aware MM |
| **Leakage foil** | Naive bar fantasy fills vs realistic LOB fills (same tape) |
| **Signals** | Spread, microprice, OFI, toxicity proxy |
| **Accounting** | Cash/inventory ledger, MTM, drawdown, fill rate, spread capture, adverse selection |
| **Risk (stub)** | `KillSwitchGate` — inventory / notional / drawdown (Cross-Asset-Risk-Engine shaped) |
| **Reports** | Markdown + CSV comparison output (Portfolio-Analyser-style surfaces) |
| **Experiments** | Config-driven CLI (`quantforge --config … --out-dir …`) |

---

## Architecture (how files link)

```
CSV / synthetic tape
        ↓
 marketdata/csv_loader + as_of guards
        ↓
types.hpp  →  order.hpp / trade.hpp
                 ↓
            order_book.cpp     ← matching (price-time)
                 ↓
            simulator.cpp      ← event clock + latency + risk gate
                 ↓
     strategy/*.hpp  +  signals/signals.hpp
                 ↓
            accounting.cpp     ← PnL / MM metrics
                 ↓
   naive_bar_backtester.cpp    ← fantasy-fill foil (leakage tests)
                 ↓
   experiment + report         ← compare strategies, write MD/CSV
                 ↓
         apps/quantforge_demo.cpp
```

**Mental model**

1. **`engine/`** — venue primitives (`Order`, `Trade`, `OrderBook`).
2. **`marketdata/`** — CSV tape + as-of / look-ahead guards.
3. **`sim/`** — schedules exogenous / CSV orders + strategy ticks; optional naive bar foil.
4. **`strategy/`** — plugins that turn book + inventory into a `QuoteIntent`.
5. **`risk/`** — kill-switch stub (full Risk Engine pybind later).
6. **`metrics/`** — portfolio accounting and MM scorecard.
7. **`experiment/` + `report/`** — reproducible comparisons and research artefacts.

---

## Project layout

```
QuantForge/
├── include/quantforge/
│   ├── engine/          # LOB types + order book API
│   ├── marketdata/      # CSV ingest, as-of clock
│   ├── sim/             # events, latency, simulator, naive bar
│   ├── strategy/        # MM strategies
│   ├── signals/         # spread / microprice / OFI
│   ├── metrics/         # accounting + MM metrics
│   ├── risk/            # kill-switch stub
│   ├── experiment/      # comparison runner + JSON config
│   └── report/          # markdown/CSV research reports
├── src/
├── apps/
│   ├── lob_demo.cpp
│   └── quantforge_demo.cpp
├── tests/               # LOB, sim, marketdata, leakage
├── data/                # sample synthetic CSV tape
├── configs/             # experiment JSON
├── .github/workflows/   # CI
└── CMakeLists.txt
```

---

## Build

**Requirements:** CMake ≥ 3.25, C++20 compiler (MSVC, Clang, or GCC/MinGW).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

MinGW example on Windows:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Binaries

| Target | Purpose |
|--------|---------|
| `lob_demo` | Manual LOB walkthrough |
| `quantforge` | Strategy comparison (+ optional report files) |
| `quantforge_tests` | Unit + leakage tests |
| `quantforge_benchmark` | Matching throughput |

```bash
./build/quantforge
./build/quantforge --config configs/stress_mm.json --out-dir build/reports
./build/quantforge --config configs/csv_replay.json --out-dir build/reports
./build/lob_demo
```

---

## Market-data CSV

Header required:

```text
timestamp,kind,side,price,quantity
0,mid,,10000,0
10,market,buy,0,5
10,limit,sell,10015,8
```

- `kind` ∈ `mid` | `market` | `limit`
- `side` required for orders (`buy`/`sell`); empty for `mid`
- `#` comment lines allowed
- Timestamps must be non-decreasing

Replay is gated by `AsOfSeries`: requesting a mid/event after the simulation clock throws `LookAheadError`.

Sample tape: [`data/synthetic_flow.csv`](data/synthetic_flow.csv).

---

## Leakage methodology (why this matters)

Naive bar backtests often **decide and fill on the same bar close** — a classic look-ahead / fantasy-fill pattern. QuantForge keeps that path as an explicit foil (`NaiveBarBacktester`) and compares it to LOB matching on the same tape.

| Path | Decision info | Fill model |
|------|---------------|------------|
| **LOB sim** | As-of mid + book at event time | Price-time matching, fees, latency |
| **Naive bar** | Same-bar close (leaky) | Touch high/low → fill at close |

Leakage tests in CI fail if:

- future data can be read through the guarded as-of API, or
- naive bar and LOB produce identical fill/PnL outcomes (foil broken).

---

## Experiment configs

| File | Intent |
|------|--------|
| `configs/default_experiment.json` | Baseline seeded comparison |
| `configs/stress_mm.json` | Longer horizon, wider fees, risk gate on |
| `configs/csv_replay.json` | Replay `data/synthetic_flow.csv` |

Reports (when `--out-dir` is set):

- `{name}_report.md` — strategy table + leakage case study
- `{name}_results.csv` — machine-readable metrics

---

## Strategies (short)

- **`no_trade`** — never quotes; baseline.
- **`symmetric_mm`** — posts bid/ask at `fair ± half_spread` with inventory cap.
- **`avellaneda_stoikov`** — reservation price `mid − q·γ·σ²·T`, optimal spread, size skew, toxicity widen.

Gross spread ≠ profit: inventory PnL, fees, and adverse selection show up in the metrics table.

---

## Absorbs / roadmap

| Source | Role |
|--------|------|
| cpp-exchange-engine | Core LOB (ported under `quantforge::engine`) |
| Cross-Asset-Risk-Engine | Stubbed via `risk::KillSwitchGate`; pybind next |
| Portfolio-Analyser | Report surfaces (`report::ResearchReport`); UI later |

**Remaining near-term**

1. Deeper Risk Engine integration (pybind11) + overnight VaR/ES hooks.
2. Walk-forward / OOS experiment harness.
3. Research API (FastAPI) + Analyser dashboard.
4. Animated LOB replay + methodology one-pager / demo video.

---

## License / provenance

Private research portfolio project. LOB core logic ported from Wojtek-06/cpp-exchange-engine and extended for MM research.
