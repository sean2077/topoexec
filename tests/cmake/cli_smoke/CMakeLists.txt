cmake_minimum_required(VERSION 3.20)
project(topoexec_cli_smoke NONE)

find_package(topoexec CONFIG REQUIRED COMPONENTS cli)

if(NOT TOPOEXEC_HAS_CLI)
  message(FATAL_ERROR "Installed package did not report TOPOEXEC_HAS_CLI")
endif()
if(NOT TARGET topoexec::topoexec_cli)
  message(FATAL_ERROR "Installed package did not export topoexec::topoexec_cli")
endif()
if(NOT TOPOEXEC_SCHEMA_VERSION EQUAL 1)
  message(FATAL_ERROR "Unexpected TopoExec schema version metadata")
endif()
if(NOT TOPOEXEC_SEMANTIC_CONTRACT_VERSION STREQUAL "0.2")
  message(FATAL_ERROR "Unexpected TopoExec semantic contract metadata")
endif()

add_custom_target(topoexec_cli_package_smoke ALL
  COMMAND $<TARGET_FILE:topoexec::topoexec_cli> doctor --format json
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  VERBATIM
)
