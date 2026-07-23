#include "quantforge/engine/order_book.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

template <typename Function>
void runBenchmark(
    std::string_view name,
    std::uint64_t operations,
    Function&& function
)
{
    const auto start = Clock::now();

    function();

    const auto end = Clock::now();

    const auto elapsed =
        std::chrono::duration<double>(end - start).count();

    const double operations_per_second =
        static_cast<double>(operations) / elapsed;

    const double nanoseconds_per_operation =
        (elapsed * 1'000'000'000.0) /
        static_cast<double>(operations);

    std::cout
        << std::left << std::setw(34) << name
        << std::right << std::fixed << std::setprecision(2)
        << std::setw(15) << operations_per_second
        << " ops/s"
        << std::setw(15) << nanoseconds_per_operation
        << " ns/op\n";
}

quantforge::engine::Order makeLimitOrder(
    quantforge::engine::OrderId id,
    quantforge::engine::Side side,
    quantforge::engine::Price price,
    quantforge::engine::Quantity quantity,
    quantforge::engine::Timestamp timestamp
)
{
    return quantforge::engine::Order{
        id,
        side,
        quantforge::engine::OrderType::Limit,
        price,
        quantity,
        timestamp
    };
}

} // namespace

int main()
{
    constexpr std::uint64_t operation_count = 100'000;

    std::cout << "QuantForge Order Book Benchmark\n";
    std::cout << "Operations per scenario: " << operation_count << "\n\n";

    std::cout
        << std::left << std::setw(34) << "Scenario"
        << std::right << std::setw(21) << "Throughput"
        << std::setw(20) << "Average latency\n";

    std::cout << std::string(75, '-') << '\n';

    runBenchmark(
        "Non-crossing limit insertions",
        operation_count,
        [] {
            quantforge::engine::OrderBook book;

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                book.addOrder(makeLimitOrder(
                    i,
                    quantforge::engine::Side::Buy,
                    10'000 - static_cast<quantforge::engine::Price>(i % 100),
                    1,
                    i
                ));
            }
        }
    );

    runBenchmark(
        "Crossing limit-order matches",
        operation_count,
        [] {
            quantforge::engine::OrderBook book;

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                book.addOrder(makeLimitOrder(
                    i,
                    quantforge::engine::Side::Sell,
                    10'000,
                    1,
                    i
                ));
            }

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                book.addOrder(makeLimitOrder(
                    operation_count + i,
                    quantforge::engine::Side::Buy,
                    10'000,
                    1,
                    operation_count + i
                ));
            }
        }
    );

    runBenchmark(
        "Order cancellations",
        operation_count,
        [] {
            quantforge::engine::OrderBook book;

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                book.addOrder(makeLimitOrder(
                    i,
                    quantforge::engine::Side::Buy,
                    10'000,
                    1,
                    i
                ));
            }

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                book.cancelOrder(i);
            }
        }
    );

    runBenchmark(
        "Alternating insert and match",
        operation_count * 2,
        [] {
            quantforge::engine::OrderBook book;

            for (std::uint64_t i = 1; i <= operation_count; ++i) {
                const auto sell_id = (i * 2) - 1;
                const auto buy_id = i * 2;

                book.addOrder(makeLimitOrder(
                    sell_id,
                    quantforge::engine::Side::Sell,
                    10'000,
                    1,
                    sell_id
                ));

                book.addOrder(makeLimitOrder(
                    buy_id,
                    quantforge::engine::Side::Buy,
                    10'000,
                    1,
                    buy_id
                ));
            }
        }
    );

    return 0;
}
