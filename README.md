# QuantForge

Event-driven **limit-order-book market-making & backtesting lab** in C++20.

QuantForge absorbs and extends [cpp-exchange-engine](https://github.com/Wojtek-06/cpp-exchange-engine): a price-time priority matching venue becomes the core of a reproducible microstructure simulator with inventory-aware quoting strategies, fees, latency, and MM metrics.

> Placement pitch: *I can build and stress-test HF-style MM ideas with a realistic sim—not bar-close fantasy fills—and measure spread capture, inventory risk, and adverse selection.*

---

## What you get (Phase 1)

| Layer | Capability |
|-------|------------|
| **Venue** | C++20 LOB: market/limit/cancel, partial fills, **IOC/FOK**, maker/taker fees & rebates, **queue position** |
| **Simulator** | Priority event queue, synthetic flow, fixed latency model, deterministic seed replay |
| **Strategies** | `no_trade` baseline · symmetric two-sided MM · Avellaneda–Stoikov inventory-aware MM |
| **Signals** | Spread, microprice, OFI, toxicity proxy (real interfaces; deepen later) |
| **Accounting** | Cash/inventory ledger, MTM, drawdown, fill rate, spread capture, adverse selection |
| **Experiments** | Config-driven strategy comparison CLI (`quantforge`) |

Later (not in this scaffold): Risk Engine kill-switches, pybind11/FastAPI research API, Portfolio-Analyser dashboards, walk-forward/leakage CI.

---

## Architecture (how files link)

```
types.hpp  →  order.hpp / trade.hpp
                 ↓
            order_book.cpp     ← matching (price-time)
                 ↓
            simulator.cpp      ← event clock + latency
                 ↓
     strategy/*.hpp  +  signals/signals.hpp
                 ↓
            accounting.cpp     ← PnL / MM metrics
                 ↓
            experiment.cpp     ← compare strategies
                 ↓
         apps/quantforge_demo.cpp
```

**Mental model**

1. **`engine/`** — venue primitives (`Order`, `Trade`, `OrderBook`).
2. **`sim/`** — schedules exogenous orders + strategy ticks on a deterministic event heap.
3. **`strategy/`** — plugins that turn book + inventory into a `QuoteIntent`.
4. **`signals/`** — microstructure features strategies may consume.
5. **`metrics/`** — portfolio accounting and MM scorecard.
6. **`experiment/`** — runs the same seeded world under multiple strategies and prints a table.

---

## Project layout

```
QuantForge/
├── include/quantforge/
│   ├── engine/          # LOB types + order book API
│   ├── sim/             # events, latency, simulator
│   ├── strategy/        # MM strategies
│   ├── signals/         # spread / microprice / OFI
│   ├── metrics/         # accounting + MM metrics
│   └── experiment/      # comparison runner
├── src/                 # order_book, simulator, accounting, experiment
├── apps/
│   ├── lob_demo.cpp           # classic matching demo
│   └── quantforge_demo.cpp    # strategy comparison
├── tests/               # GoogleTest (LOB + sim)
├── benchmarks/          # LOB throughput
├── configs/             # experiment JSON (human-readable)
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
| `lob_demo` | Manual LOB walkthrough (resting, match, IOC/FOK, fees) |
| `quantforge` | Run no-trade / symmetric / AS comparison |
| `quantforge_tests` | Unit tests |
| `quantforge_benchmark` | Matching throughput |

```bash
./build/quantforge
./build/lob_demo
```

---

## Strategies (short)

- **`no_trade`** — never quotes; baseline for opportunity cost / adverse selection.
- **`symmetric_mm`** — posts bid/ask at `fair ± half_spread` with inventory cap.
- **`avellaneda_stoikov`** — reservation price `mid − q·γ·σ²·T`, optimal spread, size skew, toxicity widen.

Gross spread ≠ profit: inventory PnL, fees, and adverse selection show up in the metrics table.

---

## Absorbs / roadmap

| Source | Role |
|--------|------|
| cpp-exchange-engine | Core LOB (ported under `quantforge::engine`) |
| Cross-Asset-Risk-Engine | Next: inventory/notional/drawdown kill-switches |
| Portfolio-Analyser | Next: research UI / walk-forward reports |

**Near-term next steps**

1. Historical CSV ingest + validation (no look-ahead).
2. Leakage tests (bar-close fantasy vs LOB fills case study).
3. Risk Engine hooks via pybind11.
4. CI workflow + benchmark regression gates.

---

## License / provenance

Private research portfolio project. LOB core logic ported from Wojtek-06/cpp-exchange-engine and extended for MM research.
