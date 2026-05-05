if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

set(SMOKE_ROOT "${BUILD_DIR}/runtime-only-options-smoke")
set(RUNTIME_ONLY_BUILD_DIR "${SMOKE_ROOT}/build")
set(RUNTIME_ONLY_INSTALL_DIR "${SMOKE_ROOT}/install")

file(REMOVE_RECURSE "${SMOKE_ROOT}")
file(MAKE_DIRECTORY "${SMOKE_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${RUNTIME_ONLY_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_YAML=OFF
    -DTOPOEXEC_BUILD_CLI=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_TESTING=OFF
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Runtime-only options configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${RUNTIME_ONLY_BUILD_DIR}" --target topoexec_runtime -j
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Runtime-only target build failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${RUNTIME_ONLY_BUILD_DIR}" --prefix "${RUNTIME_ONLY_INSTALL_DIR}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Runtime-only install failed")
endif()

if(NOT EXISTS "${RUNTIME_ONLY_INSTALL_DIR}/lib/cmake/topoexec/topoexecConfig.cmake")
  message(FATAL_ERROR "Runtime-only install did not export topoexecConfig.cmake")
endif()
if(NOT EXISTS "${RUNTIME_ONLY_INSTALL_DIR}/include/topoexec/runtime/runtime_runner.hpp")
  message(FATAL_ERROR "Runtime-only install did not install public runtime headers")
endif()
