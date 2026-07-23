#pragma once

#include "quantforge/engine/types.hpp"
#include "quantforge/marketdata/types.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace quantforge::marketdata {

/// Thrown when code attempts to read market data strictly after the as-of clock.
class LookAheadError : public std::runtime_error {
public:
    explicit LookAheadError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/// Monotonic simulation clock that gates data access.
class AsOfClock {
public:
    engine::Timestamp now() const { return now_; }

    void advanceTo(engine::Timestamp t)
    {
        if (t < now_) {
            throw std::invalid_argument(
                "AsOfClock::advanceTo cannot move backwards"
            );
        }
        now_ = t;
    }

    void reset(engine::Timestamp t = 0) { now_ = t; }

    /// Fail closed if `data_ts` is in the future relative to the clock.
    void guard(engine::Timestamp data_ts, const char* context) const
    {
        if (data_ts > now_) {
            throw LookAheadError(
                std::string("look-ahead blocked in ") + context +
                ": data_ts=" + std::to_string(data_ts) +
                " > now=" + std::to_string(now_)
            );
        }
    }

private:
    engine::Timestamp now_{0};
};

/// Sorted event series with as-of / look-ahead guards.
class AsOfSeries {
public:
    AsOfSeries() = default;

    explicit AsOfSeries(MarketEventSeries events)
        : events_(std::move(events))
    {
    }

    const MarketEventSeries& events() const { return events_; }
    AsOfClock& clock() { return clock_; }
    const AsOfClock& clock() const { return clock_; }

    void reset()
    {
        clock_.reset(0);
        cursor_ = 0;
    }

    void advanceTo(engine::Timestamp t)
    {
        clock_.advanceTo(t);
        while (cursor_ < events_.size() &&
               events_[cursor_].timestamp <= clock_.now()) {
            ++cursor_;
        }
    }

    /// Latest mid with timestamp <= as-of clock (throws if caller passes future t).
    std::optional<engine::Price> midAsOf(engine::Timestamp t) const
    {
        clock_.guard(t, "midAsOf");
        std::optional<engine::Price> mid;
        for (const auto& e : events_) {
            if (e.timestamp > t) {
                break;
            }
            if (e.kind == EventKind::Mid) {
                mid = e.price;
            }
        }
        return mid;
    }

    /// Events with timestamp in (prev_exclusive, t] — t must be <= clock.
    std::vector<MarketEvent> eventsInRange(
        engine::Timestamp prev_exclusive,
        engine::Timestamp t
    ) const
    {
        clock_.guard(t, "eventsInRange");
        std::vector<MarketEvent> out;
        for (const auto& e : events_) {
            if (e.timestamp <= prev_exclusive) {
                continue;
            }
            if (e.timestamp > t) {
                break;
            }
            out.push_back(e);
        }
        return out;
    }

    /// Intentionally unsafe helper used only by leakage tests.
    std::optional<engine::Price> midUnguarded(engine::Timestamp t) const
    {
        std::optional<engine::Price> mid;
        for (const auto& e : events_) {
            if (e.timestamp > t) {
                break;
            }
            if (e.kind == EventKind::Mid) {
                mid = e.price;
            }
        }
        return mid;
    }

    std::size_t size() const { return events_.size(); }

private:
    MarketEventSeries events_{};
    mutable AsOfClock clock_{};
    std::size_t cursor_{0};
};

} // namespace quantforge::marketdata
