# QuantForge

Event-driven **limit-order-book market-making & backtesting lab** in C++20, with a Python research façade.

QuantForge absorbs and extends [cpp-exchange-engine](https://github.com/Wojtek-06/cpp-exchange-engine): a price-time matching venue becomes the core of a reproducible microstructure simulator with inventory-aware quoting, fees, latency, CSV replay, walk-forward OOS, risk kill-switches, and MM metrics.

> Placement pitch: *I can build and stress-test HF-style MM ideas with a realistic sim—not bar-close fantasy fills—and measure spread capture, inventory risk, and adverse selection.*

---

## What you get

| Layer | Capability |
|-------|------------|
| **Venue** | C++20 LOB: market/limit/cancel, partial fills, **IOC/FOK**, maker/taker fees, **queue position** |
| **Market data** | CSV ingest + `AsOf` look-ahead guards |
| **Simulator** | Priority event queue, synthetic/CSV flow, latency, LOB replay frames |
| **Strategies** | `no_trade` · symmetric MM · Avellaneda–Stoikov |
| **Signals** | Spread, microprice, OFI, toxicity proxy |
| **Accounting** | MTM, drawdown, fill rate, spread capture, adverse selection, equity path |
| **Risk** | Kill switch (inventory / notional / drawdown) + overnight historical **VaR/ES**; Python bridge to Cross-Asset-Risk-Engine |
| **Walk-forward** | Rolling IS → OOS folds with seeded regimes |
| **Leakage foil** | Naive bar fantasy fills vs LOB fills |
| **Research UI** | FastAPI + dashboard (Portfolio-Analyser-style) + animated LOB replay |

---

## How the pieces link (study map)

```
types → order/trade → order_book
                         ↓
                   simulator (events + latency + risk)
                         ↓
              strategies + signals → accounting
                         ↓
         experiment / walk_forward → reports / JSON
                         ↓
        quantforge CLI  ·  FastAPI UI  ·  LOB replay
```

1. **`engine/`** — venue truth (what can fill).
2. **`sim/`** — when things happen (clock, latency, replay).
3. **`strategy/`** — what you quote given book + inventory.
4. **`metrics/` + `risk/`** — whether you survived economically / within limits.
5. **`experiment/`** — reproducible comparisons and OOS folds.
6. **`python/`** — research API without rewriting the hot path.

Deep dives: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) · [`docs/METHODOLOGY.md`](docs/METHODOLOGY.md) · [`docs/EVIDENCE_PACK.md`](docs/EVIDENCE_PACK.md)

---

## Build (C++)

**Requirements:** CMake ≥ 3.25, C++20 (MinGW g++ / Clang / MSVC).

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Binaries

| Target | Purpose |
|--------|---------|
| `quantforge` | Strategy comparison, walk-forward, JSON/MD/CSV out |
| `lob_demo` | Manual LOB walkthrough |
| `lob_replay_export` | JSON frames for animated replay |
| `quantforge_tests` | Unit + leakage + risk/WF tests |
| `quantforge_benchmark` | Matching throughput |

```bash
./build/quantforge --config configs/default_experiment.json --out-dir build/reports --json-out build/reports/out.json
./build/quantforge --config configs/walk_forward.json --walk-forward --out-dir build/reports
./build/lob_replay_export --strategy avellaneda_stoikov --out web/replay/sample_replay.json
```

---

## Research UI (Python)

```bash
cd python
python -m pip install -r requirements.txt
set PYTHONPATH=.
set QUANTFORGE_BIN=..\build\quantforge.exe   # if needed
uvicorn quantforge_research.app:app --reload --port 8000
```

Open:

- http://127.0.0.1:8000 — comparison / equity / walk-forward dashboard  
- http://127.0.0.1:8000/replay/ — animated LOB evidence viewer  

Optional: install [Cross-Asset-Risk-Engine](https://github.com/Wojtek-06/Cross-Asset-Risk-Engine) so `/api/risk/stress` uses pybind MC; otherwise BS fallback is used.

---

## Configs

| File | Intent |
|------|--------|
| `configs/default_experiment.json` | Baseline seeded comparison |
| `configs/stress_mm.json` | Wider fees / risk gate |
| `configs/csv_replay.json` | Replay `data/synthetic_flow.csv` |
| `configs/walk_forward.json` | IS/OOS folds + overnight VaR |

---

## Absorbs / roadmap

| Source | Role |
|--------|------|
| cpp-exchange-engine | Core LOB |
| Cross-Asset-Risk-Engine | Kill-switch + optional pybind stress |
| Portfolio-Analyser | Report / UI interaction patterns |

**Still open**

1. Deeper pybind11 of the LOB itself (today the CLI is the bridge).
2. Parameter search on IS only (walk-forward currently evaluates fixed params).
3. Richer Analyser charts / deployed demo hosting.

---

## License / provenance

Private research portfolio project. LOB core ported from Wojtek-06/cpp-exchange-engine and extended for MM research.
