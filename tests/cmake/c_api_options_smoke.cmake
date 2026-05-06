if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

set(SMOKE_ROOT "${BUILD_DIR}/c-api-options-smoke")
set(C_API_BUILD_DIR "${SMOKE_ROOT}/build")
set(C_API_INSTALL_DIR "${SMOKE_ROOT}/install")
set(DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/downstream-build")

file(REMOVE_RECURSE "${SMOKE_ROOT}")
file(MAKE_DIRECTORY "${SMOKE_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${C_API_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_C_API=ON
    -DTOPOEXEC_BUILD_YAML=OFF
    -DTOPOEXEC_BUILD_CLI=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_TESTING=OFF
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "C API option configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${C_API_BUILD_DIR}" --target topoexec_c_api -j
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "C API option build failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${C_API_BUILD_DIR}" --prefix "${C_API_INSTALL_DIR}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "C API option install failed")
endif()

if(NOT EXISTS "${C_API_INSTALL_DIR}/include/topoexec/c_api/topoexec.h")
  message(FATAL_ERROR "C API install did not include header")
endif()
if(EXISTS "${C_API_INSTALL_DIR}/bin/topoexec")
  message(FATAL_ERROR "C API runtime-only install unexpectedly included the CLI")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/c_api_smoke"
    -B "${DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${C_API_INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  RESULT_VARIABLE downstream_configure_result
)
if(NOT downstream_configure_result EQUAL 0)
  message(FATAL_ERROR "C API downstream configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE downstream_build_result
)
if(NOT downstream_build_result EQUAL 0)
  message(FATAL_ERROR "C API downstream build failed")
endif()

execute_process(
  COMMAND "${DOWNSTREAM_BUILD_DIR}/topoexec_c_api_smoke"
  RESULT_VARIABLE downstream_run_result
)
if(NOT downstream_run_result EQUAL 0)
  message(FATAL_ERROR "C API downstream executable failed")
endif()
