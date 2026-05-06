if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(NOT CMAKE_CTEST_COMMAND)
  find_program(CMAKE_CTEST_COMMAND NAMES ctest REQUIRED)
endif()

set(SMOKE_ROOT "${BUILD_DIR}/python-preview-options-smoke")
set(DISABLED_BUILD_DIR "${SMOKE_ROOT}/disabled-build")
set(PREVIEW_BUILD_DIR "${SMOKE_ROOT}/preview-build")
set(PREVIEW_INSTALL_DIR "${SMOKE_ROOT}/preview-install")
set(CONFIG_CHECK_DIR "${SMOKE_ROOT}/installed-config-check")
set(CONFIG_CHECK_BUILD_DIR "${SMOKE_ROOT}/installed-config-check-build")

file(REMOVE_RECURSE "${SMOKE_ROOT}")
file(MAKE_DIRECTORY "${SMOKE_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${DISABLED_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_PYTHON_PREVIEW=OFF
    -DTOPOEXEC_BUILD_YAML=OFF
    -DTOPOEXEC_BUILD_CLI=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_TESTING=OFF
  RESULT_VARIABLE disabled_configure_result
)
if(NOT disabled_configure_result EQUAL 0)
  message(FATAL_ERROR "Python preview disabled runtime-only configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${DISABLED_BUILD_DIR}" --target topoexec_runtime -j
  RESULT_VARIABLE disabled_build_result
)
if(NOT disabled_build_result EQUAL 0)
  message(FATAL_ERROR "Python preview disabled runtime-only build failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${PREVIEW_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_PYTHON_PREVIEW=ON
  RESULT_VARIABLE preview_configure_result
)
if(NOT preview_configure_result EQUAL 0)
  message(FATAL_ERROR "Python preview configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${PREVIEW_BUILD_DIR}" --target topoexec_cli -j
  RESULT_VARIABLE preview_build_result
)
if(NOT preview_build_result EQUAL 0)
  message(FATAL_ERROR "Python preview CLI build failed")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PREVIEW_BUILD_DIR}" --output-on-failure -R "python_preview_smoke|policy_.*"
  RESULT_VARIABLE preview_test_result
)
if(NOT preview_test_result EQUAL 0)
  message(FATAL_ERROR "Python preview smoke failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${PREVIEW_BUILD_DIR}" --prefix "${PREVIEW_INSTALL_DIR}"
  RESULT_VARIABLE preview_install_result
)
if(NOT preview_install_result EQUAL 0)
  message(FATAL_ERROR "Python preview install failed")
endif()

if(NOT EXISTS "${PREVIEW_INSTALL_DIR}/share/topoexec/python/topoexec_preview/client.py")
  message(FATAL_ERROR "Python preview install did not include client.py")
endif()
if(NOT EXISTS "${PREVIEW_INSTALL_DIR}/bin/topoexec")
  message(FATAL_ERROR "Python preview install did not include CLI executable")
endif()

file(MAKE_DIRECTORY "${CONFIG_CHECK_DIR}")
file(WRITE "${CONFIG_CHECK_DIR}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.20)
project(topoexec_python_preview_config_check LANGUAGES NONE)
find_package(topoexec CONFIG REQUIRED COMPONENTS python_preview)
if(NOT TOPOEXEC_HAS_PYTHON_PREVIEW)
  message(FATAL_ERROR \"Installed package did not report TOPOEXEC_HAS_PYTHON_PREVIEW\")
endif()
")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${CONFIG_CHECK_DIR}"
    -B "${CONFIG_CHECK_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${PREVIEW_INSTALL_DIR}"
  RESULT_VARIABLE config_check_result
)
if(NOT config_check_result EQUAL 0)
  message(FATAL_ERROR "Python preview installed config check failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "PYTHONPATH=${PREVIEW_INSTALL_DIR}/share/topoexec/python"
    "TOPOEXEC_PYTHON_PREVIEW_EXE=${PREVIEW_INSTALL_DIR}/bin/topoexec"
    "TOPOEXEC_SOURCE_DIR=${SOURCE_DIR}"
    "${Python3_EXECUTABLE}" "${SOURCE_DIR}/tests/python/test_python_preview.py"
  RESULT_VARIABLE installed_smoke_result
)
if(NOT installed_smoke_result EQUAL 0)
  message(FATAL_ERROR "Installed Python preview smoke failed")
endif()
