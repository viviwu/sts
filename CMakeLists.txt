cmake_minimum_required(VERSION 3.20)

project(sts VERSION 0.1.0 LANGUAGES C CXX)

option(STS_BUILD_TESTS "Build STS tests" ON)
option(STS_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
# Vendor SDK gate (M7-2b/c, ADR-033): OFF everywhere — CI, default builds —
# so no vendor header is ever visible outside adapters/ (ADR-027 invariant).
option(STS_ENABLE_VENDOR_SDKS "Build vendor SDK adapters (AMA/QuantAPI)" OFF)
# In-repo SDK root (ADR-040): broker SDKs ship under ./vendor, so vendor
# builds work on a fresh clone with zero flags; still overridable for
# out-of-tree SDK layouts (the 2026-10-01 "no default" decision, revisited).
set(STS_VENDOR_DIR "${PROJECT_SOURCE_DIR}/vendor" CACHE PATH "Vendor SDK root containing ama/quantapi subdirectories")
# PostgreSQL gate (ADR-038): OFF everywhere — CI, default builds — so the
# default build keeps zero system dependencies. ON links the system libpq
# (infra-phase conventional package, like the vendor SDKs) and compiles the
# real PgEventStore; OFF keeps the fail-fast stub in pg_store.cpp.
option(STS_ENABLE_PG "Link system libpq and enable the PostgreSQL projection backend" OFF)
if(STS_ENABLE_PG)
  find_package(PostgreSQL REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unified runtime layout: every built executable and shared library lands in
# ./bin (./bin/cfg holds configs, ./bin/log and ./bin/db are created at
# runtime and never committed). Sanitizer/CI builds that must not clobber
# ./bin override STS_RUNTIME_DIR with their own directory.
set(STS_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/bin" CACHE PATH "Output directory for built executables and shared libraries")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${STS_RUNTIME_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${STS_RUNTIME_DIR}")

# Third-party dependencies, pinned by version (ADR-021, ADR-035): exact
# sources live in the workspace-level ../vendor/oss tree (manifest in
# vendor/README.md), so fresh machines build offline with zero flags. When
# the tree is absent the build falls back to downloading the same pins
# (slow but identical); `tools/sts.sh doctor` reports which case applies.
set(STS_OSS_ROOT "${PROJECT_SOURCE_DIR}/../vendor/oss" CACHE PATH "Vendored oss pin sources (yaml-cpp/json/sqlite)")
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_YAML-CPP AND EXISTS "${STS_OSS_ROOT}/yaml-cpp-0.8.0/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_YAML-CPP "${STS_OSS_ROOT}/yaml-cpp-0.8.0")
endif()
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON AND EXISTS "${STS_OSS_ROOT}/json-3.11.3/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${STS_OSS_ROOT}/json-3.11.3")
endif()
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION AND EXISTS "${STS_OSS_ROOT}/sqlite-3510100/sqlite3.c")
  set(FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION "${STS_OSS_ROOT}/sqlite-3510100")
endif()
include(FetchContent)
# yaml-cpp 0.8.0 predates CMake 3.5 minimum-version support; our own floor
# stays 3.20, this only permits configuring the pinned dependency.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
# SQLite amalgamation (M9-3, ADR-034): pinned zip, no system dependency.
# Offline builds can point FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION at
# an extracted tree, same as the other FetchContent dependencies.
FetchContent_Declare(
    sqlite_amalgamation
    URL https://www.sqlite.org/2025/sqlite-amalgamation-3510100.zip
    URL_HASH SHA256=84a85d6a1b920234349f01720912c12391a4f0cb5cb998087e641dee3ef8ef2e
)
FetchContent_MakeAvailable(yaml-cpp nlohmann_json sqlite_amalgamation)

# Amalgamation ships no CMake target; compile the single translation unit
# directly. Our warning flags stay on sts_core only, never on sqlite3.c.
add_library(sqlite3 STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

add_library(sts_core
    src/config/config.cpp
    src/config/credentials.cpp
    src/persistence/file_ledger.cpp
    src/persistence/sqlite_store.cpp
    src/persistence/event_store.cpp
    src/persistence/pg_store.cpp
    src/ops/metrics.cpp
    src/ops/metrics_endpoint.cpp
    src/ops/log.cpp
    src/ops/admin_view.cpp
    src/ops/admin_socket.cpp
    src/ops/control.cpp
    src/gateway/gateway_service.cpp
    src/gateway/gateway_transport.cpp
    src/market/quote_feed.cpp
    src/broker/alternate_paper_broker.cpp
    src/broker/channel_factory.cpp
    src/broker/ama_snapshot.cpp
    src/broker/ama_tick.cpp
    src/broker/simulated_ama_feed.cpp
    src/broker/routing.cpp
    src/broker/order_book.cpp
    src/execution/execution.cpp
    src/core/event_bus.cpp
    src/market/ashare.cpp
    src/oms/order_manager.cpp
    src/risk/risk_engine.cpp
    src/broker/simulated_broker.cpp
    src/portfolio/portfolio.cpp
    src/strategy/grid_strategy.cpp
    src/strategy/rebalance_strategy.cpp
)

add_library(sts::core ALIAS sts_core)

target_include_directories(sts_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(sts_core PUBLIC cxx_std_17)
target_link_libraries(sts_core PRIVATE yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json sqlite3)
if(STS_ENABLE_PG)
  target_compile_definitions(sts_core PRIVATE STS_HAS_LIBPQ)
  target_link_libraries(sts_core PRIVATE PostgreSQL::PostgreSQL)
endif()

if(MSVC)
    target_compile_options(sts_core PRIVATE /W4 /permissive-)
    if(STS_ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(sts_core PRIVATE /WX)
    endif()
else()
    target_compile_options(sts_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    if(STS_ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(sts_core PRIVATE -Werror)
    endif()
endif()

add_executable(sts_server apps/server_main.cpp)
target_link_libraries(sts_server PRIVATE sts::core)
target_include_directories(sts_server SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

add_executable(sts_replay apps/replay_main.cpp)
target_link_libraries(sts_replay PRIVATE sts::core)

add_executable(sts_admin apps/admin_main.cpp)
target_link_libraries(sts_admin PRIVATE sts::core)
target_include_directories(sts_admin SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

add_executable(sts_cli apps/cli_main.cpp)
target_link_libraries(sts_cli PRIVATE sts::core yaml-cpp::yaml-cpp)
target_include_directories(sts_cli SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

if(STS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(sts_test_core tests/unit/core_test.cpp)
    target_link_libraries(sts_test_core PRIVATE sts::core)
    add_test(NAME sts_test_core COMMAND sts_test_core)

    add_executable(sts_test_config tests/unit/config_test.cpp)
    target_link_libraries(sts_test_config PRIVATE sts::core)
    add_test(NAME sts_test_config COMMAND sts_test_config)

    add_executable(sts_test_market tests/unit/market_test.cpp)
    target_link_libraries(sts_test_market PRIVATE sts::core)
    add_test(NAME sts_test_market COMMAND sts_test_market)

    add_executable(sts_test_risk tests/unit/risk_engine_test.cpp)
    target_link_libraries(sts_test_risk PRIVATE sts::core)
    add_test(NAME sts_test_risk COMMAND sts_test_risk)

    add_executable(sts_test_ashare tests/unit/ashare_test.cpp)
    target_link_libraries(sts_test_ashare PRIVATE sts::core)
    add_test(NAME sts_test_ashare COMMAND sts_test_ashare)

    add_executable(sts_test_oms tests/unit/order_manager_test.cpp)
    target_link_libraries(sts_test_oms PRIVATE sts::core)
    add_test(NAME sts_test_oms COMMAND sts_test_oms)

    add_executable(sts_test_paper tests/unit/paper_engine_test.cpp)
    target_link_libraries(sts_test_paper PRIVATE sts::core)
    add_test(NAME sts_test_paper COMMAND sts_test_paper)

    add_executable(sts_test_recovery tests/unit/recovery_test.cpp)
    target_link_libraries(sts_test_recovery PRIVATE sts::core)
    add_test(NAME sts_test_recovery COMMAND sts_test_recovery)

    add_executable(sts_test_e2e tests/integration/e2e_paper_test.cpp)
    target_link_libraries(sts_test_e2e PRIVATE sts::core)
    add_test(NAME sts_test_e2e COMMAND sts_test_e2e)

    add_executable(sts_test_replay tests/integration/replay_test.cpp)
    target_link_libraries(sts_test_replay PRIVATE sts::core)
    add_test(NAME sts_test_replay COMMAND sts_test_replay)

    add_executable(sts_test_portfolio tests/unit/portfolio_test.cpp)
    target_link_libraries(sts_test_portfolio PRIVATE sts::core)
    add_test(NAME sts_test_portfolio COMMAND sts_test_portfolio)

    add_executable(sts_test_report tests/unit/report_test.cpp)
    target_link_libraries(sts_test_report PRIVATE sts::core)
    add_test(NAME sts_test_report COMMAND sts_test_report)

    add_executable(sts_test_file_ledger tests/unit/file_ledger_test.cpp)
    target_link_libraries(sts_test_file_ledger PRIVATE sts::core nlohmann_json::nlohmann_json)
    add_test(NAME sts_test_file_ledger COMMAND sts_test_file_ledger)

    add_executable(sts_test_alternate tests/unit/alternate_broker_test.cpp)
    target_link_libraries(sts_test_alternate PRIVATE sts::core)
    add_test(NAME sts_test_alternate COMMAND sts_test_alternate)

    add_executable(sts_test_ops tests/unit/ops_test.cpp)
    target_link_libraries(sts_test_ops PRIVATE sts::core)
    add_test(NAME sts_test_ops COMMAND sts_test_ops)

    add_executable(sts_test_execution tests/unit/execution_test.cpp)
    target_link_libraries(sts_test_execution PRIVATE sts::core)
    add_test(NAME sts_test_execution COMMAND sts_test_execution)

    add_executable(sts_test_ama tests/unit/ama_snapshot_test.cpp)
    target_link_libraries(sts_test_ama PRIVATE sts::core)
    add_test(NAME sts_test_ama COMMAND sts_test_ama)

    add_executable(sts_test_ama_tick tests/unit/ama_tick_test.cpp)
    target_link_libraries(sts_test_ama_tick PRIVATE sts::core)
    add_test(NAME sts_test_ama_tick COMMAND sts_test_ama_tick)

    add_executable(sts_test_routing tests/unit/routing_test.cpp)
    target_link_libraries(sts_test_routing PRIVATE sts::core)
    add_test(NAME sts_test_routing COMMAND sts_test_routing)

    add_executable(sts_test_sqlite tests/unit/sqlite_store_test.cpp)
    target_link_libraries(sts_test_sqlite PRIVATE sts::core)
    target_include_directories(sts_test_sqlite SYSTEM PRIVATE ${sqlite_amalgamation_SOURCE_DIR})
    add_test(NAME sts_test_sqlite COMMAND sts_test_sqlite)

    add_executable(sts_test_ledger_verify tests/unit/ledger_verify_test.cpp)
    target_link_libraries(sts_test_ledger_verify PRIVATE sts::core)
    add_test(NAME sts_test_ledger_verify COMMAND sts_test_ledger_verify)

    add_executable(sts_test_channels tests/unit/channel_factory_test.cpp)
    target_link_libraries(sts_test_channels PRIVATE sts::core)
    add_test(NAME sts_test_channels COMMAND sts_test_channels)

    add_executable(sts_test_grid tests/unit/grid_strategy_test.cpp)
    target_link_libraries(sts_test_grid PRIVATE sts::core)
    add_test(NAME sts_test_grid COMMAND sts_test_grid)

    add_executable(sts_test_rebalance tests/unit/rebalance_strategy_test.cpp)
    target_link_libraries(sts_test_rebalance PRIVATE sts::core)
    add_test(NAME sts_test_rebalance COMMAND sts_test_rebalance)

    add_executable(sts_test_feed tests/unit/quote_feed_test.cpp)
    target_link_libraries(sts_test_feed PRIVATE sts::core)
    add_test(NAME sts_test_feed COMMAND sts_test_feed)

    add_executable(sts_test_ama_feed tests/unit/simulated_ama_feed_test.cpp)
    target_link_libraries(sts_test_ama_feed PRIVATE sts::core)
    add_test(NAME sts_test_ama_feed COMMAND sts_test_ama_feed)

    add_executable(sts_test_ama_e2e tests/integration/ama_sim_e2e_test.cpp)
    target_link_libraries(sts_test_ama_e2e PRIVATE sts::core)
    add_test(NAME sts_test_ama_e2e COMMAND sts_test_ama_e2e)

    add_executable(sts_test_exec_gateway tests/integration/execution_gateway_test.cpp)
    target_link_libraries(sts_test_exec_gateway PRIVATE sts::core)
    add_test(NAME sts_test_exec_gateway COMMAND sts_test_exec_gateway)

    add_executable(sts_test_control tests/unit/control_test.cpp)
    target_link_libraries(sts_test_control PRIVATE sts::core)
    add_test(NAME sts_test_control COMMAND sts_test_control)

    add_executable(sts_test_gateway tests/unit/gateway_service_test.cpp)
    target_link_libraries(sts_test_gateway PRIVATE sts::core)
    add_test(NAME sts_test_gateway COMMAND sts_test_gateway)

    add_executable(sts_test_gateway_transport tests/unit/gateway_transport_test.cpp)
    target_link_libraries(sts_test_gateway_transport PRIVATE sts::core nlohmann_json::nlohmann_json)
    add_test(NAME sts_test_gateway_transport COMMAND sts_test_gateway_transport)

    add_executable(sts_test_fees tests/unit/fee_schedule_test.cpp)
    target_link_libraries(sts_test_fees PRIVATE sts::core)
    add_test(NAME sts_test_fees COMMAND sts_test_fees)

    add_executable(sts_test_reconcile tests/unit/reconcile_test.cpp)
    target_link_libraries(sts_test_reconcile PRIVATE sts::core)
    add_test(NAME sts_test_reconcile COMMAND sts_test_reconcile)

    add_executable(sts_bench_throughput tests/bench/throughput_bench.cpp)
    target_link_libraries(sts_bench_throughput PRIVATE sts::core)
    add_test(NAME sts_bench_throughput COMMAND sts_bench_throughput)

    # PostgreSQL conformance (ADR-038): only in PG-gated builds, needs a
    # live server (STS_PG_TEST_CONNINFO); unset means skip, never fail.
    if(STS_ENABLE_PG)
        add_executable(sts_test_pg tests/integration/pg_store_test.cpp)
        target_link_libraries(sts_test_pg PRIVATE sts::core)
        add_test(NAME sts_test_pg COMMAND sts_test_pg)
    endif()
endif()

# Vendor-gated adapters (M7-2b/c, ADR-033). sts_core above never sees a
# vendor header; the CI lint jobs only walk src/ and apps/, and this block
# never runs in CI, so the no-SDK environments stay green by construction.
if(STS_ENABLE_VENDOR_SDKS)
  if(STS_VENDOR_DIR STREQUAL "")
    message(FATAL_ERROR "STS_ENABLE_VENDOR_SDKS=ON requires -DSTS_VENDOR_DIR=<sdk root>")
  endif()
  add_subdirectory(adapters)
endif()
