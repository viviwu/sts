cmake_minimum_required(VERSION 3.20)

project(qts VERSION 0.1.0 LANGUAGES C CXX)

option(QTS_BUILD_TESTS "Build QTS tests" ON)
option(QTS_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
# Vendor SDK gate (M7-2b/c, ADR-033): OFF everywhere — CI, default builds —
# so no vendor header is ever visible outside adapters/ (ADR-027 invariant).
option(QTS_ENABLE_VENDOR_SDKS "Build vendor SDK adapters (AMA/QuantAPI)" OFF)
# In-repo SDK root (ADR-040): broker SDKs ship under ./vendor, so vendor
# builds work on a fresh clone with zero flags; still overridable for
# out-of-tree SDK layouts (the 2026-10-01 "no default" decision, revisited).
set(QTS_VENDOR_DIR "${PROJECT_SOURCE_DIR}/vendor" CACHE PATH "Vendor SDK root containing ama/quantapi subdirectories")
# PostgreSQL gate (ADR-038): OFF everywhere — CI, default builds — so the
# default build keeps zero system dependencies. ON links the system libpq
# (infra-phase conventional package, like the vendor SDKs) and compiles the
# real PgEventStore; OFF keeps the fail-fast stub in pg_store.cpp.
option(QTS_ENABLE_PG "Link system libpq and enable the PostgreSQL projection backend" OFF)
if(QTS_ENABLE_PG)
  find_package(PostgreSQL REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unified runtime layout: every built executable and shared library lands in
# ./bin (./bin/cfg holds configs, ./bin/log and ./bin/db are created at
# runtime and never committed). Sanitizer/CI builds that must not clobber
# ./bin override QTS_RUNTIME_DIR with their own directory.
set(QTS_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/bin" CACHE PATH "Output directory for built executables and shared libraries")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${QTS_RUNTIME_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${QTS_RUNTIME_DIR}")

# Third-party dependencies, pinned by version (ADR-021, ADR-035): exact
# sources live in the workspace-level ../vendor/oss tree (manifest in
# vendor/README.md), so fresh machines build offline with zero flags. When
# the tree is absent the build falls back to downloading the same pins
# (slow but identical); `tools/qts.sh doctor` reports which case applies.
set(QTS_OSS_ROOT "${PROJECT_SOURCE_DIR}/../vendor/oss" CACHE PATH "Vendored oss pin sources (yaml-cpp/json/sqlite)")
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_YAML-CPP AND EXISTS "${QTS_OSS_ROOT}/yaml-cpp-0.8.0/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_YAML-CPP "${QTS_OSS_ROOT}/yaml-cpp-0.8.0")
endif()
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON AND EXISTS "${QTS_OSS_ROOT}/json-3.11.3/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${QTS_OSS_ROOT}/json-3.11.3")
endif()
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION AND EXISTS "${QTS_OSS_ROOT}/sqlite-3510100/sqlite3.c")
  set(FETCHCONTENT_SOURCE_DIR_SQLITE_AMALGAMATION "${QTS_OSS_ROOT}/sqlite-3510100")
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
# directly. Our warning flags stay on qts_core only, never on sqlite3.c.
add_library(sqlite3 STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

add_library(qts_core
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

add_library(qts::core ALIAS qts_core)

target_include_directories(qts_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(qts_core PUBLIC cxx_std_17)
target_link_libraries(qts_core PRIVATE yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json sqlite3)
if(QTS_ENABLE_PG)
  target_compile_definitions(qts_core PRIVATE QTS_HAS_LIBPQ)
  target_link_libraries(qts_core PRIVATE PostgreSQL::PostgreSQL)
endif()

if(MSVC)
    target_compile_options(qts_core PRIVATE /W4 /permissive-)
    if(QTS_ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(qts_core PRIVATE /WX)
    endif()
else()
    target_compile_options(qts_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    if(QTS_ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(qts_core PRIVATE -Werror)
    endif()
endif()

add_executable(qts_server apps/server_main.cpp)
target_link_libraries(qts_server PRIVATE qts::core)
target_include_directories(qts_server SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

add_executable(qts_replay apps/replay_main.cpp)
target_link_libraries(qts_replay PRIVATE qts::core)

add_executable(qts_admin apps/admin_main.cpp)
target_link_libraries(qts_admin PRIVATE qts::core)
target_include_directories(qts_admin SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

add_executable(qts_cli apps/cli_main.cpp)
target_link_libraries(qts_cli PRIVATE qts::core yaml-cpp::yaml-cpp)
target_include_directories(qts_cli SYSTEM PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

if(QTS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(qts_test_core tests/unit/core_test.cpp)
    target_link_libraries(qts_test_core PRIVATE qts::core)
    add_test(NAME qts_test_core COMMAND qts_test_core)

    add_executable(qts_test_config tests/unit/config_test.cpp)
    target_link_libraries(qts_test_config PRIVATE qts::core)
    add_test(NAME qts_test_config COMMAND qts_test_config)

    add_executable(qts_test_market tests/unit/market_test.cpp)
    target_link_libraries(qts_test_market PRIVATE qts::core)
    add_test(NAME qts_test_market COMMAND qts_test_market)

    add_executable(qts_test_risk tests/unit/risk_engine_test.cpp)
    target_link_libraries(qts_test_risk PRIVATE qts::core)
    add_test(NAME qts_test_risk COMMAND qts_test_risk)

    add_executable(qts_test_ashare tests/unit/ashare_test.cpp)
    target_link_libraries(qts_test_ashare PRIVATE qts::core)
    add_test(NAME qts_test_ashare COMMAND qts_test_ashare)

    add_executable(qts_test_oms tests/unit/order_manager_test.cpp)
    target_link_libraries(qts_test_oms PRIVATE qts::core)
    add_test(NAME qts_test_oms COMMAND qts_test_oms)

    add_executable(qts_test_paper tests/unit/paper_engine_test.cpp)
    target_link_libraries(qts_test_paper PRIVATE qts::core)
    add_test(NAME qts_test_paper COMMAND qts_test_paper)

    add_executable(qts_test_recovery tests/unit/recovery_test.cpp)
    target_link_libraries(qts_test_recovery PRIVATE qts::core)
    add_test(NAME qts_test_recovery COMMAND qts_test_recovery)

    add_executable(qts_test_e2e tests/integration/e2e_paper_test.cpp)
    target_link_libraries(qts_test_e2e PRIVATE qts::core)
    add_test(NAME qts_test_e2e COMMAND qts_test_e2e)

    add_executable(qts_test_replay tests/integration/replay_test.cpp)
    target_link_libraries(qts_test_replay PRIVATE qts::core)
    add_test(NAME qts_test_replay COMMAND qts_test_replay)

    add_executable(qts_test_portfolio tests/unit/portfolio_test.cpp)
    target_link_libraries(qts_test_portfolio PRIVATE qts::core)
    add_test(NAME qts_test_portfolio COMMAND qts_test_portfolio)

    add_executable(qts_test_report tests/unit/report_test.cpp)
    target_link_libraries(qts_test_report PRIVATE qts::core)
    add_test(NAME qts_test_report COMMAND qts_test_report)

    add_executable(qts_test_file_ledger tests/unit/file_ledger_test.cpp)
    target_link_libraries(qts_test_file_ledger PRIVATE qts::core nlohmann_json::nlohmann_json)
    add_test(NAME qts_test_file_ledger COMMAND qts_test_file_ledger)

    add_executable(qts_test_alternate tests/unit/alternate_broker_test.cpp)
    target_link_libraries(qts_test_alternate PRIVATE qts::core)
    add_test(NAME qts_test_alternate COMMAND qts_test_alternate)

    add_executable(qts_test_ops tests/unit/ops_test.cpp)
    target_link_libraries(qts_test_ops PRIVATE qts::core)
    add_test(NAME qts_test_ops COMMAND qts_test_ops)

    add_executable(qts_test_execution tests/unit/execution_test.cpp)
    target_link_libraries(qts_test_execution PRIVATE qts::core)
    add_test(NAME qts_test_execution COMMAND qts_test_execution)

    add_executable(qts_test_ama tests/unit/ama_snapshot_test.cpp)
    target_link_libraries(qts_test_ama PRIVATE qts::core)
    add_test(NAME qts_test_ama COMMAND qts_test_ama)

    add_executable(qts_test_ama_tick tests/unit/ama_tick_test.cpp)
    target_link_libraries(qts_test_ama_tick PRIVATE qts::core)
    add_test(NAME qts_test_ama_tick COMMAND qts_test_ama_tick)

    add_executable(qts_test_routing tests/unit/routing_test.cpp)
    target_link_libraries(qts_test_routing PRIVATE qts::core)
    add_test(NAME qts_test_routing COMMAND qts_test_routing)

    add_executable(qts_test_sqlite tests/unit/sqlite_store_test.cpp)
    target_link_libraries(qts_test_sqlite PRIVATE qts::core)
    target_include_directories(qts_test_sqlite SYSTEM PRIVATE ${sqlite_amalgamation_SOURCE_DIR})
    add_test(NAME qts_test_sqlite COMMAND qts_test_sqlite)

    add_executable(qts_test_ledger_verify tests/unit/ledger_verify_test.cpp)
    target_link_libraries(qts_test_ledger_verify PRIVATE qts::core)
    add_test(NAME qts_test_ledger_verify COMMAND qts_test_ledger_verify)

    add_executable(qts_test_channels tests/unit/channel_factory_test.cpp)
    target_link_libraries(qts_test_channels PRIVATE qts::core)
    add_test(NAME qts_test_channels COMMAND qts_test_channels)

    add_executable(qts_test_grid tests/unit/grid_strategy_test.cpp)
    target_link_libraries(qts_test_grid PRIVATE qts::core)
    add_test(NAME qts_test_grid COMMAND qts_test_grid)

    add_executable(qts_test_rebalance tests/unit/rebalance_strategy_test.cpp)
    target_link_libraries(qts_test_rebalance PRIVATE qts::core)
    add_test(NAME qts_test_rebalance COMMAND qts_test_rebalance)

    add_executable(qts_test_feed tests/unit/quote_feed_test.cpp)
    target_link_libraries(qts_test_feed PRIVATE qts::core)
    add_test(NAME qts_test_feed COMMAND qts_test_feed)

    add_executable(qts_test_ama_feed tests/unit/simulated_ama_feed_test.cpp)
    target_link_libraries(qts_test_ama_feed PRIVATE qts::core)
    add_test(NAME qts_test_ama_feed COMMAND qts_test_ama_feed)

    add_executable(qts_test_ama_e2e tests/integration/ama_sim_e2e_test.cpp)
    target_link_libraries(qts_test_ama_e2e PRIVATE qts::core)
    add_test(NAME qts_test_ama_e2e COMMAND qts_test_ama_e2e)

    add_executable(qts_test_exec_gateway tests/integration/execution_gateway_test.cpp)
    target_link_libraries(qts_test_exec_gateway PRIVATE qts::core)
    add_test(NAME qts_test_exec_gateway COMMAND qts_test_exec_gateway)

    add_executable(qts_test_control tests/unit/control_test.cpp)
    target_link_libraries(qts_test_control PRIVATE qts::core)
    add_test(NAME qts_test_control COMMAND qts_test_control)

    add_executable(qts_test_gateway tests/unit/gateway_service_test.cpp)
    target_link_libraries(qts_test_gateway PRIVATE qts::core)
    add_test(NAME qts_test_gateway COMMAND qts_test_gateway)

    add_executable(qts_test_gateway_transport tests/unit/gateway_transport_test.cpp)
    target_link_libraries(qts_test_gateway_transport PRIVATE qts::core nlohmann_json::nlohmann_json)
    add_test(NAME qts_test_gateway_transport COMMAND qts_test_gateway_transport)

    add_executable(qts_test_fees tests/unit/fee_schedule_test.cpp)
    target_link_libraries(qts_test_fees PRIVATE qts::core)
    add_test(NAME qts_test_fees COMMAND qts_test_fees)

    add_executable(qts_test_reconcile tests/unit/reconcile_test.cpp)
    target_link_libraries(qts_test_reconcile PRIVATE qts::core)
    add_test(NAME qts_test_reconcile COMMAND qts_test_reconcile)

    add_executable(qts_bench_throughput tests/bench/throughput_bench.cpp)
    target_link_libraries(qts_bench_throughput PRIVATE qts::core)
    add_test(NAME qts_bench_throughput COMMAND qts_bench_throughput)

    # PostgreSQL conformance (ADR-038): only in PG-gated builds, needs a
    # live server (QTS_PG_TEST_CONNINFO); unset means skip, never fail.
    if(QTS_ENABLE_PG)
        add_executable(qts_test_pg tests/integration/pg_store_test.cpp)
        target_link_libraries(qts_test_pg PRIVATE qts::core)
        add_test(NAME qts_test_pg COMMAND qts_test_pg)
    endif()
endif()

# Vendor-gated adapters (M7-2b/c, ADR-033). qts_core above never sees a
# vendor header; the CI lint jobs only walk src/ and apps/, and this block
# never runs in CI, so the no-SDK environments stay green by construction.
if(QTS_ENABLE_VENDOR_SDKS)
  if(QTS_VENDOR_DIR STREQUAL "")
    message(FATAL_ERROR "QTS_ENABLE_VENDOR_SDKS=ON requires -DQTS_VENDOR_DIR=<sdk root>")
  endif()
  add_subdirectory(adapters)
endif()
