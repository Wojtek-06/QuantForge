from pathlib import Path

from quantforge_research.history import get_experiment, list_experiments, save_experiment


def test_history_roundtrip(tmp_path: Path):
    db = tmp_path / "hist.sqlite3"
    payload = {
        "experiment": {
            "experiment": "unit",
            "results": [{"strategy": "symmetric_mm", "metrics": {"mtm_pnl": 1.0}}],
        },
        "walk_forward": {"oos_mtm_mean": -2.5, "param_search_enabled": True},
    }
    meta = save_experiment("default_experiment", True, payload, db_path=db)
    assert meta["id"]
    listed = list_experiments(db_path=db)
    assert len(listed) == 1
    assert listed[0]["config"] == "default_experiment"
    loaded = get_experiment(meta["id"], db_path=db)
    assert loaded is not None
    assert loaded["payload"]["walk_forward"]["oos_mtm_mean"] == -2.5
