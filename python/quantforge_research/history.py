"""Simple SQLite experiment history for the research dashboard."""

from __future__ import annotations

import json
import sqlite3
import time
import uuid
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = REPO_ROOT / "build" / "research_history.sqlite3"


def _connect(db_path: Path | None = None) -> sqlite3.Connection:
    path = db_path or DEFAULT_DB
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(path))
    conn.row_factory = sqlite3.Row
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS experiments (
            id TEXT PRIMARY KEY,
            created_at REAL NOT NULL,
            config TEXT NOT NULL,
            walk_forward INTEGER NOT NULL,
            summary_json TEXT NOT NULL,
            payload_json TEXT NOT NULL
        )
        """
    )
    conn.commit()
    return conn


def save_experiment(
    config: str,
    walk_forward: bool,
    payload: dict[str, Any],
    db_path: Path | None = None,
) -> dict[str, Any]:
    comparison = payload.get("comparison", payload)
    experiment = comparison.get("experiment", comparison)
    results = experiment.get("results", []) if isinstance(experiment, dict) else []
    summary = {
        "experiment": experiment.get("experiment")
        if isinstance(experiment, dict)
        else None,
        "n_strategies": len(results),
        "strategies": [r.get("strategy") for r in results],
        "has_walk_forward": "walk_forward" in payload,
    }
    if "walk_forward" in payload:
        wf = payload["walk_forward"]
        summary["oos_mtm_mean"] = wf.get("oos_mtm_mean")
        summary["param_search_enabled"] = wf.get("param_search_enabled")

    row_id = uuid.uuid4().hex[:12]
    created = time.time()
    with _connect(db_path) as conn:
        conn.execute(
            """
            INSERT INTO experiments
            (id, created_at, config, walk_forward, summary_json, payload_json)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (
                row_id,
                created,
                config,
                1 if walk_forward else 0,
                json.dumps(summary),
                json.dumps(payload),
            ),
        )
        conn.commit()
    return {
        "id": row_id,
        "created_at": created,
        "config": config,
        "walk_forward": walk_forward,
        "summary": summary,
    }


def list_experiments(
    limit: int = 30,
    db_path: Path | None = None,
) -> list[dict[str, Any]]:
    with _connect(db_path) as conn:
        rows = conn.execute(
            """
            SELECT id, created_at, config, walk_forward, summary_json
            FROM experiments
            ORDER BY created_at DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()
    out: list[dict[str, Any]] = []
    for r in rows:
        out.append(
            {
                "id": r["id"],
                "created_at": r["created_at"],
                "config": r["config"],
                "walk_forward": bool(r["walk_forward"]),
                "summary": json.loads(r["summary_json"]),
            }
        )
    return out


def get_experiment(
    experiment_id: str,
    db_path: Path | None = None,
) -> dict[str, Any] | None:
    with _connect(db_path) as conn:
        row = conn.execute(
            """
            SELECT id, created_at, config, walk_forward, summary_json, payload_json
            FROM experiments WHERE id = ?
            """,
            (experiment_id,),
        ).fetchone()
    if row is None:
        return None
    return {
        "id": row["id"],
        "created_at": row["created_at"],
        "config": row["config"],
        "walk_forward": bool(row["walk_forward"]),
        "summary": json.loads(row["summary_json"]),
        "payload": json.loads(row["payload_json"]),
    }
