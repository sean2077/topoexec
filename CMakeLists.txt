cmake_minimum_required(VERSION 3.20)
project(topoexec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TOPOEXEC_SCHEMA_VERSION 1)
set(TOPOEXEC_SEMANTIC_CONTRACT_VERSION "0.2")

option(TOPOEXEC_BUILD_TESTING "Build TopoExec tests" ON)
option(TOPOEXEC_BUILD_YAML "Build YAML graph loading target" ON)
option(TOPOEXEC_BUILD_CLI "Build topoexec CLI" ON)
option(TOPOEXEC_BUILD_EXAMPLES "Build runnable example applications" ON)
option(TOPOEXEC_BUILD_FUZZERS "Build optional graph input fuzz targets" OFF)
option(TOPOEXEC_BUILD_OTEL_ADAPTER "Build optional OTel exporter preview target" OFF)
set(TOPOEXEC_FUZZER_ENGINE "AUTO" CACHE STRING "Fuzzer engine: AUTO, LIBFUZZER, or STANDALONE")
set_property(CACHE TOPOEXEC_FUZZER_ENGINE PROPERTY STRINGS AUTO LIBFUZZER STANDALONE)
option(TOPOEXEC_ENABLE_ASAN "Build TopoExec with AddressSanitizer instrumentation" OFF)
option(TOPOEXEC_ENABLE_UBSAN "Build TopoExec with UndefinedBehaviorSanitizer instrumentation" OFF)
option(TOPOEXEC_ENABLE_TSAN "Build TopoExec with ThreadSanitizer instrumentation" OFF)

set(TOPOEXEC_GIT_COMMIT_VALUE "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE TOPOEXEC_GIT_COMMIT_RESULT
    OUTPUT_VARIABLE TOPOEXEC_GIT_COMMIT_OUTPUT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(TOPOEXEC_GIT_COMMIT_RESULT EQUAL 0 AND NOT TOPOEXEC_GIT_COMMIT_OUTPUT STREQUAL "")
    set(TOPOEXEC_GIT_COMMIT_VALUE "${TOPOEXEC_GIT_COMMIT_OUTPUT}")
  endif()
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  add_custom_target(topoexec_format_check
    COMMAND ${CMAKE_COMMAND} -E env CLANG_FORMAT=${CLANG_FORMAT_EXE}
      /usr/bin/env bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/format_check.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking C++ formatting with clang-format"
  )
else()
  add_custom_target(topoexec_format_check
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found; install clang-format or set CLANG_FORMAT to run the optional format check"
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(TOPOEXEC_ENABLE_ASAN OR TOPOEXEC_ENABLE_UBSAN OR TOPOEXEC_ENABLE_TSAN)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "TopoExec sanitizer options require GCC or Clang")
  endif()
  if(TOPOEXEC_ENABLE_TSAN AND (TOPOEXEC_ENABLE_ASAN OR TOPOEXEC_ENABLE_UBSAN))
    message(FATAL_ERROR "ThreadSanitizer cannot be combined with ASAN/UBSAN in this build")
  endif()

  set(TOPOEXEC_SANITIZER_LIST)
  if(TOPOEXEC_ENABLE_ASAN)
    list(APPEND TOPOEXEC_SANITIZER_LIST address)
  endif()
  if(TOPOEXEC_ENABLE_UBSAN)
    list(APPEND TOPOEXEC_SANITIZER_LIST undefined)
  endif()
  if(TOPOEXEC_ENABLE_TSAN)
    list(APPEND TOPOEXEC_SANITIZER_LIST thread)
  endif()
  list(JOIN TOPOEXEC_SANITIZER_LIST "," TOPOEXEC_SANITIZER_FLAGS)
  add_compile_options("-fsanitize=${TOPOEXEC_SANITIZER_FLAGS}" -fno-omit-frame-pointer)
  add_link_options("-fsanitize=${TOPOEXEC_SANITIZER_FLAGS}")
endif()

find_package(nlohmann_json REQUIRED)
if(TOPOEXEC_BUILD_YAML)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(YAML_CPP REQUIRED IMPORTED_TARGET yaml-cpp)
endif()
if(TOPOEXEC_BUILD_CLI)
  if(NOT TOPOEXEC_BUILD_YAML)
    message(FATAL_ERROR "TOPOEXEC_BUILD_CLI=ON requires TOPOEXEC_BUILD_YAML=ON")
  endif()
  find_package(CLI11 REQUIRED)
endif()
if(TOPOEXEC_BUILD_EXAMPLES AND NOT TOPOEXEC_BUILD_YAML)
  message(FATAL_ERROR "TOPOEXEC_BUILD_EXAMPLES=ON requires TOPOEXEC_BUILD_YAML=ON")
endif()
if(TOPOEXEC_BUILD_FUZZERS AND NOT TOPOEXEC_BUILD_YAML)
  message(FATAL_ERROR "TOPOEXEC_BUILD_FUZZERS=ON requires TOPOEXEC_BUILD_YAML=ON")
endif()
include(CMakePackageConfigHelpers)

add_library(topoexec_core INTERFACE)
set_target_properties(topoexec_core PROPERTIES EXPORT_NAME core)
target_include_directories(topoexec_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(topoexec_core INTERFACE cxx_std_20)
add_library(topoexec::core ALIAS topoexec_core)

add_library(topoexec_runtime
  src/buffer.cpp
  src/channel.cpp
  src/clock.cpp
  src/component.cpp
  src/component_registry.cpp
  src/diagnostics.cpp
  src/event_runtime.cpp
  src/graph.cpp
  src/logging.cpp
  src/metric_schema.cpp
  src/metrics.cpp
  src/payload.cpp
  src/runtime_runner.cpp
  src/scheduler.cpp
  src/state.cpp
  src/task_executor.cpp
  src/trace.cpp
  src/trigger_policy.cpp
)
set_target_properties(topoexec_runtime PROPERTIES
  EXPORT_NAME runtime
)
target_link_libraries(topoexec_runtime PUBLIC topoexec_core)

add_library(topoexec::runtime ALIAS topoexec_runtime)

add_library(topoexec_adapter_sdk INTERFACE)
set_target_properties(topoexec_adapter_sdk PROPERTIES
  EXPORT_NAME adapter_sdk
)
target_link_libraries(topoexec_adapter_sdk INTERFACE topoexec_runtime)
add_library(topoexec::adapter_sdk ALIAS topoexec_adapter_sdk)

if(TOPOEXEC_BUILD_OTEL_ADAPTER)
  add_library(topoexec_adapters_otel INTERFACE)
  set_target_properties(topoexec_adapters_otel PROPERTIES
    EXPORT_NAME otel
  )
  target_link_libraries(topoexec_adapters_otel INTERFACE topoexec_adapter_sdk)
  add_library(topoexec_adapters::otel ALIAS topoexec_adapters_otel)
endif()

set(TOPOEXEC_INSTALL_TARGETS topoexec_core topoexec_runtime topoexec_adapter_sdk)

if(TOPOEXEC_BUILD_YAML)
  add_library(topoexec_yaml
    src/graph_io.cpp
  )
  set_target_properties(topoexec_yaml PROPERTIES
    OUTPUT_NAME topoexec
    EXPORT_NAME yaml
  )
  target_link_libraries(topoexec_yaml
    PUBLIC topoexec_runtime
    PRIVATE PkgConfig::YAML_CPP nlohmann_json::nlohmann_json
  )
  add_library(topoexec::yaml ALIAS topoexec_yaml)
  add_library(topoexec ALIAS topoexec_yaml)
  list(APPEND TOPOEXEC_INSTALL_TARGETS topoexec_yaml)
else()
  add_library(topoexec ALIAS topoexec_runtime)
endif()

if(TOPOEXEC_BUILD_CLI)
  add_executable(topoexec_cli tools/topoexec/main.cpp)
  target_link_libraries(topoexec_cli PRIVATE topoexec_yaml CLI11::CLI11 nlohmann_json::nlohmann_json)
  if(CMAKE_BUILD_TYPE)
    target_compile_definitions(topoexec_cli PRIVATE TOPOEXEC_CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
  else()
    target_compile_definitions(topoexec_cli PRIVATE TOPOEXEC_CMAKE_BUILD_TYPE="unknown")
  endif()
  target_compile_definitions(topoexec_cli PRIVATE TOPOEXEC_GIT_COMMIT_DEFAULT="${TOPOEXEC_GIT_COMMIT_VALUE}")
  set_target_properties(topoexec_cli PROPERTIES OUTPUT_NAME topoexec)
  list(APPEND TOPOEXEC_INSTALL_TARGETS topoexec_cli)
endif()

if(TOPOEXEC_BUILD_EXAMPLES)
  add_executable(topoexec_app_minimal_pipeline examples/apps/minimal_pipeline/main.cpp)
  target_link_libraries(topoexec_app_minimal_pipeline PRIVATE topoexec)

  add_executable(topoexec_app_overload_latest_vs_queue examples/apps/overload_latest_vs_queue/main.cpp)
  target_link_libraries(topoexec_app_overload_latest_vs_queue PRIVATE topoexec_runtime)

  add_executable(topoexec_app_control_feedback_delay examples/apps/control_feedback_delay/main.cpp)
  target_link_libraries(topoexec_app_control_feedback_delay PRIVATE topoexec)

  add_executable(topoexec_app_composite_loop_fixed_point examples/apps/composite_loop_fixed_point/main.cpp)
  target_link_libraries(topoexec_app_composite_loop_fixed_point PRIVATE topoexec)

  add_executable(topoexec_app_async_worker examples/apps/async_worker/main.cpp)
  target_link_libraries(topoexec_app_async_worker PRIVATE topoexec)

  add_executable(topoexec_app_cpp_builder_minimal examples/apps/cpp_builder_minimal/main.cpp)
  target_link_libraries(topoexec_app_cpp_builder_minimal PRIVATE topoexec_runtime)

  add_executable(topoexec_app_low_latency_sensor_pipeline examples/apps/low_latency_sensor_pipeline/main.cpp)
  target_link_libraries(topoexec_app_low_latency_sensor_pipeline PRIVATE topoexec_runtime)

  add_executable(topoexec_app_control_loop_with_state examples/apps/control_loop_with_state/main.cpp)
  target_link_libraries(topoexec_app_control_loop_with_state PRIVATE topoexec)

  add_executable(topoexec_app_async_request_response examples/apps/async_request_response/main.cpp)
  target_link_libraries(topoexec_app_async_request_response PRIVATE topoexec_runtime)

  add_executable(topoexec_app_composite_solver examples/apps/composite_solver/main.cpp)
  target_link_libraries(topoexec_app_composite_solver PRIVATE topoexec)

  add_executable(topoexec_app_payload_pool_pipeline examples/apps/payload_pool_pipeline/main.cpp)
  target_link_libraries(topoexec_app_payload_pool_pipeline PRIVATE topoexec_runtime)
  add_executable(topoexec_app_robot_cell_pilot examples/apps/robot_cell_pilot/main.cpp)
  target_link_libraries(topoexec_app_robot_cell_pilot PRIVATE topoexec_runtime)
endif()

if(TOPOEXEC_BUILD_FUZZERS)
  string(TOUPPER "${TOPOEXEC_FUZZER_ENGINE}" TOPOEXEC_FUZZER_ENGINE_NORMALIZED)
  set(TOPOEXEC_FUZZER_ENGINES AUTO LIBFUZZER STANDALONE)
  if(NOT TOPOEXEC_FUZZER_ENGINE_NORMALIZED IN_LIST TOPOEXEC_FUZZER_ENGINES)
    message(FATAL_ERROR "TOPOEXEC_FUZZER_ENGINE must be AUTO, LIBFUZZER, or STANDALONE")
  endif()
  if(TOPOEXEC_FUZZER_ENGINE_NORMALIZED STREQUAL "AUTO")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      set(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE LIBFUZZER)
    else()
      set(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE STANDALONE)
    endif()
  else()
    set(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE "${TOPOEXEC_FUZZER_ENGINE_NORMALIZED}")
  endif()
  if(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE STREQUAL "LIBFUZZER" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "TOPOEXEC_FUZZER_ENGINE=LIBFUZZER requires Clang")
  endif()

  add_executable(fuzz_graph_inputs tests/fuzz/fuzz_graph_inputs.cpp)
  target_link_libraries(fuzz_graph_inputs PRIVATE topoexec_yaml)
  if(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE STREQUAL "LIBFUZZER")
    target_compile_options(fuzz_graph_inputs PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(fuzz_graph_inputs PRIVATE -fsanitize=fuzzer,address,undefined)
  else()
    target_compile_definitions(fuzz_graph_inputs PRIVATE TOPOEXEC_STANDALONE_FUZZER=1)
  endif()
endif()

install(TARGETS ${TOPOEXEC_INSTALL_TARGETS}
  EXPORT topoexecTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT topoexecTargets
  FILE topoexecTargets.cmake
  NAMESPACE topoexec::
  DESTINATION lib/cmake/topoexec
)
if(TOPOEXEC_BUILD_OTEL_ADAPTER)
  install(TARGETS topoexec_adapters_otel
    EXPORT topoexecAdapterTargets
    INCLUDES DESTINATION include
  )
  install(EXPORT topoexecAdapterTargets
    FILE topoexecAdapterTargets.cmake
    NAMESPACE topoexec_adapters::
    DESTINATION lib/cmake/topoexec
  )
endif()
configure_package_config_file(
  cmake/topoexecConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/topoexecConfig.cmake
  INSTALL_DESTINATION lib/cmake/topoexec
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/topoexecConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/topoexecConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/topoexecConfigVersion.cmake
  DESTINATION lib/cmake/topoexec
)
install(FILES schema/topoexec.schema.v1.json DESTINATION share/topoexec/schema)

set(CPACK_PACKAGE_NAME "topoexec")
set(CPACK_PACKAGE_VENDOR "TopoExec")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Small embeddable C++20 semantic runtime")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/sean2077/topoexec")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  "/\\\\.git/"
  "/build/"
  "/build-[^/]+/"
  "/dist/"
  "/compile_commands\\\\.json$"
  "/benchmarks/local-baseline[^/]*\\\\.json$"
)
include(CPack)

include(CTest)
if(TOPOEXEC_BUILD_TESTING AND BUILD_TESTING)
  if(NOT TOPOEXEC_BUILD_YAML OR NOT TOPOEXEC_BUILD_CLI OR NOT TOPOEXEC_BUILD_EXAMPLES)
    message(FATAL_ERROR "TOPOEXEC_BUILD_TESTING=ON currently requires YAML, CLI, and examples; disable testing for runtime-only option smokes")
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  function(topoexec_add_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE topoexec GTest::gtest_main)
    add_test(NAME ${target} COMMAND ${target})
  endfunction()

  topoexec_add_test(test_common tests/test_common.cpp)
  topoexec_add_test(test_graph tests/test_graph.cpp)
  topoexec_add_test(test_channel tests/test_channel.cpp)
  topoexec_add_test(test_state tests/test_state.cpp)
  topoexec_add_test(test_runtime tests/test_runtime.cpp)
  topoexec_add_test(test_stress tests/test_stress.cpp)

  add_executable(test_adapter_sdk tests/test_adapter_sdk.cpp)
  target_link_libraries(test_adapter_sdk PRIVATE topoexec_adapter_sdk GTest::gtest_main)
  add_test(NAME test_adapter_sdk COMMAND test_adapter_sdk)

  if(TOPOEXEC_BUILD_OTEL_ADAPTER)
    add_executable(test_otel_adapter tests/test_otel_adapter.cpp)
    target_link_libraries(test_otel_adapter PRIVATE topoexec_adapters_otel GTest::gtest_main)
    add_test(NAME test_otel_adapter COMMAND test_otel_adapter)
  endif()

  add_executable(topoexec_bench_task_executor benchmarks/task_executor.cpp)
  target_link_libraries(topoexec_bench_task_executor PRIVATE topoexec_runtime)

  add_test(NAME cli_validate_minimal
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml
  )
  add_test(NAME cli_validate_control_feedback_delay
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_feedback_delay.yaml
  )
  add_test(NAME cli_validate_state_config_snapshot
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/state_config_snapshot.yaml
  )
  add_test(NAME cli_validate_batch_time_sync
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/batch_time_sync.yaml
  )
  add_test(NAME cli_validate_service_pipeline
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/service_pipeline.yaml
  )
  add_test(NAME cli_validate_boundary_adapter_pattern
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/boundary_adapter_pattern.yaml
  )
  add_test(NAME cli_validate_template_source_transform_sink
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/template_source_transform_sink.yaml
  )
  add_test(NAME cli_validate_schema_only_minimal
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --schema-only --format json
  )
  set_tests_properties(cli_validate_schema_only_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"ok\": true")
  add_test(NAME cli_validate_input_limit_override_fails_safely
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/check_parser_limits.py
      --topoexec $<TARGET_FILE:topoexec_cli>
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(NAME cli_validate_semantic_minimal
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --semantic --format json
  )
  set_tests_properties(cli_validate_semantic_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"region_order\"")
  add_test(NAME cli_validate_warning_diagnostics_do_not_fail
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/diagnostic_warnings.yaml --format json
  )
  set_tests_properties(cli_validate_warning_diagnostics_do_not_fail PROPERTIES PASS_REGULAR_EXPRESSION "\"high_queue_depth_latency_risk\"")
  add_test(NAME cli_validate_strict_diagnostics_fail_warnings
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/diagnostic_warnings.yaml --strict-diagnostics --format json
  )
  set_tests_properties(cli_validate_strict_diagnostics_fail_warnings PROPERTIES
    WILL_FAIL TRUE)
  add_test(NAME cli_render_mermaid
    COMMAND topoexec graph render ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --format mermaid
  )
  add_test(NAME cli_plan_json_composite
    COMMAND topoexec graph plan ${CMAKE_CURRENT_SOURCE_DIR}/examples/composite_loop.yaml --format json
  )
  add_test(NAME cli_run_minimal
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1
  )
  set_tests_properties(cli_run_minimal PROPERTIES PASS_REGULAR_EXPRESSION "runtime_publication_committed: 2")
  add_test(NAME cli_run_state_config_snapshot
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/state_config_snapshot.yaml --steps 2
  )
  set_tests_properties(cli_run_state_config_snapshot PROPERTIES PASS_REGULAR_EXPRESSION "order: config_source,config_source,consumer")
  add_test(NAME cli_run_batch_time_sync
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/batch_time_sync.yaml --steps 1
  )
  set_tests_properties(cli_run_batch_time_sync PROPERTIES PASS_REGULAR_EXPRESSION "order: left_sensor,right_sensor,sync_join,sink")
  add_test(NAME cli_run_service_pipeline
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/service_pipeline.yaml --steps 2
  )
  set_tests_properties(cli_run_service_pipeline PROPERTIES PASS_REGULAR_EXPRESSION "order: client_boundary,validator,client_boundary,worker_result,validator,response_boundary")
  add_test(NAME cli_run_boundary_adapter_pattern
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/boundary_adapter_pattern.yaml --steps 1
  )
  set_tests_properties(cli_run_boundary_adapter_pattern PROPERTIES PASS_REGULAR_EXPRESSION "order: external_input,app_logic,external_output")
  add_test(NAME cli_run_template_source_transform_sink
    COMMAND topoexec graph run ${CMAKE_CURRENT_SOURCE_DIR}/examples/template_source_transform_sink.yaml --steps 1
  )
  set_tests_properties(cli_run_template_source_transform_sink PROPERTIES PASS_REGULAR_EXPRESSION "order: demo.source,demo.transform,demo.sink")
  add_test(NAME cli_metrics_json_minimal
    COMMAND topoexec graph metrics ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --format json
  )
  set_tests_properties(cli_metrics_json_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"runtime.component.execution_count\"")
  add_test(NAME cli_trace_minimal
    COMMAND topoexec graph trace ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1
  )
  set_tests_properties(cli_trace_minimal PROPERTIES PASS_REGULAR_EXPRESSION "channel_publish")
  add_test(NAME cli_trace_json_structured_minimal
    COMMAND topoexec graph trace ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --format json
  )
  set_tests_properties(cli_trace_json_structured_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"edge_kind\"")
  add_test(NAME cli_trace_chrome_minimal
    COMMAND topoexec graph trace ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --format chrome
  )
  set_tests_properties(cli_trace_chrome_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"traceEvents\"")
  add_test(NAME cli_lint_control_feedback_delay
    COMMAND topoexec graph lint ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_feedback_delay.yaml
  )
  set_tests_properties(cli_lint_control_feedback_delay PROPERTIES PASS_REGULAR_EXPRESSION "delay_epoch_boundary")
  add_test(NAME cli_lint_large_payload_copy
    COMMAND topoexec graph lint ${CMAKE_CURRENT_SOURCE_DIR}/examples/large_payload_copy.yaml
  )
  set_tests_properties(cli_lint_large_payload_copy PROPERTIES PASS_REGULAR_EXPRESSION "large_payload_copy")
  add_test(NAME cli_lint_reject_move_only_multireader
    COMMAND ${Python3_EXECUTABLE} -c
      "import subprocess, sys; p = subprocess.run([sys.argv[1], 'graph', 'lint', sys.argv[2]], text=True, capture_output=True); out = p.stdout + p.stderr; sys.stdout.write(out); sys.exit(0 if p.returncode != 0 and 'invalid_move_only_multireader' in out else 1)"
      $<TARGET_FILE:topoexec_cli>
      ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_move_only_multireader.yaml
  )
  add_test(NAME cli_lint_loaned_view_without_pool_owner
    COMMAND topoexec graph lint ${CMAKE_CURRENT_SOURCE_DIR}/examples/loaned_view_without_pool_owner.yaml
  )
  set_tests_properties(cli_lint_loaned_view_without_pool_owner PROPERTIES
    PASS_REGULAR_EXPRESSION "loaned_view_without_pool_owner"
  )
  add_test(NAME cli_explain_minimal
    COMMAND topoexec graph explain ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml
  )
  set_tests_properties(cli_explain_minimal PROPERTIES PASS_REGULAR_EXPRESSION "publish: staged by runtime")
  add_test(NAME cli_diff_plan_minimal_vs_delay
    COMMAND topoexec graph diff-plan ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_feedback_delay.yaml
  )
  set_tests_properties(cli_diff_plan_minimal_vs_delay PROPERTIES PASS_REGULAR_EXPRESSION "same_region_order: false")
  add_test(NAME cli_bench_minimal
    COMMAND topoexec graph bench ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --runs 2
  )
  set_tests_properties(cli_bench_minimal PROPERTIES PASS_REGULAR_EXPRESSION "runs: 2")
  add_test(NAME cli_bench_json_minimal
    COMMAND topoexec graph bench ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --runs 2 --format json
  )
  set_tests_properties(cli_bench_json_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"elapsed_ms\"")
  add_test(NAME cli_bench_json_immediate_chain
    COMMAND topoexec graph bench ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/immediate_chain.yaml --steps 2 --runs 2 --format json
  )
  set_tests_properties(cli_bench_json_immediate_chain PROPERTIES PASS_REGULAR_EXPRESSION "\"throughput_tick_calls_per_sec\"")
  add_test(NAME bench_task_executor_smoke
    COMMAND topoexec_bench_task_executor --tasks 16 --runs 2 --format json
  )
  set_tests_properties(bench_task_executor_smoke PROPERTIES PASS_REGULAR_EXPRESSION "\"case\": \"task_executor\"")
  add_test(NAME bench_contract_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/bench/check_bench_contract.py
      --topoexec $<TARGET_FILE:topoexec_cli>
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
      --task-executor-bench $<TARGET_FILE:topoexec_bench_task_executor>
  )
  add_test(NAME cli_golden_outputs
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/check_cli_golden.py
      --topoexec $<TARGET_FILE:topoexec_cli>
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
      --golden-dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden
  )
  add_test(NAME schema_v1_contract_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/schema/check_schema_contract.py
      --schema ${CMAKE_CURRENT_SOURCE_DIR}/schema/topoexec.schema.v1.json
      --topoexec $<TARGET_FILE:topoexec_cli>
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(NAME docs_command_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/docs/check_docs.py
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
      --build-dir ${CMAKE_CURRENT_BINARY_DIR}
      --topoexec $<TARGET_FILE:topoexec_cli>
  )
  add_test(NAME fuzz_graph_input_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/fuzz_graph_inputs.py
      --topoexec $<TARGET_FILE:topoexec_cli>
  )
  add_test(NAME stress_graph_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/stress/check_stress_workloads.py
      --topoexec $<TARGET_FILE:topoexec_cli>
      --profile smoke
      --scale 12
      --steps 4
  )
  if(TARGET fuzz_graph_inputs)
    if(TOPOEXEC_FUZZER_ENGINE_EFFECTIVE STREQUAL "LIBFUZZER")
      add_test(NAME fuzz_graph_input_target_smoke
        COMMAND fuzz_graph_inputs -runs=64 -max_len=1048576
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus/graph_inputs
      )
    else()
      add_test(NAME fuzz_graph_input_target_smoke
        COMMAND fuzz_graph_inputs ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus/graph_inputs
      )
    endif()
  endif()
  add_test(NAME policy_no_core_adapter_deps
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/policy/check_no_adapter_deps.py
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(NAME policy_architecture_self_test
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/policy/check_no_adapter_deps.py
      --self-test
  )
  add_test(NAME cli_doctor_json
    COMMAND $<TARGET_FILE:topoexec_cli> doctor --format json
  )
  set_tests_properties(cli_doctor_json PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PASS_REGULAR_EXPRESSION "\"semantic_contract_version\""
  )
  add_test(NAME cli_schema_dump_json
    COMMAND $<TARGET_FILE:topoexec_cli> schema dump --format json
  )
  set_tests_properties(cli_schema_dump_json PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PASS_REGULAR_EXPRESSION "\"x-topoexec-semantic_contract_version\""
  )
  add_test(NAME cli_schema_check_minimal_json
    COMMAND $<TARGET_FILE:topoexec_cli> schema check examples/minimal.yaml --format json
  )
  set_tests_properties(cli_schema_check_minimal_json PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PASS_REGULAR_EXPRESSION "\"ok\": true"
  )
  add_test(NAME cli_reject_invalid_unknown_field
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_unknown_field.yaml
  )
  set_tests_properties(cli_reject_invalid_unknown_field PROPERTIES WILL_FAIL TRUE)
  add_test(NAME cli_reject_missing_edge_kind
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_missing_edge_kind.yaml
  )
  set_tests_properties(cli_reject_missing_edge_kind PROPERTIES WILL_FAIL TRUE)
  add_test(NAME cli_reject_immediate_cycle_without_loop
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_immediate_cycle.yaml
  )
  set_tests_properties(cli_reject_immediate_cycle_without_loop PROPERTIES WILL_FAIL TRUE)
  add_test(NAME cmake_package_runtime_smoke
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DSANITIZER_FLAGS=${TOPOEXEC_SANITIZER_FLAGS}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/package_smoke.cmake
  )
  add_test(NAME cmake_runtime_only_options_smoke
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/runtime_only_options_smoke.cmake
  )
  add_test(NAME cmake_otel_adapter_options_smoke
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/otel_adapter_options_smoke.cmake
  )
  add_test(NAME cmake_cpack_smoke
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DCPACK_COMMAND=${CMAKE_CPACK_COMMAND}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/cpack_smoke.cmake
  )
  add_test(NAME package_draft_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/package/check_package_drafts.py
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(NAME release_prepare_smoke
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/release/check_release_prepare.py
      --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
      --build-dir ${CMAKE_CURRENT_BINARY_DIR}
  )
  add_test(NAME app_minimal_pipeline_runs
    COMMAND topoexec_app_minimal_pipeline
  )
  add_test(NAME app_overload_latest_vs_queue_runs
    COMMAND topoexec_app_overload_latest_vs_queue
  )
  set_tests_properties(app_overload_latest_vs_queue_runs PROPERTIES PASS_REGULAR_EXPRESSION "queue_drop_count=1")
  add_test(NAME app_control_feedback_delay_runs
    COMMAND topoexec_app_control_feedback_delay
  )
  set_tests_properties(app_control_feedback_delay_runs PROPERTIES PASS_REGULAR_EXPRESSION "delayed_correction_epoch=2")
  add_test(NAME app_composite_loop_fixed_point_runs
    COMMAND topoexec_app_composite_loop_fixed_point
  )
  set_tests_properties(app_composite_loop_fixed_point_runs PROPERTIES PASS_REGULAR_EXPRESSION "loop_iteration_count=2")
  add_test(NAME app_async_worker_runs
    COMMAND topoexec_app_async_worker
  )
  set_tests_properties(app_async_worker_runs PROPERTIES PASS_REGULAR_EXPRESSION "async_drop_count=1")
  add_test(NAME app_cpp_builder_minimal_runs
    COMMAND topoexec_app_cpp_builder_minimal
  )
  set_tests_properties(app_cpp_builder_minimal_runs PROPERTIES PASS_REGULAR_EXPRESSION "builder_sink=hello:built")
  add_test(NAME app_low_latency_sensor_pipeline_runs
    COMMAND topoexec_app_low_latency_sensor_pipeline
  )
  set_tests_properties(app_low_latency_sensor_pipeline_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "tracker_latest=track:detection:preprocessed:frame-3")
  add_test(NAME app_control_loop_with_state_runs
    COMMAND topoexec_app_control_loop_with_state
  )
  set_tests_properties(app_control_loop_with_state_runs PROPERTIES PASS_REGULAR_EXPRESSION "state_snapshot_epoch=2")
  add_test(NAME app_async_request_response_runs
    COMMAND topoexec_app_async_request_response
  )
  set_tests_properties(app_async_request_response_runs PROPERTIES PASS_REGULAR_EXPRESSION "response_payload=accepted:req-1")
  add_test(NAME app_composite_solver_runs
    COMMAND topoexec_app_composite_solver
  )
  set_tests_properties(app_composite_solver_runs PROPERTIES PASS_REGULAR_EXPRESSION "budget_overrun_count=1")
  add_test(NAME app_payload_pool_pipeline_runs
    COMMAND topoexec_app_payload_pool_pipeline
  )
  set_tests_properties(app_payload_pool_pipeline_runs PROPERTIES PASS_REGULAR_EXPRESSION "loaned_address_preserved=true")
  add_test(NAME app_robot_cell_pilot_runs
    COMMAND topoexec_app_robot_cell_pilot
  )
  set_tests_properties(app_robot_cell_pilot_runs PROPERTIES
    PASS_REGULAR_EXPRESSION "pilot_value=explicit_feedback_bounded_observable_cpp")
endif()
