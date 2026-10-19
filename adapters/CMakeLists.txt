# Vendor SDK adapters (M7-2b/c, ADR-033).
#
# Reached only when the root gate is open:
#   cmake -DSTS_ENABLE_VENDOR_SDKS=ON [-DSTS_VENDOR_DIR=<root>]
# with <root> laid out as (default: the in-repo ./vendor tree, ADR-040;
# manifest in vendor/README.md):
#   <root>/ama/include/ama.h          <root>/ama/lib/Lnx64/libama.so
#   <root>/quantapi/include/...       <root>/quantapi/lib/...   (M7-2c)
#
# Invariants this file upholds:
#  - sts_core stays vendor-free: adapters link sts::core one-way (ADR-027);
#  - CI never enters this block (gate defaults OFF), so its lint job —
#    which only walks src/ and apps/ — cannot trip over missing SDKs;
#  - binaries get BUILD_RPATH into the SDK dir so the Linux host runs them without
#    exporting LD_LIBRARY_PATH by hand (the demo script's manual export is
#    the alternative; both resolve libama.so's own NEEDED entries).

if(NOT EXISTS "${STS_VENDOR_DIR}/ama/include/ama.h")
  message(FATAL_ERROR "STS_VENDOR_DIR must contain ama/include/ama.h (got: '${STS_VENDOR_DIR}'). Run tools/sts.sh doctor for the exact missing-file list and fixes (manifest: vendor/README.md).")
endif()
set(sts_ama_lib_dir "${STS_VENDOR_DIR}/ama/lib/Lnx64")
if(NOT EXISTS "${sts_ama_lib_dir}/libama.so")
  message(FATAL_ERROR "AMA library not found: ${sts_ama_lib_dir}/libama.so. Run tools/sts.sh doctor for the exact missing-file list and fixes.")
endif()

add_library(sts_ama_sdk SHARED IMPORTED)
set_target_properties(sts_ama_sdk PROPERTIES IMPORTED_LOCATION "${sts_ama_lib_dir}/libama.so")

add_library(sts_adapter_ama STATIC ama/ama_quote_adapter.cpp)
target_include_directories(sts_adapter_ama
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ama" "${STS_VENDOR_DIR}/ama/include")
target_link_libraries(sts_adapter_ama PUBLIC sts::core sts_ama_sdk)
# libama.so's DT_NEEDED entries sit in the same directory. The default new
# dtags emit DT_RUNPATH, which does NOT apply transitively to libama.so's
# own dependencies (librum/librmm/...); --disable-new-dtags emits DT_RPATH
# instead, which does, so binaries run without exporting LD_LIBRARY_PATH.
target_link_options(sts_adapter_ama INTERFACE
  "LINKER:-rpath-link,${sts_ama_lib_dir}"
  "LINKER:--disable-new-dtags")

add_executable(sts_ama_readonly apps/ama_readonly_main.cpp)
target_link_libraries(sts_ama_readonly PRIVATE sts_adapter_ama)
set_target_properties(sts_ama_readonly PROPERTIES BUILD_RPATH "${sts_ama_lib_dir}")

if(STS_BUILD_TESTS)
  add_executable(sts_test_ama_adapter tests/ama_adapter_test.cpp)
  target_link_libraries(sts_test_ama_adapter PRIVATE sts_adapter_ama)
  set_target_properties(sts_test_ama_adapter PROPERTIES BUILD_RPATH "${sts_ama_lib_dir}")
  add_test(NAME sts_test_ama_adapter COMMAND sts_test_ama_adapter)
endif()

# --- QuantAPI trade adapter (M7-2c) -----------------------------------------
# Layout: <root>/quantapi/include/atp_quant_api.h, <root>/quantapi/lib/
# libatpquantapi.so. Unlike libama.so its DT_NEEDED entries are system
# libraries only (the TCP stack needs no rudplib), so no -rpath-link and no
# --disable-new-dtags: BUILD_RPATH into lib/ suffices for direct execution.
if(NOT EXISTS "${STS_VENDOR_DIR}/quantapi/include/atp_quant_api.h")
  message(FATAL_ERROR "STS_VENDOR_DIR must contain quantapi/include/atp_quant_api.h (got: '${STS_VENDOR_DIR}'). Run tools/sts.sh doctor for the exact missing-file list and fixes.")
endif()
set(sts_quantapi_lib_dir "${STS_VENDOR_DIR}/quantapi/lib")
if(NOT EXISTS "${sts_quantapi_lib_dir}/libatpquantapi.so")
  message(FATAL_ERROR "QuantAPI library not found: ${sts_quantapi_lib_dir}/libatpquantapi.so. Run tools/sts.sh doctor for the exact missing-file list and fixes.")
endif()

add_library(sts_quantapi_sdk SHARED IMPORTED)
set_target_properties(sts_quantapi_sdk PROPERTIES IMPORTED_LOCATION "${sts_quantapi_lib_dir}/libatpquantapi.so")

add_library(sts_adapter_quantapi STATIC quantapi/quantapi_trade_adapter.cpp)
target_include_directories(sts_adapter_quantapi
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/quantapi")
# SYSTEM: the vendor header defines no-op virtuals with named parameters
# that trip -Wunused-parameter in consumer builds; SDK code stays warning-
# free territory (we compile only our side with the project flags).
target_include_directories(sts_adapter_quantapi SYSTEM
  PUBLIC "${STS_VENDOR_DIR}/quantapi/include")
target_link_libraries(sts_adapter_quantapi PUBLIC sts::core sts_quantapi_sdk)

add_executable(sts_quantapi_readonly apps/quantapi_readonly_main.cpp)
target_link_libraries(sts_quantapi_readonly PRIVATE sts_adapter_quantapi)
set_target_properties(sts_quantapi_readonly PROPERTIES BUILD_RPATH "${sts_quantapi_lib_dir}")

if(STS_BUILD_TESTS)
  add_executable(sts_test_quantapi_adapter tests/quantapi_adapter_test.cpp)
  target_link_libraries(sts_test_quantapi_adapter PRIVATE sts_adapter_quantapi)
  set_target_properties(sts_test_quantapi_adapter PROPERTIES BUILD_RPATH "${sts_quantapi_lib_dir}")
  add_test(NAME sts_test_quantapi_adapter COMMAND sts_test_quantapi_adapter)
endif()

# --- Server vendor wiring (M7-2 server milestone) ---------------------------
# sts_server is defined at top level; in vendor builds it links both adapter
# static libs (vendor .so arrives transitively) and compiles its ifdef'd
# session/feed paths. Default builds never enter this file, so they stay
# paper-only by construction (unknown broker names fail closed).
target_link_libraries(sts_server PRIVATE sts_adapter_ama sts_adapter_quantapi)
target_compile_definitions(sts_server PRIVATE STS_HAS_VENDOR_ADAPTERS)
# The server binary must resolve both vendor .so at runtime without
# LD_LIBRARY_PATH (same treatment as the readonly tools above).
set_target_properties(sts_server PROPERTIES BUILD_RPATH "${sts_ama_lib_dir};${sts_quantapi_lib_dir}")
