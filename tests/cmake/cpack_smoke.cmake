if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required")
endif()
if(NOT DEFINED CPACK_COMMAND)
  message(FATAL_ERROR "CPACK_COMMAND is required")
endif()

file(REMOVE_RECURSE "${BUILD_DIR}/_CPack_Packages")
file(GLOB old_packages "${BUILD_DIR}/topoexec-*.tar.gz")
if(old_packages)
  file(REMOVE ${old_packages})
endif()

execute_process(
  COMMAND "${CPACK_COMMAND}" -G TGZ --config "${BUILD_DIR}/CPackConfig.cmake"
  WORKING_DIRECTORY "${BUILD_DIR}"
  RESULT_VARIABLE binary_package_result
)
if(NOT binary_package_result EQUAL 0)
  message(FATAL_ERROR "CPack binary TGZ generation failed")
endif()

execute_process(
  COMMAND "${CPACK_COMMAND}" -G TGZ --config "${BUILD_DIR}/CPackSourceConfig.cmake"
  WORKING_DIRECTORY "${BUILD_DIR}"
  RESULT_VARIABLE source_package_result
)
if(NOT source_package_result EQUAL 0)
  message(FATAL_ERROR "CPack source TGZ generation failed")
endif()

file(GLOB binary_packages "${BUILD_DIR}/topoexec-*-Linux.tar.gz")
file(GLOB source_packages "${BUILD_DIR}/topoexec-*-Source.tar.gz")
if(NOT binary_packages)
  message(FATAL_ERROR "CPack binary package was not created")
endif()
if(NOT source_packages)
  message(FATAL_ERROR "CPack source package was not created")
endif()
