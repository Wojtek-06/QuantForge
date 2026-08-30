# QuantForge

[![CI](https://github.com/Wojtek-06/QuantForge/actions/workflows/ci.yml/badge.svg)](https://github.com/Wojtek-06/QuantForge/actions/workflows/ci.yml)

Event-driven **limit-order-book market-making & backtesting lab** in C++20, with a pybind11 research module and FastAPI dashboard.

QuantForge absorbs and extends [cpp-exchange-engine](https://github.com/Wojtek-06/cpp-exchange-engine): a price-time matching venue becomes the core of a reproducible microstructure simulator with inventory-aware quoting, fees, latency, **cancel/fill races**, CSV replay, walk-forward OOS, **IS-only parameter search**, risk kill-switches, and MM metrics.

> Placement pitch: *I can build and stress-test HF-style MM ideas with a realistic sim—not bar-close fantasy fills—and measure spread capture, inventory risk, and adverse selection.*

**Leakage catch (CI):** `tests/test_leakage.cpp` fails a naïve bar-close backtest that invents fills against the same strategy on the LOB—CI keeps that regression red-flagged so “gross spread ≠ profit” stays honest.

---

## What you get

| Layer | Capability |
|-------|------------|
| **Venue** | C++20 LOB: market/limit/**stop**/cancel, partial fills, **IOC/FOK**, maker/taker fees, **queue position** |
| **Market data** | CSV ingest + `AsOf` look-ahead guards |
| **Simulator** | Priority event queue, synthetic/CSV flow, latency, cancel races, LOB replay frames |
| **Strategies** | `no_trade` · symmetric MM · Avellaneda–Stoikov (tunable params) |
| **Signals** | Spread, microprice, OFI, blended toxicity, EWMA realized vol |
| **Accounting** | MTM, drawdown, fill rate, spread capture, adverse selection, equity path |
| **Risk** | Kill switch + overnight historical **VaR/ES**; Python overnight/stress via Cross-Asset-Risk-Engine (BS fallback) |
| **Walk-forward** | Rolling IS → OOS folds; optional **IS-only grid/random param search**, freeze winner for OOS |
| **Leakage foil** | Naive bar fantasy fills vs LOB fills |
| **Python** | pybind11 hot path (`quantforge`) + FastAPI Analyser-style UI + SQLite history |

---

## How the pieces link

```
types → order/trade → order_book
                         ↓
                   simulator (events + latency + risk)
                         ↓
              strategies + params → accounting
                         ↓
     experiment / param_search / walk_forward → reports / JSON
                         ↓
   quantforge CLI  ·  pybind module  ·  FastAPI UI  ·  LOB replay
```

Deep dives: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) · [`docs/METHODOLOGY.md`](docs/METHODOLOGY.md) · [`docs/EVIDENCE_PACK.md`](docs/EVIDENCE_PACK.md)

---

## Build (C++)

**Requirements:** CMake ≥ 3.25, C++20. On Windows, put MinGW on `PATH` (`C:\msys64\mingw64\bin`).

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
| `quantforge_tests` | Unit + leakage + risk/WF + IS search tests |
| `quantforge_benchmark` | Matching throughput |

```bash
./build/quantforge --config configs/default_experiment.json --out-dir build/reports --json-out build/reports/out.json
./build/quantforge --config configs/walk_forward_search.json --walk-forward --out-dir build/reports
./build/lob_replay_export --strategy avellaneda_stoikov --out web/replay/sample_replay.json
```

---

## Python bindings (pybind11)

Additive to the CLI. Configure with `QUANTFORGE_BUILD_PYTHON=ON` (needs Python **development** headers + a compiler on `PATH`).

```bash
# Windows PowerShell (MinGW)
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DQUANTFORGE_BUILD_PYTHON=ON
cmake --build build --target _quantforge
```

The extension lands in `python/quantforge/` (`_quantforge*.pyd` / `.so`).

```bash
cd python
python -m pip install -r requirements.txt
$env:PYTHONPATH = "."
python -c "import quantforge; print(quantforge.run_simulation(horizon=500))"
```

**API surface (minimum):**

| Function / type | Role |
|-----------------|------|
| `OrderBook`, `Order`, `Side`, … | LOB basics |
| `SimulatorConfig` | Hot-path sim knobs |
| `run_simulation` / `run_comparison` | Single run + multi-strategy |
| `run_walk_forward` | IS/OOS folds (+ optional IS param search) |
| `search_params_is_only` | Grid/random search on IS only |
| Metrics / equity / risk flags | Returned as dicts |

**pip notes (Windows):** build the extension with CMake/MinGW as above rather than a pure `pip install` of the C++ core. Keep `PYTHONPATH=python`. If CMake cannot find Python, pass `-DPython_EXECUTABLE=...` explicitly. The MinGW build copies `libgcc` / `libstdc++` / `libwinpthread` next to `_quantforge*.pyd` (gitignored) so import works without putting `mingw64\\bin` on `PATH`.

---

## Research UI (Python)

```bash
cd python
python -m pip install -r requirements.txt
$env:PYTHONPATH = "."
$env:QUANTFORGE_BIN = "..\build\quantforge.exe"   # CLI path (default backend)
uvicorn quantforge_research.app:app --reload --port 8000
```

Open http://127.0.0.1:8000 — comparison, equity/drawdown, walk-forward chart, leakage panel, overnight stress grid, experiment history (SQLite under `build/`).

Optional: set `QUANTFORGE_PREFER_NATIVE=1` to force the pybind hot path instead of the CLI subprocess (when `_quantforge` is built).

### Optional Cross-Asset-Risk-Engine

```bash
cd ..\Cross_Asset_Risk_Engine
python -m pip install -e ".[test]"
```

Or set `QUANTFORGE_RISK_ENGINE_ROOT` to that checkout. `/api/risk/stress` then uses pybind MC + Greeks; otherwise a BS scenario-grid fallback is used (tests always pass with fallback).

---

## Configs

| File | Intent |
|------|--------|
| `configs/default_experiment.json` | Baseline seeded comparison |
| `configs/stress_mm.json` | Wider fees / risk gate |
| `configs/stress_cancel_race.json` | `cancel_latency` > 0 fill race |
| `configs/stress_vol_spike.json` | Jump / vol + overnight VaR |
| `configs/stress_stale_quotes.json` | Slow cancel + inventory stress |
| `configs/stress_toxic_flow.json` | Aggressive flow / adverse selection |
| `configs/csv_replay.json` | Replay `data/synthetic_flow.csv` |
| `configs/walk_forward.json` | IS/OOS folds + overnight VaR |
| `configs/walk_forward_search.json` | Walk-forward **with IS-only param search** |

Evidence one-shot (after build): `scripts/generate_evidence.sh` / `.ps1` → `build/reports/` + replay JSON. Docker: `docker compose up --build`.

### IS-only search keys

`wf_param_search`, `wf_search_method` (`grid`|`random`), `wf_search_max_trials`, `wf_search_seed`, `mm_half_spreads`, `mm_quote_sizes`, `as_gammas`, `as_sigmas`, `as_horizons_T`, plus fixed defaults `mm_half_spread`, `as_gamma`, …

Selection scores candidates on each **IS** fold only, freezes the winner, then evaluates **OOS** with those frozen params.

---

## Absorbs / provenance

| Source | Role |
|--------|------|
| cpp-exchange-engine | Core LOB |
| Cross-Asset-Risk-Engine | Kill-switch concepts + optional pybind stress |
| Portfolio-Analyser | Report / UI interaction patterns |

Portfolio / placement project (not a live trading system). LOB core ported from
[cpp-exchange-engine](https://github.com/Wojtek-06/cpp-exchange-engine) and extended for MM research.

**License:** MIT · **Repo:** https://github.com/Wojtek-06/QuantForge
