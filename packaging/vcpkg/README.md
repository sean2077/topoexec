# vcpkg Port Draft

This folder contains a reviewable draft for a future upstream vcpkg port. It is
not submitted to any registry yet and must not be treated as a published package.

Files:

- `vcpkg.json`: draft manifest with `yaml` and `cli` features.
- `portfile.cmake`: draft CMake install/config-fixup flow.

Expected feature mapping:

| Feature | CMake options | Dependencies |
| --- | --- | --- |
| runtime-only | `TOPOEXEC_BUILD_YAML=OFF`, `TOPOEXEC_BUILD_CLI=OFF`, `TOPOEXEC_BUILD_EXAMPLES=OFF`, `TOPOEXEC_BUILD_TESTING=OFF` | none beyond C++ toolchain |
| `yaml` | `TOPOEXEC_BUILD_YAML=ON` | `yaml-cpp`, `nlohmann-json` |
| `cli` | `TOPOEXEC_BUILD_CLI=ON`, requires YAML | `cli11`, plus YAML deps |

Publication blockers:

1. Replace the placeholder `SHA512` in `portfile.cmake` with the checksum of an
   immutable release archive.
2. Verify the final feature-dependency syntax against the target vcpkg registry.
3. Run vcpkg CI on Linux/macOS/Windows before submitting.
