if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()
if(NOT CMAKE_CTEST_COMMAND)
  find_program(CMAKE_CTEST_COMMAND NAMES ctest REQUIRED)
endif()

set(SMOKE_ROOT "${BUILD_DIR}/plugin-loader-options-smoke")
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
    -DTOPOEXEC_BUILD_PLUGIN_LOADER=OFF
    -DTOPOEXEC_BUILD_YAML=OFF
    -DTOPOEXEC_BUILD_CLI=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_TESTING=OFF
  RESULT_VARIABLE disabled_configure_result
)
if(NOT disabled_configure_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader disabled runtime-only configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${DISABLED_BUILD_DIR}" --target topoexec_runtime -j
  RESULT_VARIABLE disabled_build_result
)
if(NOT disabled_build_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader disabled runtime-only build failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${PREVIEW_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DTOPOEXEC_BUILD_PLUGIN_LOADER=ON
  RESULT_VARIABLE preview_configure_result
)
if(NOT preview_configure_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader preview configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${PREVIEW_BUILD_DIR}" -j
  RESULT_VARIABLE preview_build_result
)
if(NOT preview_build_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader preview build failed")
endif()

execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PREVIEW_BUILD_DIR}" --output-on-failure -R "test_plugin_loader|policy_.*"
  RESULT_VARIABLE preview_test_result
)
if(NOT preview_test_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader preview smoke failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${PREVIEW_BUILD_DIR}" --prefix "${PREVIEW_INSTALL_DIR}"
  RESULT_VARIABLE preview_install_result
)
if(NOT preview_install_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader preview install failed")
endif()

if(NOT EXISTS "${PREVIEW_INSTALL_DIR}/include/topoexec/plugins/loader.hpp")
  message(FATAL_ERROR "Plugin loader install did not include preview header")
endif()
file(GLOB plugin_loader_libraries
  "${PREVIEW_INSTALL_DIR}/lib/*topoexec_plugin_loader*"
  "${PREVIEW_INSTALL_DIR}/lib/*plugin_loader*"
)
if(NOT plugin_loader_libraries)
  message(FATAL_ERROR "Plugin loader install did not include preview library")
endif()
if(NOT EXISTS "${PREVIEW_INSTALL_DIR}/lib/cmake/topoexec/topoexecTargets.cmake")
  message(FATAL_ERROR "Plugin loader install is missing package targets")
endif()

file(MAKE_DIRECTORY "${CONFIG_CHECK_DIR}")
file(WRITE "${CONFIG_CHECK_DIR}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.20)
project(topoexec_plugin_loader_config_check LANGUAGES CXX)
find_package(topoexec CONFIG REQUIRED COMPONENTS plugin_loader)
if(NOT TOPOEXEC_HAS_PLUGIN_LOADER)
  message(FATAL_ERROR \"Installed package did not report TOPOEXEC_HAS_PLUGIN_LOADER\")
endif()
if(NOT TARGET topoexec::plugin_loader)
  message(FATAL_ERROR \"Installed package did not export topoexec::plugin_loader\")
endif()
add_executable(topoexec_plugin_loader_config_check main.cpp)
target_link_libraries(topoexec_plugin_loader_config_check PRIVATE topoexec::plugin_loader)
")
file(WRITE "${CONFIG_CHECK_DIR}/main.cpp"
"#include \"topoexec/plugins/loader.hpp\"
#include <string>
int main() {
  return std::string(topoexec::plugins::kPluginLoaderPreviewApiVersion) == \"0\" ? 0 : 1;
}
")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${CONFIG_CHECK_DIR}"
    -B "${CONFIG_CHECK_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${PREVIEW_INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  RESULT_VARIABLE config_check_result
)
if(NOT config_check_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader installed config check failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${CONFIG_CHECK_BUILD_DIR}" -j
  RESULT_VARIABLE config_build_result
)
if(NOT config_build_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader installed config build failed")
endif()

execute_process(
  COMMAND "${CONFIG_CHECK_BUILD_DIR}/topoexec_plugin_loader_config_check"
  RESULT_VARIABLE config_run_result
)
if(NOT config_run_result EQUAL 0)
  message(FATAL_ERROR "Plugin loader installed config executable failed")
endif()
