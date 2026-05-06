if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

set(SMOKE_ROOT "${BUILD_DIR}/prometheus-adapter-options-smoke")
set(PROMETHEUS_BUILD_DIR "${SMOKE_ROOT}/build")
set(PROMETHEUS_INSTALL_DIR "${SMOKE_ROOT}/install")
set(DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/downstream-build")

file(REMOVE_RECURSE "${SMOKE_ROOT}")
file(MAKE_DIRECTORY "${SMOKE_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${PROMETHEUS_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON
    -DTOPOEXEC_BUILD_YAML=OFF
    -DTOPOEXEC_BUILD_CLI=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_TESTING=OFF
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter option configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${PROMETHEUS_BUILD_DIR}" --target topoexec_runtime -j
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter option runtime build failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${PROMETHEUS_BUILD_DIR}" --prefix "${PROMETHEUS_INSTALL_DIR}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter option install failed")
endif()

if(NOT EXISTS "${PROMETHEUS_INSTALL_DIR}/include/topoexec/adapters/prometheus.hpp")
  message(FATAL_ERROR "Prometheus adapter install did not include preview header")
endif()
if(NOT EXISTS "${PROMETHEUS_INSTALL_DIR}/lib/cmake/topoexec/topoexecAdapterTargets.cmake")
  message(FATAL_ERROR "Prometheus adapter install did not export adapter targets")
endif()
if(EXISTS "${PROMETHEUS_INSTALL_DIR}/bin/topoexec")
  message(FATAL_ERROR "Prometheus adapter runtime-only install unexpectedly included the CLI")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/prometheus_adapter_smoke"
    -B "${DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${PROMETHEUS_INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  RESULT_VARIABLE downstream_configure_result
)
if(NOT downstream_configure_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter downstream configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE downstream_build_result
)
if(NOT downstream_build_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter downstream build failed")
endif()

execute_process(
  COMMAND "${DOWNSTREAM_BUILD_DIR}/topoexec_prometheus_adapter_smoke"
  RESULT_VARIABLE downstream_run_result
)
if(NOT downstream_run_result EQUAL 0)
  message(FATAL_ERROR "Prometheus adapter downstream executable failed")
endif()
