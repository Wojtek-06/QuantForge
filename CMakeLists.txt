cmake_minimum_required(VERSION 3.25)

project(
    QuantForge
    VERSION 0.4.0
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
    src/csv_loader.cpp
    src/naive_bar_backtester.cpp
    src/comparison_report.cpp
    src/config_loader.cpp
    src/walk_forward.cpp
    src/param_search.cpp
    src/replay.cpp
    src/json_export.cpp
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

add_executable(lob_replay_export
    apps/lob_replay_export.cpp
)

target_link_libraries(lob_replay_export
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

# ---------------------------------------------------------------------------
# Optional pybind11 research module (additive; CLI remains the default path)
# ---------------------------------------------------------------------------
option(QUANTFORGE_BUILD_PYTHON "Build pybind11 Python extension _quantforge" OFF)

if(QUANTFORGE_BUILD_PYTHON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set_target_properties(quantforge_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
    )
    set(PYBIND11_FINDPYTHON ON)
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(_quantforge
        python/bindings/module.cpp
    )

    target_link_libraries(_quantforge
        PRIVATE
            quantforge_lib
    )

    # Official CPython on Windows is MSVC-built; MinGW modules need their
    # C++ runtime available. Prefer static libgcc/libstdc++ and also copy
    # remaining MinGW runtime DLLs next to the .pyd for import-time loading.
    if(MINGW)
        target_link_options(_quantforge PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()

    # Drop the extension next to the importable Python package.
    set(_qf_py_out "${PROJECT_SOURCE_DIR}/python/quantforge")
    set_target_properties(_quantforge PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${_qf_py_out}"
        RUNTIME_OUTPUT_DIRECTORY "${_qf_py_out}"
        ARCHIVE_OUTPUT_DIRECTORY "${_qf_py_out}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_qf_py_out}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_qf_py_out}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_qf_py_out}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_qf_py_out}"
    )

    if(MINGW)
        foreach(_qf_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "C:/msys64/mingw64/bin/${_qf_dll}")
                add_custom_command(TARGET _quantforge POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "C:/msys64/mingw64/bin/${_qf_dll}"
                        "${_qf_py_out}/${_qf_dll}"
                    COMMENT "Bundling ${_qf_dll} next to _quantforge"
                )
            endif()
        endforeach()
    endif()

    message(STATUS "QuantForge Python module will output to ${_qf_py_out}")
endif()
