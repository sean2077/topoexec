cmake_minimum_required(VERSION 3.20)
project(topoexec_c_api_smoke LANGUAGES C CXX)

find_package(topoexec CONFIG REQUIRED COMPONENTS c_api)

if(NOT TOPOEXEC_HAS_RUNTIME)
  message(FATAL_ERROR "Installed package did not report TOPOEXEC_HAS_RUNTIME")
endif()
if(NOT TOPOEXEC_HAS_C_API)
  message(FATAL_ERROR "Installed package did not report TOPOEXEC_HAS_C_API")
endif()
if(NOT TARGET topoexec::c_api)
  message(FATAL_ERROR "Installed package did not export topoexec::c_api")
endif()

add_executable(topoexec_c_api_smoke main.c)
set_property(TARGET topoexec_c_api_smoke PROPERTY LINKER_LANGUAGE CXX)
target_link_libraries(topoexec_c_api_smoke PRIVATE topoexec::c_api)
