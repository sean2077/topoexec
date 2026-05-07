#!/usr/bin/env python3
"""Check package matrix docs and local package metadata stay aligned."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REQUIRED_OPTIONS = [
    "TOPOEXEC_BUILD_YAML",
    "TOPOEXEC_BUILD_CLI",
    "TOPOEXEC_BUILD_EXAMPLES",
    "TOPOEXEC_BUILD_TESTING",
    "TOPOEXEC_BUILD_C_API",
    "TOPOEXEC_BUILD_PYTHON_PREVIEW",
    "TOPOEXEC_BUILD_PLUGIN_LOADER",
    "TOPOEXEC_BUILD_OTEL_ADAPTER",
    "TOPOEXEC_BUILD_PROMETHEUS_ADAPTER",
    "TOPOEXEC_BUILD_ROS2_ADAPTER",
]

REQUIRED_CONFIG_VARS = [
    "TOPOEXEC_VERSION",
    "TOPOEXEC_SCHEMA_VERSION",
    "TOPOEXEC_SEMANTIC_CONTRACT_VERSION",
    "TOPOEXEC_HAS_RUNTIME",
    "TOPOEXEC_HAS_ADAPTER_SDK",
    "TOPOEXEC_HAS_C_API",
    "TOPOEXEC_HAS_PYTHON_PREVIEW",
    "TOPOEXEC_HAS_PLUGIN_LOADER",
    "TOPOEXEC_HAS_YAML",
    "TOPOEXEC_HAS_CLI",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir
    try:
        matrix = (root / "docs/43-ci-build-release-tools/package-matrix.md").read_text(encoding="utf-8")
        build_doc = (root / "docs/43-ci-build-release-tools/build-and-package.md").read_text(encoding="utf-8")
        cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
        config = (root / "cmake/topoexecConfig.cmake.in").read_text(encoding="utf-8")
        for option in REQUIRED_OPTIONS:
            require(option in cmake, f"CMake missing option {option}")
            require(option in build_doc, f"build-and-package missing option {option}")
            require(option in matrix, f"package matrix missing option/surface {option}")
        for var in REQUIRED_CONFIG_VARS:
            require(var in config, f"package config missing metadata var {var}")
            require(var in build_doc, f"build-and-package missing metadata var {var}")
        vcpkg = json.loads((root / "packaging/vcpkg/vcpkg.json").read_text(encoding="utf-8"))
        require(vcpkg["version-string"] == "0.2.0", "vcpkg version must match candidate package version")
        require(vcpkg["license"] == "Apache-2.0", "vcpkg license must be Apache-2.0")
        conan = (root / "packaging/conan/conanfile.py").read_text(encoding="utf-8")
        require('version = "0.2.0"' in conan, "Conan version must match candidate package version")
        require('license = "Apache-2.0"' in conan, "Conan license must be Apache-2.0")
        for text, name in [(matrix, "package matrix"), (build_doc, "build-and-package")]:
            require("not published" in text.lower() or "Local only" in text, f"{name} must state registry publication boundary")
            require("production" in text.lower(), f"{name} must keep production/deferred wording visible")
        require(re.search(r"set\(CPACK_PACKAGE_VERSION\s+\"\$\{PROJECT_VERSION\}\"\)", cmake), "CPack version must come from PROJECT_VERSION")
    except Exception as exc:  # noqa: BLE001
        print(f"package matrix check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "package_version": "0.2.0", "license": "Apache-2.0"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
