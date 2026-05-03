cmake_minimum_required(VERSION 3.20)
project(topoexec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TOPOEXEC_BUILD_TESTING "Build TopoExec tests" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML_CPP REQUIRED IMPORTED_TARGET yaml-cpp)

find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(topoexec
  src/buffer.cpp
  src/channel.cpp
  src/clock.cpp
  src/component.cpp
  src/component_registry.cpp
  src/event_runtime.cpp
  src/graph.cpp
  src/logging.cpp
  src/metrics.cpp
  src/payload.cpp
  src/runtime_runner.cpp
  src/scheduler.cpp
  src/trace.cpp
  src/trigger_policy.cpp
)
target_include_directories(topoexec PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(topoexec PUBLIC cxx_std_20)
target_link_libraries(topoexec PUBLIC PkgConfig::YAML_CPP nlohmann_json::nlohmann_json)

add_executable(topoexec_cli tools/topoexec/main.cpp)
target_link_libraries(topoexec_cli PRIVATE topoexec CLI11::CLI11 nlohmann_json::nlohmann_json)
set_target_properties(topoexec_cli PROPERTIES OUTPUT_NAME topoexec)

add_executable(topoexec_app_minimal_pipeline examples/apps/minimal_pipeline/main.cpp)
target_link_libraries(topoexec_app_minimal_pipeline PRIVATE topoexec)

add_executable(topoexec_app_overload_latest_vs_queue examples/apps/overload_latest_vs_queue/main.cpp)
target_link_libraries(topoexec_app_overload_latest_vs_queue PRIVATE topoexec)

add_executable(topoexec_app_control_feedback_delay examples/apps/control_feedback_delay/main.cpp)
target_link_libraries(topoexec_app_control_feedback_delay PRIVATE topoexec)

add_executable(topoexec_app_composite_loop_fixed_point examples/apps/composite_loop_fixed_point/main.cpp)
target_link_libraries(topoexec_app_composite_loop_fixed_point PRIVATE topoexec)

add_executable(topoexec_app_async_worker examples/apps/async_worker/main.cpp)
target_link_libraries(topoexec_app_async_worker PRIVATE topoexec)

install(TARGETS topoexec topoexec_cli
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

include(CTest)
if(TOPOEXEC_BUILD_TESTING AND BUILD_TESTING)
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
  topoexec_add_test(test_runtime tests/test_runtime.cpp)

  add_test(NAME cli_validate_minimal
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml
  )
  add_test(NAME cli_validate_control_feedback_delay
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_feedback_delay.yaml
  )
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
  add_test(NAME cli_metrics_json_minimal
    COMMAND topoexec graph metrics ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1 --format json
  )
  set_tests_properties(cli_metrics_json_minimal PROPERTIES PASS_REGULAR_EXPRESSION "\"committed_publication_count\": 2")
  add_test(NAME cli_trace_minimal
    COMMAND topoexec graph trace ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --steps 1
  )
  set_tests_properties(cli_trace_minimal PROPERTIES PASS_REGULAR_EXPRESSION "channel_publish")
  add_test(NAME cli_lint_control_feedback_delay
    COMMAND topoexec graph lint ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_feedback_delay.yaml
  )
  set_tests_properties(cli_lint_control_feedback_delay PROPERTIES PASS_REGULAR_EXPRESSION "delay_epoch_boundary")
  add_test(NAME cli_lint_large_payload_copy
    COMMAND topoexec graph lint ${CMAKE_CURRENT_SOURCE_DIR}/examples/large_payload_copy.yaml
  )
  set_tests_properties(cli_lint_large_payload_copy PROPERTIES PASS_REGULAR_EXPRESSION "large_payload_copy")
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
  add_test(NAME cli_reject_invalid_unknown_field
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_unknown_field.yaml
  )
  set_tests_properties(cli_reject_invalid_unknown_field PROPERTIES WILL_FAIL TRUE)
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
endif()
