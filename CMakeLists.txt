cmake_minimum_required(VERSION 3.25)

project(
    QuantForge
    VERSION 0.1.0
    DESCRIPTION "Event-driven LOB market-making and backtesting laboratory."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(quantforge_lib
    src/order_book.cpp
    src/accounting.cpp
    src/simulator.cpp
    src/experiment.cpp
)

target_include_directories(quantforge_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(quantforge_lib PRIVATE /W4 /permissive-)
else()
    target_compile_options(quantforge_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lob_demo
    apps/lob_demo.cpp
)

target_link_libraries(lob_demo
    PRIVATE
        quantforge_lib
)

add_executable(quantforge
    apps/quantforge_demo.cpp
)

target_link_libraries(quantforge
    PRIVATE
        quantforge_lib
)

option(BUILD_TESTING "Build automated tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

option(BUILD_BENCHMARKS "Build performance benchmarks" ON)

if(BUILD_BENCHMARKS)
    add_executable(quantforge_benchmark
        benchmarks/benchmark_order_book.cpp
    )

    target_link_libraries(quantforge_benchmark
        PRIVATE
            quantforge_lib
    )
endif()
