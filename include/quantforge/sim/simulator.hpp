#pragma once

#include "quantforge/engine/order_book.hpp"
#include "quantforge/metrics/accounting.hpp"
#include "quantforge/sim/event.hpp"
#include "quantforge/sim/latency_model.hpp"
#include "quantforge/strategy/strategy.hpp"

#include <cstdint>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace quantforge::sim {

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
};

struct SimulationResult {
    metrics::PortfolioSnapshot final_snapshot{};
    metrics::MmMetrics metrics{};
    std::string strategy_name;
    std::uint64_t events_processed{0};
    std::uint64_t trades{0};
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

    std::priority_queue<Event, std::vector<Event>, EventCompare> queue_;
    std::uint64_t next_sequence_{0};
    engine::OrderId next_order_id_{1};
    engine::Timestamp now_{0};
    engine::Price fair_price_{10'000};

    std::mt19937_64 rng_;

    std::vector<engine::OrderId> strategy_resting_bids_;
    std::vector<engine::OrderId> strategy_resting_asks_;

    void enqueue(Event event);
    void scheduleExogenousFlow();
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
