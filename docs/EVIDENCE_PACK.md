# Evidence pack checklist

Use this pack for placement demos / README media.

## Artefacts to generate

```bash
# C++ comparison + walk-forward JSON
./build/quantforge --config configs/walk_forward.json --out-dir build/reports --json-out build/reports/walk_forward.json

# Animated LOB frames
./build/lob_replay_export --strategy avellaneda_stoikov --out web/replay/sample_replay.json

# Research UI (Portfolio-Analyser-style)
cd python
python -m pip install -r requirements.txt
$env:PYTHONPATH="."
uvicorn quantforge_research.app:app --reload --port 8000
# open http://127.0.0.1:8000  and  http://127.0.0.1:8000/replay/
```

## Demo script (~5 min)

1. **Architecture slide** — show `docs/ARCHITECTURE.md` layer diagram.
2. **LOB replay** — play `web/replay` and narrate quote skew vs inventory.
3. **Strategy comparison** — run default config in the research UI; highlight no-trade vs symmetric vs AS.
4. **Leakage foil** — point at LOB vs naive-bar MTM divergence (`test_leakage` / CI badge).
5. **Walk-forward** — enable checkbox / `walk_forward.json`; stress that OOS mean matters; IS-only param search freezes winner.
6. **Cancel race** — `configs/stress_cancel_race.json` (`cancel_latency` > 0) vs immediate cancel; stale quotes can still fill.
7. **Risk kill / vol stress** — `stress_mm.json` / `stress_vol_spike.json`; show `risk_killed` + VaR columns.
8. **CI** — green badge / local `ctest` 50+ tests.

## Failure we caught

Naïve bar fantasy fills reported positive MTM while the LOB path lost money under the same seed—fixed as an automated leakage test so the bar path cannot silently replace venue realism.

## Screen-record tip

Record the replay viewer + research UI side-by-side. Narrate: *“fills come from the matching engine, not bar closes.”*

## One-pager claims (defendable)

- Price-time LOB with IOC/FOK, fees, queue position.
- Event-driven latency model + deterministic seeds.
- Inventory-aware AS quoting + adverse-selection metric.
- Automated look-ahead guards + naive-bar foil tests.
- Walk-forward OOS harness + overnight VaR kill switch.
