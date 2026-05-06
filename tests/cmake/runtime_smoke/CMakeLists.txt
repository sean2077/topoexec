cmake_minimum_required(VERSION 3.20)
project(topoexec_runtime_smoke LANGUAGES CXX)

find_package(topoexec CONFIG REQUIRED)

if(NOT TOPOEXEC_HAS_RUNTIME)
  message(FATAL_ERROR "Installed package did not report TOPOEXEC_HAS_RUNTIME")
endif()
if(NOT TOPOEXEC_SCHEMA_VERSION EQUAL 1)
  message(FATAL_ERROR "Unexpected TopoExec schema version metadata")
endif()
if(NOT TOPOEXEC_SEMANTIC_CONTRACT_VERSION STREQUAL "0.2")
  message(FATAL_ERROR "Unexpected TopoExec semantic contract metadata")
endif()

add_executable(topoexec_runtime_smoke main.cpp)
target_link_libraries(topoexec_runtime_smoke PRIVATE topoexec::runtime)
