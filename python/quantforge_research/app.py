"""
FastAPI research façade for QuantForge.

Inspired by Portfolio-Analyser report surfaces: comparison tables, equity /
drawdown views, and risk summaries — served as a local research UI.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from quantforge_research.risk_bridge import option_portfolio_stress, summarize_equity_risk
from quantforge_research.runner import list_configs, resolve_config, run_experiment

REPO_ROOT = Path(__file__).resolve().parents[2]
STATIC_DIR = REPO_ROOT / "python" / "static"
REPLAY_DIR = REPO_ROOT / "web" / "replay"

app = FastAPI(
    title="QuantForge Research API",
    version="0.3.0",
    description="Event-driven MM lab research façade",
)


class ExperimentRequest(BaseModel):
    config: str = Field(
        default="default_experiment",
        description="Config stem under configs/ or relative path",
    )
    walk_forward: bool = False


class StressRequest(BaseModel):
    spot: float = 100.0
    strike: float = 100.0
    maturity: float = 0.25
    vol: float = 0.25
    quantity: float = 1.0
    is_call: bool = True


@app.get("/api/health")
def health() -> dict[str, Any]:
    return {"ok": True, "repo": str(REPO_ROOT)}


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

    # Attach Analyser-style risk summary on first strategy equity curve if present.
    comparison = payload.get("comparison", payload)
    experiment = comparison.get("experiment", comparison)
    results = experiment.get("results", [])
    for row in results:
        curve = row.get("equity_curve") or []
        row["risk_summary"] = summarize_equity_risk(curve)
    return payload


@app.post("/api/risk/stress")
def risk_stress(body: StressRequest) -> dict[str, Any]:
    return option_portfolio_stress(
        spot=body.spot,
        strike=body.strike,
        maturity=body.maturity,
        vol=body.vol,
        quantity=body.quantity,
        is_call=body.is_call,
    )


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
