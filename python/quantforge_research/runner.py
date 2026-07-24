"""Invoke the QuantForge C++ CLI and load research artefacts."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]


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


def run_experiment(
    config_path: Path | None = None,
    walk_forward: bool = False,
    timeout_s: int = 120,
) -> dict[str, Any]:
    binary = find_quantforge_binary()
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
        return json.loads(json_out.read_text(encoding="utf-8"))


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
