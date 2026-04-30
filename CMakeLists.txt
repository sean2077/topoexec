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
  add_test(NAME cli_render_mermaid
    COMMAND topoexec graph render ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal.yaml --format mermaid
  )
  add_test(NAME cli_plan_json_composite
    COMMAND topoexec graph plan ${CMAKE_CURRENT_SOURCE_DIR}/examples/composite_loop.yaml --format json
  )
  add_test(NAME cli_reject_invalid_unknown_field
    COMMAND topoexec graph validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/invalid_unknown_field.yaml
  )
  set_tests_properties(cli_reject_invalid_unknown_field PROPERTIES WILL_FAIL TRUE)
endif()
