#pragma once

#include <string>

namespace quantforge::risk {

/// Documentation hook for the optional Cross-Asset-Risk-Engine integration.
///
/// The C++ simulator enforces inventory / notional / drawdown / overnight
/// historical VaR via KillSwitchGate (see risk_gate.hpp). Deeper option MC
/// overnight/stress revaluation is intentionally kept on the Python side in
/// `python/quantforge_research/risk_bridge.py`, which optionally imports
/// `_core_risk_engine` from the sibling Cross_Asset_Risk_Engine repo.
///
/// Install path (research machine)::
///   cd ../Cross_Asset_Risk_Engine
///   python -m pip install -e ".[test]"
///
/// Or set env QUANTFORGE_RISK_ENGINE_ROOT to the sibling checkout.
/// When the core module is absent, risk_bridge falls back to Black–Scholes
/// scenario stress so the research UI remains usable.
inline constexpr const char* kRiskEnginePythonModule = "_core_risk_engine";
inline constexpr const char* kRiskBridgeModule =
    "quantforge_research.risk_bridge";

inline std::string riskEngineIntegrationNote()
{
    return "C++ kill-switch + overnight VaR in-sim; optional Python "
           "_core_risk_engine overnight/stress via risk_bridge "
           "(BS fallback when not installed).";
}

} // namespace quantforge::risk
