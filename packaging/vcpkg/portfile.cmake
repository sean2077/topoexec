# Draft vcpkg portfile for review. It is not submitted to any registry yet.
# Fill REF and SHA512 with an immutable release archive before publication.

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO sean2077/topoexec
  REF "v${VERSION}"
  SHA512 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
  HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    yaml TOPOEXEC_BUILD_YAML
    cli TOPOEXEC_BUILD_CLI
    plugin-loader TOPOEXEC_BUILD_PLUGIN_LOADER
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    ${FEATURE_OPTIONS}
    -DTOPOEXEC_BUILD_TESTING=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME topoexec CONFIG_PATH lib/cmake/topoexec)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
