"""Invoke QuantForge via CLI (full reports) or native pybind when available."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]


def native_available() -> bool:
    try:
        import quantforge

        return bool(quantforge.available())
    except Exception:
        return False


def find_quantforge_binary() -> Path:
    env = os.environ.get("QUANTFORGE_BIN")
    if env:
        path = Path(env)
        if path.exists():
            return path

    candidates = [
        REPO_ROOT / "build" / "quantforge.exe",
        REPO_ROOT / "build" / "quantforge",
        REPO_ROOT / "build" / "Release" / "quantforge.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        "quantforge binary not found. Build the C++ project or set QUANTFORGE_BIN."
    )


def _run_via_native(config_path: Path | None, walk_forward: bool) -> dict[str, Any]:
    import quantforge

    seed = 42
    horizon = 2000
    strategies = ["no_trade", "symmetric_mm", "avellaneda_stoikov"]
    params: dict[str, Any] = {}
    wf_strategy = "symmetric_mm"
    param_search = False
    is_horizon = 800
    oos_horizon = 400
    step = 400
    max_folds = 3

    if config_path is not None:
        text = config_path.read_text(encoding="utf-8")
        cfg = quantforge.load_config_json(text)
        seed = int(cfg.get("seed", seed))
        horizon = int(cfg.get("horizon", horizon))
        strategies = list(cfg.get("strategies") or strategies)
        params = dict(cfg.get("default_params") or {})
        wf_strategy = str(cfg.get("wf_strategy") or wf_strategy)
        param_search = bool(cfg.get("wf_param_search"))

    comparison = quantforge.run_comparison(
        seed=seed,
        horizon=horizon,
        strategies=strategies,
        params=params,
        enable_risk_gate=False,
    )
    payload: dict[str, Any] = {
        "notes": "Native pybind path (no CLI subprocess).",
        "experiment": comparison,
        "leakage": [],
        "backend": "native",
    }
    if walk_forward:
        wf = quantforge.run_walk_forward(
            seed=seed,
            is_horizon=is_horizon,
            oos_horizon=oos_horizon,
            step=step,
            max_folds=max_folds,
            strategy=wf_strategy,
            param_search=param_search,
            search_method="grid",
            max_trials=8,
            params=params,
        )
        return {"comparison": payload, "walk_forward": wf, "backend": "native"}
    return payload


def run_experiment(
    config_path: Path | None = None,
    walk_forward: bool = False,
    timeout_s: int = 180,
    prefer_native: bool | None = None,
) -> dict[str, Any]:
    """
    Default: CLI for full leakage/report JSON.
    Set QUANTFORGE_PREFER_NATIVE=1 to force the pybind hot path.
    Falls back to native when the CLI binary is missing.
    """
    force_native = prefer_native
    if force_native is None:
        force_native = os.environ.get("QUANTFORGE_PREFER_NATIVE", "0") == "1"

    if force_native and native_available():
        return _run_via_native(config_path, walk_forward)

    try:
        binary = find_quantforge_binary()
    except FileNotFoundError:
        if native_available():
            return _run_via_native(config_path, walk_forward)
        raise

    with tempfile.TemporaryDirectory(prefix="qf_api_") as tmp:
        tmp_path = Path(tmp)
        json_out = tmp_path / "result.json"
        out_dir = tmp_path / "reports"
        cmd = [str(binary), "--json-out", str(json_out), "--out-dir", str(out_dir)]
        if config_path is not None:
            cmd.extend(["--config", str(config_path)])
        if walk_forward:
            cmd.append("--walk-forward")

        proc = subprocess.run(
            cmd,
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=timeout_s,
            check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"quantforge failed ({proc.returncode}):\n{proc.stderr}\n{proc.stdout}"
            )
        if not json_out.exists():
            raise RuntimeError("quantforge did not write json-out")
        payload = json.loads(json_out.read_text(encoding="utf-8"))
        payload["backend"] = "cli"
        return payload


def list_configs() -> list[dict[str, str]]:
    cfg_dir = REPO_ROOT / "configs"
    items = []
    for path in sorted(cfg_dir.glob("*.json")):
        items.append({"name": path.stem, "path": str(path.relative_to(REPO_ROOT))})
    return items


def resolve_config(name_or_path: str) -> Path:
    candidate = Path(name_or_path)
    if candidate.exists():
        return candidate.resolve()
    rel = REPO_ROOT / name_or_path
    if rel.exists():
        return rel.resolve()
    cfg = REPO_ROOT / "configs" / f"{name_or_path}.json"
    if cfg.exists():
        return cfg.resolve()
    raise FileNotFoundError(f"config not found: {name_or_path}")
