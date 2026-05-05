if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

set(SMOKE_ROOT "${BUILD_DIR}/package-smoke")
set(INSTALL_DIR "${SMOKE_ROOT}/install")
set(DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/runtime-build")
set(SANITIZER_CONFIGURE_ARGS)
if(DEFINED SANITIZER_FLAGS AND NOT "${SANITIZER_FLAGS}" STREQUAL "")
  list(APPEND SANITIZER_CONFIGURE_ARGS
    "-DCMAKE_CXX_FLAGS=-fsanitize=${SANITIZER_FLAGS} -fno-omit-frame-pointer"
    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${SANITIZER_FLAGS}"
  )
endif()

file(REMOVE_RECURSE "${SMOKE_ROOT}")
file(MAKE_DIRECTORY "${SMOKE_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${INSTALL_DIR}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "TopoExec install failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/runtime_smoke"
    -B "${DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    ${SANITIZER_CONFIGURE_ARGS}
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Runtime package smoke configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Runtime package smoke build failed")
endif()

execute_process(
  COMMAND "${DOWNSTREAM_BUILD_DIR}/topoexec_runtime_smoke"
  RESULT_VARIABLE run_result
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Runtime package smoke executable failed")
endif()
