"""
Bridge to Cross-Asset-Risk-Engine concepts.

Priority:
1. Optional pybind module `_core_risk_engine` (from Cross-Asset-Risk-Engine)
2. Pure-Python historical VaR / ES fallback (always available)
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Sequence

import numpy as np

_CORE = None
try:  # optional sibling install
    import _core_risk_engine as _CORE  # type: ignore
except Exception:  # pragma: no cover - optional dependency
    _CORE = None


@dataclass
class VarEs:
    var: float
    expected_shortfall: float
    confidence: float
    observations: int
    source: str


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
) -> dict:
    """
    Overnight-style stress using Cross-Asset-Risk-Engine MC when available.
    Falls back to Black-Scholes analytical price from the same module, else a
    simple BS approximation in numpy.
    """
    if _CORE is not None:
        trade = _CORE.OptionTrade()
        trade.spot = spot
        trade.strike = strike
        trade.maturity = maturity
        trade.risk_free_rate = rate
        trade.volatility = vol
        trade.dividend_yield = 0.0
        trade.is_call = is_call
        trade.quantity = quantity

        analytic = float(_CORE.calculate_portfolio_value([trade]))
        mc = float(
            _CORE.calculate_portfolio_value_mc([trade], num_paths, seed)
        )
        greeks = _CORE.calculate_portfolio_greeks([trade])
        delta = float(greeks["delta"])
        return {
            "source": "cross_asset_risk_engine",
            "analytic_value": analytic,
            "mc_value": mc,
            "delta": delta,
            "available": True,
        }

    # Lightweight BS fallback (research UI must work without Risk Engine install).
    from math import erf, exp, log, sqrt

    def ncdf(x: float) -> float:
        return 0.5 * (1.0 + erf(x / sqrt(2.0)))

    if maturity <= 0 or vol <= 0 or spot <= 0 or strike <= 0:
        return {"source": "fallback_bs", "analytic_value": 0.0, "available": False}

    d1 = (log(spot / strike) + (rate + 0.5 * vol * vol) * maturity) / (
        vol * sqrt(maturity)
    )
    d2 = d1 - vol * sqrt(maturity)
    if is_call:
        price = spot * ncdf(d1) - strike * exp(-rate * maturity) * ncdf(d2)
    else:
        price = strike * exp(-rate * maturity) * ncdf(-d2) - spot * ncdf(-d1)

    return {
        "source": "fallback_bs",
        "analytic_value": float(price * quantity),
        "mc_value": None,
        "delta": float(ncdf(d1) if is_call else ncdf(d1) - 1.0) * quantity,
        "available": True,
    }


def summarize_equity_risk(equity_curve: Iterable[float]) -> dict:
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
