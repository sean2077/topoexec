# Dynamic Plugin Loader Preview

## Status

The dynamic plugin loader is an optional, default-off, trusted-native-code
preview target. Enable it with:

```bash
cmake -S . -B build-plugins -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_PLUGIN_LOADER=ON
cmake --build build-plugins -j
ctest --test-dir build-plugins --output-on-failure -R test_plugin_loader
```

Installed CMake consumers request the preview component explicitly:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS plugin_loader)
target_link_libraries(my_host PRIVATE topoexec::plugin_loader)
```

The package config exposes `TOPOEXEC_HAS_PLUGIN_LOADER` and exports
`topoexec::plugin_loader` only when the option was enabled. The stable primary
path remains explicit in-process `ComponentRegistry` registration.

## Manifest and exports

A plugin is a native shared object loaded by path. It must export exactly the
preview-v0 C symbols declared by `topoexec/plugins/loader.hpp`:

- `topoexec_plugin_manifest_v0`: returns a `PluginManifestView` with
  `plugin_id`, `plugin_api_version`, `schema_version`, and declared component
  descriptors.
- `topoexec_plugin_register_v0`: receives a `ComponentRegistry*` and registers
  factories for the declared component types.

The current preview requires:

- `plugin_api_version == "0"`;
- `schema_version == "1"`;
- at least one non-empty component type;
- registered component types matching the manifest when
  `PluginLoadOptions::require_declared_components` is true.

`PluginLoadResult` reports structured error codes such as `dlopen`,
`symbol.manifest`, `version.plugin_api`, `version.schema`,
`register.undeclared_component`, and `register.missing_component`.

## Security model

Plugins are trusted native code. Loading a plugin can execute arbitrary process
code and can corrupt host memory if the plugin is malicious or ABI-incompatible.
TopoExec does not sandbox plugins, verify signatures, constrain filesystem or
network access, or provide crash isolation. Hosts should load only application-
owned plugins from trusted paths and should run untrusted components in a
separate process boundary outside this preview.

## Unload semantics

`PluginLoadOptions::close_on_destroy` defaults to `false`. The loader keeps the
native handle open because a registry may retain factories whose code lives in
the plugin. Automatic unload is opt-in only for hosts that can prove all
registered factories, components, and function objects have been destroyed
before the handle closes.

## Non-goals

This preview does not freeze a stable plugin ABI, provide package discovery, load plugins
from graph schema fields, implement sandboxing, add Python/Rust/native callback
bindings, or make dynamic plugins a default runtime dependency. It also does not
replace the stable explicit `ComponentRegistry` path.

## Validation

The focused gate is:

```bash
./scripts/goal_check.sh plugins
```

It configures `TOPOEXEC_BUILD_PLUGIN_LOADER=ON`, builds sample native plugins,
runs loader tests for successful load, plugin API version mismatch, descriptor
mismatch, missing path, and opt-in unload metadata, then runs policy checks. The
CMake option smoke also proves a disabled runtime-only build still works and an
installed package can export `topoexec::plugin_loader` explicitly.
