# Conan Recipe Draft

This folder contains a reviewable draft for a future Conan recipe. It is not
published to a Conan remote yet. The current draft recipe uses version `0.2.0` and Apache-2.0 license metadata for the `v0.2.0-alpha.0` candidate line.

Files:

- `conanfile.py`: draft recipe mirroring the CMake package options.

Expected option mapping:

| Option | CMake option | Dependencies |
| --- | --- | --- |
| `yaml=False`, `cli=False` | runtime-only package | none beyond C++ toolchain |
| `yaml=True` | `TOPOEXEC_BUILD_YAML=ON` | `yaml-cpp`, `nlohmann_json` |
| `cli=True` | `TOPOEXEC_BUILD_CLI=ON`, forces YAML | `cli11`, plus YAML deps |
| `examples=True` | `TOPOEXEC_BUILD_EXAMPLES=ON` | YAML required by examples |
| `plugin_loader=True` | `TOPOEXEC_BUILD_PLUGIN_LOADER=ON` | platform dynamic-loader library only; trusted-native preview |

Publication blockers:

1. Verify dependency version ranges against ConanCenter package names.
2. Run `conan create` for runtime-only, YAML, CLI, and plugin-loader profiles on clean machines.
3. Decide whether CLI executable packaging belongs in the library recipe or a
   separate tool package before publishing.
