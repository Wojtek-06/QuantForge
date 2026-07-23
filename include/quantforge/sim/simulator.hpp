#pragma once

#include "quantforge/engine/order_book.hpp"
#include "quantforge/marketdata/as_of.hpp"
#include "quantforge/marketdata/types.hpp"
#include "quantforge/metrics/accounting.hpp"
#include "quantforge/risk/risk_gate.hpp"
#include "quantforge/sim/event.hpp"
#include "quantforge/sim/latency_model.hpp"
#include "quantforge/strategy/strategy.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace quantforge::sim {

enum class FlowMode {
    Synthetic,   ///< RNG exogenous flow (default)
    MarketData   ///< Replay validated CSV / series with as-of guards
};

struct SimulatorConfig {
    std::uint64_t seed{42};
    engine::Timestamp horizon{10'000};
    engine::Timestamp tick_interval{10};
    engine::Quantity exogenous_qty{5};
    engine::Price initial_mid{10'000};
    engine::Price tick_size{1};
    double jump_prob{0.02};
    engine::ParticipantId strategy_participant{1};
    engine::ParticipantId noise_participant{2};
    LatencyModel latency{};
    engine::FeeSchedule fees{};
    FlowMode flow_mode{FlowMode::Synthetic};
    /// Optional replay series (required when flow_mode == MarketData).
    std::optional<marketdata::MarketEventSeries> market_data{};
    /// Optional Cross-Asset-Risk-Engine-style kill switch.
    risk::RiskLimits risk_limits{};
    bool enable_risk_gate{false};
};

struct SimulationResult {
    metrics::PortfolioSnapshot final_snapshot{};
    metrics::MmMetrics metrics{};
    std::string strategy_name;
    std::uint64_t events_processed{0};
    std::uint64_t trades{0};
    bool risk_killed{false};
    std::string risk_reason;
};

class Simulator {
public:
    explicit Simulator(SimulatorConfig config);

    void setStrategy(std::unique_ptr<strategy::IStrategy> strategy);

    SimulationResult run();

private:
    SimulatorConfig config_;
    engine::OrderBook book_;
    metrics::Accounting accounting_;
    LatencyModel latency_;
    std::unique_ptr<strategy::IStrategy> strategy_;
    std::unique_ptr<risk::KillSwitchGate> risk_gate_;
    marketdata::AsOfSeries as_of_series_;

    std::priority_queue<Event, std::vector<Event>, EventCompare> queue_;
    std::uint64_t next_sequence_{0};
    engine::OrderId next_order_id_{1};
    engine::Timestamp now_{0};
    engine::Price fair_price_{10'000};
    bool risk_killed_{false};
    std::string risk_reason_;

    std::mt19937_64 rng_;

    std::vector<engine::OrderId> strategy_resting_bids_;
    std::vector<engine::OrderId> strategy_resting_asks_;

    void enqueue(Event event);
    void scheduleExogenousFlow();
    void scheduleMarketDataFlow();
    void scheduleStrategyTicks();
    void processEvent(const Event& event);
    void handleMarketOrder(const engine::Order& order);
    void handleCancel(engine::OrderId order_id);
    void handleStrategyTick();
    void cancelStrategyQuotes();
    void submitStrategyQuotes(const strategy::QuoteIntent& intent);

    engine::OrderId allocateOrderId();
    strategy::BookView makeBookView() const;
    void onTrades(
        const std::vector<engine::Trade>& trades,
        engine::ParticipantId aggressor
    );
};

} // namespace quantforge::sim
