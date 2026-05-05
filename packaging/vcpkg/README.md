# vcpkg Port Draft

This is a draft for a future upstream vcpkg port. It is not a submitted or
validated registry package yet.

Expected CMake options for a default library package:

```cmake
vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DTOPOEXEC_BUILD_TESTING=OFF
    -DTOPOEXEC_BUILD_EXAMPLES=OFF
    -DTOPOEXEC_BUILD_CLI=ON
    -DTOPOEXEC_BUILD_YAML=ON
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME topoexec CONFIG_PATH lib/cmake/topoexec)
```

A minimal runtime-only feature should pass:

```cmake
-DTOPOEXEC_BUILD_YAML=OFF
-DTOPOEXEC_BUILD_CLI=OFF
-DTOPOEXEC_BUILD_EXAMPLES=OFF
-DTOPOEXEC_BUILD_TESTING=OFF
```

Dependencies to model:

- default/YAML feature: `yaml-cpp`, `nlohmann-json`;
- CLI feature: `cli11`;
- tests are off for package builds.
