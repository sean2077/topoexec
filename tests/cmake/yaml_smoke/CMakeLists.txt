cmake_minimum_required(VERSION 3.20)
project(topoexec_yaml_smoke LANGUAGES CXX)

find_package(topoexec CONFIG REQUIRED COMPONENTS yaml)

if(NOT TOPOEXEC_HAS_YAML)
  message(FATAL_ERROR "Installed package did not report TOPOEXEC_HAS_YAML")
endif()
if(NOT TOPOEXEC_SCHEMA_VERSION EQUAL 1)
  message(FATAL_ERROR "Unexpected TopoExec schema version metadata")
endif()
if(NOT TOPOEXEC_SEMANTIC_CONTRACT_VERSION STREQUAL "0.2")
  message(FATAL_ERROR "Unexpected TopoExec semantic contract metadata")
endif()

add_executable(topoexec_yaml_smoke main.cpp)
target_link_libraries(topoexec_yaml_smoke PRIVATE topoexec::yaml)
target_compile_definitions(topoexec_yaml_smoke PRIVATE TOPOEXEC_SMOKE_GRAPH="${TOPOEXEC_SMOKE_GRAPH}")
