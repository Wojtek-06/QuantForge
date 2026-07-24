include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(quantforge_tests
    test_order_book.cpp
    test_simulator.cpp
    test_marketdata.cpp
    test_leakage.cpp
    test_risk_walkforward.cpp
)

target_link_libraries(quantforge_tests
    PRIVATE
        quantforge_lib
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(quantforge_tests)
