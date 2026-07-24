from quantforge_research.risk_bridge import historical_var_es, option_portfolio_stress, summarize_equity_risk


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
