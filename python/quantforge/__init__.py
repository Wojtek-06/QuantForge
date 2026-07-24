"""
QuantForge native Python package.

Imports the pybind11 extension `_quantforge` built with:
  cmake -S . -B build -G "MinGW Makefiles" -DQUANTFORGE_BUILD_PYTHON=ON
  cmake --build build --target _quantforge

On Windows/MinGW, ensure ``C:\\msys64\\mingw64\\bin`` is on PATH so the
extension can load ``libstdc++`` / ``libgcc`` at import time.
"""

from __future__ import annotations

__version__ = "0.4.0"

_IMPORT_ERROR: BaseException | None = None

try:
    from quantforge import _quantforge as core  # type: ignore
except Exception as exc:  # pragma: no cover - build/PATH dependent
    core = None  # type: ignore
    _IMPORT_ERROR = exc


def available() -> bool:
    return core is not None


def require() -> object:
    if core is None:
        raise ImportError(
            "Native module quantforge._quantforge is not available. "
            "Configure with -DQUANTFORGE_BUILD_PYTHON=ON, build target "
            "_quantforge, and put MinGW bin on PATH (see README). "
            f"Underlying error: {_IMPORT_ERROR!r}"
        )
    return core


def __getattr__(name: str):
    mod = require()
    if hasattr(mod, name):
        return getattr(mod, name)
    raise AttributeError(name)


__all__ = [
    "available",
    "require",
    "core",
    "__version__",
    "Order",
    "OrderBook",
    "OrderType",
    "Side",
    "SimulatorConfig",
    "TimeInForce",
    "run_simulation",
    "run_comparison",
    "run_walk_forward",
    "search_params_is_only",
    "load_config_json",
]
