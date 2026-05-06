#!/usr/bin/env python3
"""Validate that package-manager draft files are reviewable and aligned."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir

    vcpkg_json = root / "packaging/vcpkg/vcpkg.json"
    portfile = root / "packaging/vcpkg/portfile.cmake"
    conanfile = root / "packaging/conan/conanfile.py"
    failures: list[str] = []

    try:
        manifest = json.loads(vcpkg_json.read_text(encoding="utf-8"))
        require(manifest["name"] == "topoexec", "vcpkg name drifted")
        require(manifest["version-string"] == "0.1.0", "vcpkg version drifted")
        require(manifest["license"] == "MIT", "vcpkg license drifted")
        require("yaml" in manifest.get("features", {}), "vcpkg yaml feature missing")
        require("cli" in manifest.get("features", {}), "vcpkg cli feature missing")
        require("vcpkg-cmake" in {dep["name"] for dep in manifest.get("dependencies", []) if isinstance(dep, dict)},
                "vcpkg host cmake dependency missing")
    except Exception as error:  # noqa: BLE001
        failures.append(f"vcpkg manifest: {error}")

    try:
        text = portfile.read_text(encoding="utf-8")
        for token in [
            "vcpkg_from_github",
            "vcpkg_check_features",
            "TOPOEXEC_BUILD_TESTING=OFF",
            "TOPOEXEC_BUILD_EXAMPLES=OFF",
            "vcpkg_cmake_config_fixup",
            "vcpkg_install_copyright",
        ]:
            require(token in text, f"vcpkg portfile missing {token}")
    except Exception as error:  # noqa: BLE001
        failures.append(f"vcpkg portfile: {error}")

    try:
        text = conanfile.read_text(encoding="utf-8")
        for token in [
            "class TopoExecConan",
            'name = "topoexec"',
            'version = "0.1.0"',
            '"yaml": [True, False]',
            '"cli": [True, False]',
            'tc.variables["TOPOEXEC_BUILD_TESTING"] = False',
            'cmake.install()',
            'topoexec::runtime',
        ]:
            require(token in text, f"Conan draft missing {token}")
    except Exception as error:  # noqa: BLE001
        failures.append(f"Conan draft: {error}")

    forbidden = ["find_package(ROS", "OpenTelemetry", "Prometheus"]
    for path in [vcpkg_json, portfile, conanfile]:
        text = path.read_text(encoding="utf-8")
        for token in forbidden:
            if token in text:
                failures.append(f"{path.relative_to(root)} unexpectedly mentions {token}")

    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
