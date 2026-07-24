import pytest

import quantforge

pytestmark = pytest.mark.skipif(
    not quantforge.available(),
    reason="quantforge._quantforge not built or MinGW runtime not on PATH",
)


def test_order_book_basics():
    book = quantforge.OrderBook()
    assert book.empty()
    n = book.add_order(
        quantforge.Order(
            1,
            quantforge.Side.Buy,
            quantforge.OrderType.Limit,
            100,
            5,
            1,
        )
    )
    assert n == 0
    assert book.best_bid() == 100


def test_run_simulation_returns_metrics():
    out = quantforge.run_simulation(
        strategy="symmetric_mm",
        seed=7,
        horizon=400,
        params={"half_spread": 5, "quote_size": 10},
    )
    assert "metrics" in out
    assert "equity_curve" in out
    assert isinstance(out["equity_curve"], list)


def test_search_params_is_only():
    out = quantforge.search_params_is_only(
        seed=5,
        horizon=400,
        strategy="symmetric_mm",
        method="grid",
        max_trials=4,
    )
    assert "best_params" in out
    assert len(out["trials"]) <= 4
