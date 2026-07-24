"""
FastAPI research façade for QuantForge.

Portfolio-Analyser-style surfaces: experiment history, comparison tables,
equity / drawdown charts, walk-forward folds, leakage panel, risk stress.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from quantforge_research.history import get_experiment, list_experiments, save_experiment
from quantforge_research.risk_bridge import (
    drawdown_series,
    option_portfolio_stress,
    overnight_scenario_stress,
    risk_engine_status,
    summarize_equity_risk,
)
from quantforge_research.runner import (
    list_configs,
    native_available,
    resolve_config,
    run_experiment,
)

REPO_ROOT = Path(__file__).resolve().parents[2]
STATIC_DIR = REPO_ROOT / "python" / "static"
REPLAY_DIR = REPO_ROOT / "web" / "replay"

app = FastAPI(
    title="QuantForge Research API",
    version="0.4.0",
    description="Event-driven MM lab research façade",
)


class ExperimentRequest(BaseModel):
    config: str = Field(
        default="default_experiment",
        description="Config stem under configs/ or relative path",
    )
    walk_forward: bool = False
    save: bool = True


class StressRequest(BaseModel):
    spot: float = 100.0
    strike: float = 100.0
    maturity: float = 0.25
    vol: float = 0.25
    quantity: float = 1.0
    is_call: bool = True
    overnight: bool = True


def _enrich_payload(payload: dict[str, Any]) -> dict[str, Any]:
    comparison = payload.get("comparison", payload)
    experiment = comparison.get("experiment", comparison)
    results = experiment.get("results", [])
    for row in results:
        curve = row.get("equity_curve") or []
        row["risk_summary"] = summarize_equity_risk(curve)
        row["drawdown_curve"] = drawdown_series(curve)
    return payload


@app.get("/api/health")
def health() -> dict[str, Any]:
    return {
        "ok": True,
        "repo": str(REPO_ROOT),
        "native_module": native_available(),
        "risk_engine": risk_engine_status(),
    }


@app.get("/api/configs")
def configs() -> list[dict[str, str]]:
    return list_configs()


@app.post("/api/experiments/run")
def experiments_run(body: ExperimentRequest) -> dict[str, Any]:
    try:
        cfg = resolve_config(body.config)
        payload = run_experiment(cfg, walk_forward=body.walk_forward)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001 - surface to research UI
        raise HTTPException(status_code=500, detail=str(exc)) from exc

    payload = _enrich_payload(payload)
    meta = None
    if body.save:
        meta = save_experiment(body.config, body.walk_forward, payload)
        payload["history_id"] = meta["id"]
    return payload


@app.get("/api/experiments/history")
def experiments_history(limit: int = 30) -> list[dict[str, Any]]:
    return list_experiments(limit=limit)


@app.get("/api/experiments/history/{experiment_id}")
def experiments_history_item(experiment_id: str) -> dict[str, Any]:
    row = get_experiment(experiment_id)
    if row is None:
        raise HTTPException(status_code=404, detail="experiment not found")
    return row


@app.post("/api/risk/stress")
def risk_stress(body: StressRequest) -> dict[str, Any]:
    if body.overnight:
        return overnight_scenario_stress(
            spot=body.spot,
            strike=body.strike,
            maturity=body.maturity,
            vol=body.vol,
            quantity=body.quantity,
            is_call=body.is_call,
        )
    return option_portfolio_stress(
        spot=body.spot,
        strike=body.strike,
        maturity=body.maturity,
        vol=body.vol,
        quantity=body.quantity,
        is_call=body.is_call,
    )


@app.get("/api/risk/status")
def risk_status() -> dict[str, Any]:
    return risk_engine_status()


@app.get("/api/replay/sample")
def replay_sample() -> dict[str, Any]:
    path = REPLAY_DIR / "sample_replay.json"
    if not path.exists():
        raise HTTPException(
            status_code=404,
            detail="sample_replay.json missing — run lob_replay_export",
        )
    return json.loads(path.read_text(encoding="utf-8"))


@app.get("/")
def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


if STATIC_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")
if REPLAY_DIR.exists():
    app.mount("/replay", StaticFiles(directory=str(REPLAY_DIR)), name="replay")
