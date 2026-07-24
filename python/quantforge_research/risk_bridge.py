"""
Bridge to Cross-Asset-Risk-Engine concepts.

Priority:
1. Optional pybind module `_core_risk_engine` (from Cross-Asset-Risk-Engine)
2. Pure-Python historical VaR / ES + Black–Scholes fallback (always available)

Sibling install (Windows / MinGW research machine)::

    cd ..\\Cross_Asset_Risk_Engine
    python -m pip install -e ".[test]"

Or set ``QUANTFORGE_RISK_ENGINE_ROOT`` to the sibling checkout before importing
this module (adds it to ``sys.path`` for editable/dev layouts).
"""

from __future__ import annotations

import os
import sys
from dataclasses import dataclass
from math import erf, exp, log, sqrt
from pathlib import Path
from typing import Any, Iterable, Sequence

import numpy as np

_CORE = None
_CORE_ERROR: str | None = None


def _try_import_core() -> None:
    global _CORE, _CORE_ERROR
    if _CORE is not None or _CORE_ERROR == "missing":
        return

    root = os.environ.get("QUANTFORGE_RISK_ENGINE_ROOT")
    if root:
        root_path = Path(root)
        for candidate in (root_path, root_path / "python", root_path / "src"):
            if candidate.is_dir() and str(candidate) not in sys.path:
                sys.path.insert(0, str(candidate))

    # Default sibling layout: Quant/Cross_Asset_Risk_Engine next to QuantForge.
    here = Path(__file__).resolve()
    sibling = here.parents[3] / "Cross_Asset_Risk_Engine"
    if sibling.is_dir() and str(sibling) not in sys.path:
        sys.path.insert(0, str(sibling))

    try:
        import _core_risk_engine as core  # type: ignore

        _CORE = core
        _CORE_ERROR = None
    except Exception as exc:  # pragma: no cover - optional dependency
        _CORE = None
        _CORE_ERROR = str(exc)


_try_import_core()


@dataclass
class VarEs:
    var: float
    expected_shortfall: float
    confidence: float
    observations: int
    source: str


def core_available() -> bool:
    return _CORE is not None


def risk_engine_status() -> dict[str, Any]:
    return {
        "core_risk_engine_loaded": _CORE is not None,
        "import_error": _CORE_ERROR,
        "hint": (
            "pip install -e ../Cross_Asset_Risk_Engine "
            "or set QUANTFORGE_RISK_ENGINE_ROOT"
        ),
    }


def historical_var_es(
    equity_curve: Sequence[float],
    confidence: float = 0.95,
) -> VarEs:
    arr = np.asarray(equity_curve, dtype=float)
    if arr.size < 6:
        return VarEs(0.0, 0.0, confidence, max(0, int(arr.size) - 1), "insufficient")

    pnl = np.diff(arr)
    pnl_sorted = np.sort(pnl)
    alpha = 1.0 - confidence
    idx = int(np.floor(alpha * pnl_sorted.size))
    idx = min(max(idx, 0), pnl_sorted.size - 1)
    var = float(max(0.0, -pnl_sorted[idx]))
    tail = pnl_sorted[: idx + 1]
    es = float(max(0.0, -tail.mean())) if tail.size else 0.0
    return VarEs(var, es, confidence, int(pnl.size), "historical_python")


def _bs_price(
    spot: float,
    strike: float,
    maturity: float,
    rate: float,
    vol: float,
    is_call: bool,
    quantity: float,
) -> dict[str, float]:
    if maturity <= 0 or vol <= 0 or spot <= 0 or strike <= 0:
        return {"analytic_value": 0.0, "delta": 0.0}

    def ncdf(x: float) -> float:
        return 0.5 * (1.0 + erf(x / sqrt(2.0)))

    d1 = (log(spot / strike) + (rate + 0.5 * vol * vol) * maturity) / (
        vol * sqrt(maturity)
    )
    d2 = d1 - vol * sqrt(maturity)
    if is_call:
        price = spot * ncdf(d1) - strike * exp(-rate * maturity) * ncdf(d2)
        delta = ncdf(d1)
    else:
        price = strike * exp(-rate * maturity) * ncdf(-d2) - spot * ncdf(-d1)
        delta = ncdf(d1) - 1.0
    return {
        "analytic_value": float(price * quantity),
        "delta": float(delta * quantity),
    }


def _make_trade(
    spot: float,
    strike: float,
    maturity: float,
    rate: float,
    vol: float,
    quantity: float,
    is_call: bool,
) -> Any:
    assert _CORE is not None
    trade = _CORE.OptionTrade()
    trade.spot = spot
    trade.strike = strike
    trade.maturity = maturity
    trade.risk_free_rate = rate
    trade.volatility = vol
    trade.dividend_yield = 0.0
    trade.is_call = is_call
    trade.quantity = quantity
    return trade


def option_portfolio_stress(
    spot: float,
    strike: float,
    maturity: float = 0.25,
    rate: float = 0.03,
    vol: float = 0.25,
    quantity: float = 1.0,
    is_call: bool = True,
    num_paths: int = 20_000,
    seed: int = 42,
) -> dict[str, Any]:
    """
    Overnight-style stress using Cross-Asset-Risk-Engine MC when available.
    Falls back to Black-Scholes analytical price when the core is missing.
    """
    if _CORE is not None:
        trade = _make_trade(spot, strike, maturity, rate, vol, quantity, is_call)
        analytic = float(_CORE.calculate_portfolio_value([trade]))
        mc = float(_CORE.calculate_portfolio_value_mc([trade], num_paths, seed))
        greeks = _CORE.calculate_portfolio_greeks([trade])
        return {
            "source": "cross_asset_risk_engine",
            "analytic_value": analytic,
            "mc_value": mc,
            "delta": float(greeks["delta"]),
            "gamma": float(greeks.get("gamma", 0.0)),
            "vega_1bp": float(greeks.get("vega_1bp", 0.0)),
            "theta": float(greeks.get("theta", 0.0)),
            "available": True,
            "fallback": False,
        }

    fb = _bs_price(spot, strike, maturity, rate, vol, is_call, quantity)
    return {
        "source": "fallback_bs",
        "analytic_value": fb["analytic_value"],
        "mc_value": None,
        "delta": fb["delta"],
        "gamma": None,
        "vega_1bp": None,
        "theta": None,
        "available": True,
        "fallback": True,
    }


def overnight_scenario_stress(
    spot: float,
    strike: float,
    maturity: float = 0.25,
    rate: float = 0.03,
    vol: float = 0.25,
    quantity: float = 1.0,
    is_call: bool = True,
    spot_shocks: Sequence[float] | None = None,
    vol_shocks: Sequence[float] | None = None,
    num_paths: int = 5_000,
    seed: int = 42,
) -> dict[str, Any]:
    """
    Multi-scenario overnight stress: revalue an option book under spot/vol
    shocks. Uses Risk Engine MC when installed; otherwise BS fallback.
    """
    spot_shocks = list(spot_shocks or [-0.10, -0.05, 0.0, 0.05, 0.10])
    vol_shocks = list(vol_shocks or [-0.05, 0.0, 0.05, 0.10])

    base = option_portfolio_stress(
        spot=spot,
        strike=strike,
        maturity=maturity,
        rate=rate,
        vol=vol,
        quantity=quantity,
        is_call=is_call,
        num_paths=num_paths,
        seed=seed,
    )
    base_value = float(base["analytic_value"])

    scenarios: list[dict[str, Any]] = []
    pnl_values: list[float] = []

    for ds in spot_shocks:
        for dv in vol_shocks:
            shocked_spot = spot * (1.0 + ds)
            shocked_vol = max(1e-6, vol + dv)
            if _CORE is not None:
                trade = _make_trade(
                    shocked_spot,
                    strike,
                    maturity,
                    rate,
                    shocked_vol,
                    quantity,
                    is_call,
                )
                value = float(_CORE.calculate_portfolio_value([trade]))
                source = "cross_asset_risk_engine"
            else:
                value = _bs_price(
                    shocked_spot,
                    strike,
                    maturity,
                    rate,
                    shocked_vol,
                    is_call,
                    quantity,
                )["analytic_value"]
                source = "fallback_bs"
            pnl = value - base_value
            pnl_values.append(pnl)
            scenarios.append(
                {
                    "spot_shock": ds,
                    "vol_shock": dv,
                    "spot": shocked_spot,
                    "vol": shocked_vol,
                    "value": value,
                    "pnl": pnl,
                    "source": source,
                }
            )

    arr = np.asarray(pnl_values, dtype=float)
    worst = float(arr.min()) if arr.size else 0.0
    best = float(arr.max()) if arr.size else 0.0
    # Treat scenario PnL distribution as a discrete stress VaR proxy (5%).
    stress_var = float(max(0.0, -np.quantile(arr, 0.05))) if arr.size else 0.0

    return {
        "base": base,
        "scenarios": scenarios,
        "worst_pnl": worst,
        "best_pnl": best,
        "stress_var_95": stress_var,
        "n_scenarios": len(scenarios),
        "core_risk_engine_loaded": _CORE is not None,
        "fallback": _CORE is None,
    }


def summarize_equity_risk(equity_curve: Iterable[float]) -> dict[str, Any]:
    curve = list(equity_curve)
    var_es = historical_var_es(curve)
    peak = 0.0
    max_dd = 0.0
    for x in curve:
        peak = max(peak, x)
        max_dd = max(max_dd, peak - x)
    return {
        "var_95": var_es.var,
        "es_95": var_es.expected_shortfall,
        "max_drawdown": max_dd,
        "observations": var_es.observations,
        "source": var_es.source,
        "core_risk_engine_loaded": _CORE is not None,
    }


def drawdown_series(equity_curve: Sequence[float]) -> list[float]:
    peak = float("-inf")
    out: list[float] = []
    for x in equity_curve:
        peak = max(peak, float(x))
        out.append(peak - float(x))
    return out
