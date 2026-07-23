#include "quantforge/engine/order_book.hpp"

#include <gtest/gtest.h>

#include "quantforge/engine/exceptions.hpp"

namespace {

using quantforge::engine::Order;
using quantforge::engine::OrderBook;
using quantforge::engine::OrderType;
using quantforge::engine::Side;
using quantforge::engine::TimeInForce;

Order makeLimitOrder(
    quantforge::engine::OrderId id,
    Side side,
    quantforge::engine::Price price,
    quantforge::engine::Quantity quantity,
    quantforge::engine::Timestamp timestamp,
    TimeInForce tif = TimeInForce::GTC
)
{
    return Order{
        id,
        side,
        OrderType::Limit,
        price,
        quantity,
        timestamp,
        tif
    };
}

Order makeMarketOrder(
    quantforge::engine::OrderId id,
    Side side,
    quantforge::engine::Quantity quantity,
    quantforge::engine::Timestamp timestamp
)
{
    return Order{
        id,
        side,
        OrderType::Market,
        0,
        quantity,
        timestamp
    };
}

TEST(OrderBookTest, EmptyBookHasNoBestPrices)
{
    OrderBook book;

    EXPECT_TRUE(book.empty());
    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBookTest, NonCrossingLimitOrdersRestInBook)
{
    OrderBook book;

    const auto buy_trades =
        book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 50, 1));

    const auto sell_trades =
        book.addOrder(makeLimitOrder(2, Side::Sell, 10100, 40, 2));

    EXPECT_TRUE(buy_trades.empty());
    EXPECT_TRUE(sell_trades.empty());

    ASSERT_TRUE(book.bestBid().has_value());
    ASSERT_TRUE(book.bestAsk().has_value());

    EXPECT_EQ(*book.bestBid(), 10000);
    EXPECT_EQ(*book.bestAsk(), 10100);
}

TEST(OrderBookTest, CrossingLimitOrdersGenerateTrade)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 50, 1));

    const auto trades =
        book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 50, 2));

    ASSERT_EQ(trades.size(), 1);

    EXPECT_EQ(trades[0].buy_order_id, 2);
    EXPECT_EQ(trades[0].sell_order_id, 1);
    EXPECT_EQ(trades[0].price, 10100);
    EXPECT_EQ(trades[0].quantity, 50);

    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, PartialFillLeavesRemainingOrder)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 100, 1));

    const auto trades =
        book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 40, 2));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 40);

    ASSERT_TRUE(book.bestAsk().has_value());
    EXPECT_EQ(*book.bestAsk(), 10100);
    EXPECT_FALSE(book.bestBid().has_value());
}

TEST(OrderBookTest, IncomingOrderCanMatchMultipleOrders)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 50, 1));
    book.addOrder(makeLimitOrder(2, Side::Sell, 10100, 40, 2));

    const auto trades =
        book.addOrder(makeLimitOrder(3, Side::Buy, 10100, 70, 3));

    ASSERT_EQ(trades.size(), 2);

    EXPECT_EQ(trades[0].sell_order_id, 1);
    EXPECT_EQ(trades[0].quantity, 50);

    EXPECT_EQ(trades[1].sell_order_id, 2);
    EXPECT_EQ(trades[1].quantity, 20);

    ASSERT_TRUE(book.bestAsk().has_value());
    EXPECT_EQ(*book.bestAsk(), 10100);
}

TEST(OrderBookTest, OrdersAtSamePriceUseTimePriority)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(10, Side::Sell, 10100, 25, 1));
    book.addOrder(makeLimitOrder(11, Side::Sell, 10100, 25, 2));

    const auto trades =
        book.addOrder(makeLimitOrder(12, Side::Buy, 10100, 25, 3));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].sell_order_id, 10);
}

TEST(OrderBookTest, BetterPriceHasPriority)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10200, 20, 1));
    book.addOrder(makeLimitOrder(2, Side::Sell, 10100, 20, 2));

    const auto trades =
        book.addOrder(makeLimitOrder(3, Side::Buy, 10200, 20, 3));

    ASSERT_EQ(trades.size(), 1);

    EXPECT_EQ(trades[0].sell_order_id, 2);
    EXPECT_EQ(trades[0].price, 10100);
}

TEST(OrderBookTest, MarketOrderConsumesAvailableLiquidity)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 30, 1));

    const auto trades =
        book.addOrder(makeMarketOrder(2, Side::Sell, 20, 2));

    ASSERT_EQ(trades.size(), 1);

    EXPECT_EQ(trades[0].buy_order_id, 1);
    EXPECT_EQ(trades[0].sell_order_id, 2);
    EXPECT_EQ(trades[0].quantity, 20);

    ASSERT_TRUE(book.bestBid().has_value());
    EXPECT_EQ(*book.bestBid(), 10000);
}

TEST(OrderBookTest, UnfilledMarketOrderDoesNotRest)
{
    OrderBook book;

    const auto trades =
        book.addOrder(makeMarketOrder(1, Side::Buy, 100, 1));

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, RestingOrderCanBeCancelled)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 50, 1));

    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, CancellingUnknownOrderReturnsFalse)
{
    OrderBook book;

    EXPECT_FALSE(book.cancelOrder(999));
}

TEST(OrderBookTest, RejectsZeroQuantityOrder)
{
    OrderBook book;

    EXPECT_THROW(
        book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 0, 1)),
        quantforge::engine::InvalidOrder
    );
}

TEST(OrderBookTest, RejectsInvalidLimitPrice)
{
    OrderBook book;

    EXPECT_THROW(
        book.addOrder(makeLimitOrder(1, Side::Buy, 0, 50, 1)),
        quantforge::engine::InvalidOrder
    );
}

TEST(OrderBookTest, RejectsZeroOrderId)
{
    OrderBook book;

    EXPECT_THROW(
        book.addOrder(makeLimitOrder(0, Side::Buy, 10000, 50, 1)),
        quantforge::engine::InvalidOrder
    );
}

TEST(OrderBookTest, RejectsDuplicateRestingOrderId)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 50, 1));

    EXPECT_THROW(
        book.addOrder(makeLimitOrder(1, Side::Buy, 9900, 20, 2)),
        quantforge::engine::DuplicateOrderId
    );
}

TEST(OrderBookTest, MarketOrderDoesNotRequirePositivePrice)
{
    OrderBook book;

    EXPECT_NO_THROW(
        book.addOrder(makeMarketOrder(1, Side::Buy, 50, 1))
    );
}

TEST(OrderBookTest, ReportsAggregatedQuantityAtPriceLevel)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 40, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10000, 60, 2));
    book.addOrder(makeLimitOrder(3, Side::Sell, 10100, 25, 3));

    EXPECT_EQ(book.bidQuantityAt(10000), 100);
    EXPECT_EQ(book.askQuantityAt(10100), 25);
}

TEST(OrderBookTest, MissingPriceLevelHasZeroQuantity)
{
    OrderBook book;

    EXPECT_EQ(book.bidQuantityAt(10000), 0);
    EXPECT_EQ(book.askQuantityAt(10100), 0);
}

TEST(OrderBookTest, PartialFillUpdatesPriceLevelQuantity)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 100, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 40, 2));

    EXPECT_EQ(book.askQuantityAt(10100), 60);
}

TEST(OrderBookTest, PartiallyFilledIncomingLimitOrderRestsInBook)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 40, 1));

    const auto trades = book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 100, 2));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 40);

    ASSERT_TRUE(book.bestBid().has_value());
    EXPECT_EQ(*book.bestBid(), 10100);
    EXPECT_EQ(book.bidQuantityAt(10100), 60);

    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBookTest, BuyOrderMatchesAcrossMultipleAskPriceLevels)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 30, 1));
    book.addOrder(makeLimitOrder(2, Side::Sell, 10200, 40, 2));
    book.addOrder(makeLimitOrder(3, Side::Sell, 10300, 50, 3));

    const auto trades = book.addOrder(makeLimitOrder(4, Side::Buy, 10200, 60, 4));

    ASSERT_EQ(trades.size(), 2);

    EXPECT_EQ(trades[0].sell_order_id, 1);
    EXPECT_EQ(trades[0].price, 10100);
    EXPECT_EQ(trades[0].quantity, 30);

    EXPECT_EQ(trades[1].sell_order_id, 2);
    EXPECT_EQ(trades[1].price, 10200);
    EXPECT_EQ(trades[1].quantity, 30);

    EXPECT_EQ(book.askQuantityAt(10200), 10);
    EXPECT_EQ(book.askQuantityAt(10300), 50);

    ASSERT_TRUE(book.bestAsk().has_value());
    EXPECT_EQ(*book.bestAsk(), 10200);
}

TEST(OrderBookTest, SellOrderMatchesHighestBidFirst)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 9900, 20, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 20, 2));
    book.addOrder(makeLimitOrder(3, Side::Buy, 10000, 20, 3));

    const auto trades = book.addOrder(makeLimitOrder(4, Side::Sell, 9900, 20, 4));

    ASSERT_EQ(trades.size(), 1);

    EXPECT_EQ(trades[0].buy_order_id, 2);
    EXPECT_EQ(trades[0].sell_order_id, 4);
    EXPECT_EQ(trades[0].price, 10100);
    EXPECT_EQ(trades[0].quantity, 20);

    ASSERT_TRUE(book.bestBid().has_value());
    EXPECT_EQ(*book.bestBid(), 10000);
}

TEST(OrderBookTest, CancellingOneOrderPreservesOtherOrdersAtSameLevel)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 40, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10000, 60, 2));

    EXPECT_EQ(book.bidQuantityAt(10000), 100);

    EXPECT_TRUE(book.cancelOrder(1));

    EXPECT_EQ(book.bidQuantityAt(10000), 60);

    ASSERT_TRUE(book.bestBid().has_value());
    EXPECT_EQ(*book.bestBid(), 10000);
}

TEST(OrderBookTest, CancellingLastOrderRemovesPriceLevel)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 50, 1));

    EXPECT_TRUE(book.cancelOrder(1));

    EXPECT_EQ(book.askQuantityAt(10100), 0);
    EXPECT_FALSE(book.bestAsk().has_value());
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, FullyFilledOrderCannotBeCancelled)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 50, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 50, 2));

    EXPECT_FALSE(book.cancelOrder(1));
    EXPECT_FALSE(book.cancelOrder(2));
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, MarketOrderConsumesMultiplePriceLevels)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 25, 1));
    book.addOrder(makeLimitOrder(2, Side::Sell, 10200, 35, 2));
    book.addOrder(makeLimitOrder(3, Side::Sell, 10300, 50, 3));

    const auto trades = book.addOrder(makeMarketOrder(4, Side::Buy, 80, 4));

    ASSERT_EQ(trades.size(), 3);

    EXPECT_EQ(trades[0].price, 10100);
    EXPECT_EQ(trades[0].quantity, 25);

    EXPECT_EQ(trades[1].price, 10200);
    EXPECT_EQ(trades[1].quantity, 35);

    EXPECT_EQ(trades[2].price, 10300);
    EXPECT_EQ(trades[2].quantity, 20);

    EXPECT_EQ(book.askQuantityAt(10300), 30);

    ASSERT_TRUE(book.bestAsk().has_value());
    EXPECT_EQ(*book.bestAsk(), 10300);
}

TEST(OrderBookTest, MarketOrderOnEmptyBookProducesNoTrade)
{
    OrderBook book;

    const auto trades = book.addOrder(makeMarketOrder(1, Side::Sell, 100, 1));

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, OrderIdCanBeReusedAfterCancellation)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Buy, 10000, 50, 1));

    ASSERT_TRUE(book.cancelOrder(1));

    EXPECT_NO_THROW(
        book.addOrder(makeLimitOrder(1, Side::Buy, 9900, 25, 2))
    );

    EXPECT_EQ(book.bidQuantityAt(9900), 25);
}

TEST(OrderBookTest, OrderIdCanBeReusedAfterFullExecution)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 50, 1));
    book.addOrder(makeLimitOrder(2, Side::Buy, 10100, 50, 2));

    EXPECT_NO_THROW(
        book.addOrder(makeLimitOrder(1, Side::Buy, 9900, 20, 3))
    );

    EXPECT_EQ(book.bidQuantityAt(9900), 20);
}

TEST(OrderBookTest, IocDoesNotRestUnfilledRemainder)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 40, 1));

    const auto trades = book.addOrder(
        makeLimitOrder(2, Side::Buy, 10100, 100, 2, TimeInForce::IOC)
    );

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 40);
    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, FokRejectsWhenLiquidityInsufficient)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 40, 1));

    const auto trades = book.addOrder(
        makeLimitOrder(2, Side::Buy, 10100, 100, 2, TimeInForce::FOK)
    );

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.askQuantityAt(10100), 40);
}

TEST(OrderBookTest, FokFillsCompletelyWhenLiquidityExists)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 100, 1));

    const auto trades = book.addOrder(
        makeLimitOrder(2, Side::Buy, 10100, 100, 2, TimeInForce::FOK)
    );

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 100);
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, QueuePositionTracksTimePriority)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::Sell, 10100, 10, 1));
    book.addOrder(makeLimitOrder(2, Side::Sell, 10100, 10, 2));
    book.addOrder(makeLimitOrder(3, Side::Sell, 10100, 10, 3));

    ASSERT_TRUE(book.queuePosition(1).has_value());
    ASSERT_TRUE(book.queuePosition(2).has_value());
    ASSERT_TRUE(book.queuePosition(3).has_value());

    EXPECT_EQ(*book.queuePosition(1), 0u);
    EXPECT_EQ(*book.queuePosition(2), 1u);
    EXPECT_EQ(*book.queuePosition(3), 2u);
}

TEST(OrderBookTest, TradesIncludeFees)
{
    OrderBook book{quantforge::engine::FeeSchedule{ -1.0, 2.0 }};

    book.addOrder(makeLimitOrder(1, Side::Sell, 10'000, 10, 1));
    const auto trades = book.addOrder(makeLimitOrder(2, Side::Buy, 10'000, 10, 2));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_GT(trades[0].taker_fee, 0.0);
    EXPECT_LT(trades[0].maker_fee, 0.0);  // rebate
}

} // namespace
