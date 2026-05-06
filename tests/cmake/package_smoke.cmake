if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

set(SMOKE_ROOT "${BUILD_DIR}/package-smoke")
set(INSTALL_DIR "${SMOKE_ROOT}/install")
set(DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/runtime-build")
set(YAML_DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/yaml-build")
set(CLI_DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/cli-build")
set(ADAPTER_SDK_DOWNSTREAM_BUILD_DIR "${SMOKE_ROOT}/adapter-sdk-build")
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

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/adapter_sdk_smoke"
    -B "${ADAPTER_SDK_DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    ${SANITIZER_CONFIGURE_ARGS}
  RESULT_VARIABLE adapter_sdk_configure_result
)
if(NOT adapter_sdk_configure_result EQUAL 0)
  message(FATAL_ERROR "Adapter SDK package smoke configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${ADAPTER_SDK_DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE adapter_sdk_build_result
)
if(NOT adapter_sdk_build_result EQUAL 0)
  message(FATAL_ERROR "Adapter SDK package smoke build failed")
endif()

execute_process(
  COMMAND "${ADAPTER_SDK_DOWNSTREAM_BUILD_DIR}/topoexec_adapter_sdk_smoke"
  RESULT_VARIABLE adapter_sdk_run_result
)
if(NOT adapter_sdk_run_result EQUAL 0)
  message(FATAL_ERROR "Adapter SDK package smoke executable failed")
endif()

if(NOT EXISTS "${INSTALL_DIR}/lib/cmake/topoexec/topoexecConfig.cmake")
  message(FATAL_ERROR "Install did not export topoexecConfig.cmake")
endif()
if(NOT EXISTS "${INSTALL_DIR}/lib/cmake/topoexec/topoexecConfigVersion.cmake")
  message(FATAL_ERROR "Install did not export topoexecConfigVersion.cmake")
endif()
if(NOT EXISTS "${INSTALL_DIR}/share/topoexec/schema/topoexec.schema.v1.json")
  message(FATAL_ERROR "Install did not include bundled schema")
endif()
if(NOT EXISTS "${INSTALL_DIR}/bin/topoexec")
  message(FATAL_ERROR "Default install did not include topoexec CLI")
endif()

set(DOCTOR_CWD "${SMOKE_ROOT}/installed-cli-cwd")
file(MAKE_DIRECTORY "${DOCTOR_CWD}")
execute_process(
  COMMAND "${INSTALL_DIR}/bin/topoexec" doctor --format json
  WORKING_DIRECTORY "${DOCTOR_CWD}"
  RESULT_VARIABLE installed_doctor_result
  OUTPUT_VARIABLE installed_doctor_json
  ERROR_VARIABLE installed_doctor_error
)
if(NOT installed_doctor_result EQUAL 0)
  message(FATAL_ERROR "Installed topoexec doctor failed: ${installed_doctor_error}")
endif()
if(NOT installed_doctor_json MATCHES "\"schema_found\"[^\n]*true")
  message(FATAL_ERROR "Installed topoexec doctor did not discover installed schema: ${installed_doctor_json}")
endif()
if(NOT installed_doctor_json MATCHES "share/topoexec/schema/topoexec\\.schema\\.v1\\.json")
  message(FATAL_ERROR "Installed topoexec doctor schema_path did not point at installed schema: ${installed_doctor_json}")
endif()
execute_process(
  COMMAND "${INSTALL_DIR}/bin/topoexec" schema dump --format json
  WORKING_DIRECTORY "${DOCTOR_CWD}"
  RESULT_VARIABLE installed_schema_dump_result
  OUTPUT_VARIABLE installed_schema_dump_json
  ERROR_VARIABLE installed_schema_dump_error
)
if(NOT installed_schema_dump_result EQUAL 0)
  message(FATAL_ERROR "Installed topoexec schema dump failed: ${installed_schema_dump_error}")
endif()
if(NOT installed_schema_dump_json MATCHES "\"\\$id\"[^\n]*topoexec\\.schema\\.v1\\.json")
  message(FATAL_ERROR "Installed topoexec schema dump did not read the installed schema")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/yaml_smoke"
    -B "${YAML_DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}"
    "-DTOPOEXEC_SMOKE_GRAPH=${SOURCE_DIR}/examples/minimal.yaml"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    ${SANITIZER_CONFIGURE_ARGS}
  RESULT_VARIABLE yaml_configure_result
)
if(NOT yaml_configure_result EQUAL 0)
  message(FATAL_ERROR "YAML package smoke configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${YAML_DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE yaml_build_result
)
if(NOT yaml_build_result EQUAL 0)
  message(FATAL_ERROR "YAML package smoke build failed")
endif()

execute_process(
  COMMAND "${YAML_DOWNSTREAM_BUILD_DIR}/topoexec_yaml_smoke"
  RESULT_VARIABLE yaml_run_result
)
if(NOT yaml_run_result EQUAL 0)
  message(FATAL_ERROR "YAML package smoke executable failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}/tests/cmake/cli_smoke"
    -B "${CLI_DOWNSTREAM_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}"
  RESULT_VARIABLE cli_configure_result
)
if(NOT cli_configure_result EQUAL 0)
  message(FATAL_ERROR "CLI package smoke configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${CLI_DOWNSTREAM_BUILD_DIR}" -j
  RESULT_VARIABLE cli_build_result
)
if(NOT cli_build_result EQUAL 0)
  message(FATAL_ERROR "CLI package smoke build/run failed")
endif()
