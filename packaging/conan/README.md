# Conan Recipe Draft

This is a draft for a future Conan recipe. It is not a published package recipe
yet.

Package options should mirror CMake:

```python
options = {
    "yaml": [True, False],
    "cli": [True, False],
    "examples": [True, False],
}
default_options = {
    "yaml": True,
    "cli": True,
    "examples": False,
}
```

The generated CMake configure step should translate options to:

```python
tc.variables["TOPOEXEC_BUILD_YAML"] = self.options.yaml
tc.variables["TOPOEXEC_BUILD_CLI"] = self.options.cli
tc.variables["TOPOEXEC_BUILD_EXAMPLES"] = self.options.examples
tc.variables["TOPOEXEC_BUILD_TESTING"] = False
```

Dependency mapping:

- `yaml=True`: require `yaml-cpp` and `nlohmann_json`;
- `cli=True`: require `cli11` and force `yaml=True`;
- runtime-only consumers link `topoexec::runtime`.
