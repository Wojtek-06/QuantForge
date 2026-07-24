import pytest

from quantforge_research.risk_bridge import (
    core_available,
    historical_var_es,
    option_portfolio_stress,
    overnight_scenario_stress,
    risk_engine_status,
    summarize_equity_risk,
)


def test_historical_var_es_positive_on_drawdowns():
    equity = [0, 5, 6, -30, -28, -10, 0, 2, 3, 4, 5]
    res = historical_var_es(equity, 0.95)
    assert res.observations >= 5
    assert res.var > 0
    assert res.expected_shortfall >= res.var - 1e-9


def test_option_stress_fallback_works():
    out = option_portfolio_stress(spot=100, strike=100, vol=0.2)
    assert out["available"] is True
    assert out["analytic_value"] > 0


def test_summarize_equity_risk():
    summary = summarize_equity_risk([0, 1, 2, -5, -4, 0, 1])
    assert "var_95" in summary
    assert "max_drawdown" in summary


def test_overnight_scenario_stress_fallback():
    out = overnight_scenario_stress(spot=100, strike=100, vol=0.25)
    assert out["n_scenarios"] > 0
    assert "worst_pnl" in out
    assert "stress_var_95" in out
    assert out["fallback"] is (not core_available())


def test_risk_engine_status_shape():
    status = risk_engine_status()
    assert "core_risk_engine_loaded" in status
    assert "hint" in status


@pytest.mark.skipif(not core_available(), reason="_core_risk_engine not installed")
def test_option_stress_uses_core_when_installed():
    out = option_portfolio_stress(spot=100, strike=100, vol=0.2, num_paths=2000)
    assert out["source"] == "cross_asset_risk_engine"
    assert out["mc_value"] is not None
    assert out["fallback"] is False
