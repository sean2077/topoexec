cmake_minimum_required(VERSION 3.20)
project(topoexec_runtime_smoke LANGUAGES CXX)

find_package(topoexec CONFIG REQUIRED)

add_executable(topoexec_runtime_smoke main.cpp)
target_link_libraries(topoexec_runtime_smoke PRIVATE topoexec::runtime)
