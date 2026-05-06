#!/usr/bin/env python3
"""Enforce TopoExec architecture boundary rules without external deps."""

from __future__ import annotations

import argparse
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

ADAPTER_TOKENS = (
    "rclcpp",
    "opentelemetry",
    "prometheus",
    "perfetto",
    "pybind11",
    "Python.h",
)

PRIVATE_INCLUDE_TOKENS = (
    "tools/topoexec",
    "src/",
    "../src",
)

YAML_CLI_TOKENS = (
    "yaml-cpp",
    "YAML_CPP",
    "CLI/CLI.hpp",
    "CLI11",
)

SEMANTIC_BYPASS_HEADERS = (
    "topoexec/runtime/channel.hpp",
    "topoexec/runtime/event_runtime.hpp",
    "topoexec/runtime/scheduler.hpp",
    "topoexec/runtime/trigger_policy.hpp",
)

SEARCH_ROOTS = (
    "CMakeLists.txt",
    "include",
    "src",
    "tools",
    "cmake",
)

SKIP_SUFFIXES = (
    ".md",
    ".txt",
)

RUNTIME_SOURCE_FILES = {
    "src/buffer.cpp",
    "src/channel.cpp",
    "src/clock.cpp",
    "src/component.cpp",
    "src/component_registry.cpp",
    "src/diagnostics.cpp",
    "src/event_runtime.cpp",
    "src/graph.cpp",
    "src/logging.cpp",
    "src/metrics.cpp",
    "src/payload.cpp",
    "src/runtime_runner.cpp",
    "src/scheduler.cpp",
    "src/state.cpp",
    "src/task_executor.cpp",
    "src/trace.cpp",
    "src/trigger_policy.cpp",
}


@dataclass(frozen=True)
class SourceFile:
    path: Path
    rel: str
    text: str


def iter_files(root: Path) -> Iterable[SourceFile]:
    for entry in SEARCH_ROOTS:
        path = root / entry
        if path.is_file():
            yield SourceFile(path, entry, path.read_text(encoding="utf-8", errors="ignore"))
            continue
        if path.is_dir():
            for child in path.rglob("*"):
                if child.is_file() and child.suffix not in SKIP_SUFFIXES:
                    rel = child.relative_to(root).as_posix()
                    yield SourceFile(child, rel, child.read_text(encoding="utf-8", errors="ignore"))


def add_token_violations(violations: list[str], source: SourceFile, tokens: Iterable[str], reason: str) -> None:
    lowered = source.text.lower()
    for token in tokens:
        if is_allowed_optional_adapter_token(source, token):
            continue
        if token.lower() in lowered:
            violations.append(f"{source.rel} contains {reason} token {token!r}")


def is_allowed_optional_adapter_token(source: SourceFile, token: str) -> bool:
    token = token.lower()
    if token != "prometheus":
        return False
    return source.rel in {
        "CMakeLists.txt",
        "cmake/topoexecConfig.cmake.in",
        "include/topoexec/adapters/prometheus.hpp",
    }


def audit_files(root: Path) -> list[str]:
    violations: list[str] = []
    files = list(iter_files(root))

    for source in files:
        add_token_violations(violations, source, ADAPTER_TOKENS, "adapter SDK")

        if source.rel.startswith("include/"):
            if "API stability:" not in source.text:
                violations.append(f"{source.rel} missing API stability marker")
            if "internal-use-only" in source.text.lower():
                violations.append(f"{source.rel} is installed but marked internal-use-only")
            add_token_violations(violations, source, PRIVATE_INCLUDE_TOKENS, "private include")

        if source.rel.startswith("include/topoexec/common/"):
            add_token_violations(violations, source, ("topoexec/runtime/", *YAML_CLI_TOKENS), "common-layer dependency")

        if source.rel.startswith("include/topoexec/runtime/") or source.rel in RUNTIME_SOURCE_FILES:
            add_token_violations(violations, source, YAML_CLI_TOKENS, "runtime YAML/CLI dependency")

        if source.rel.startswith("tools/topoexec/"):
            add_token_violations(violations, source, SEMANTIC_BYPASS_HEADERS, "CLI semantic-bypass include")

    return violations


def cmake_call_body(text: str, prefix: str) -> str:
    start = text.find(prefix)
    if start == -1:
        return ""
    open_index = text.find("(", start)
    if open_index == -1:
        return ""
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return text[open_index + 1:index]
    return text[open_index + 1:]


def audit_cmake(root: Path) -> list[str]:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    violations: list[str] = []

    runtime_sources = cmake_call_body(cmake, "add_library(topoexec_runtime")
    if "src/graph_io.cpp" in runtime_sources:
        violations.append("topoexec_runtime must not compile YAML graph_io.cpp")

    runtime_links = cmake_call_body(cmake, "target_link_libraries(topoexec_runtime")
    if "topoexec_core" not in runtime_links:
        violations.append("topoexec_runtime must link topoexec_core")
    for token in ("topoexec_yaml", "topoexec_adapter_sdk", "CLI11", "YAML_CPP", "nlohmann_json"):
        if token in runtime_links:
            violations.append(f"topoexec_runtime must not link {token}")

    adapter_links = cmake_call_body(cmake, "target_link_libraries(topoexec_adapter_sdk")
    if "topoexec_runtime" not in adapter_links:
        violations.append("topoexec_adapter_sdk must consume topoexec_runtime")
    for token in ("topoexec_yaml", "CLI11", "YAML_CPP", "nlohmann_json"):
        if token in adapter_links:
            violations.append(f"topoexec_adapter_sdk must not link {token}")

    if "TOPOEXEC_BUILD_OTEL_ADAPTER" in cmake:
        otel_links = cmake_call_body(cmake, "target_link_libraries(topoexec_adapters_otel")
        if "topoexec_adapter_sdk" not in otel_links:
            violations.append("topoexec_adapters_otel must consume topoexec_adapter_sdk")
        for token in ("topoexec_runtime", "topoexec_yaml", "CLI11", "YAML_CPP", "nlohmann_json"):
            if token in otel_links:
                violations.append(f"topoexec_adapters_otel must not directly link {token}")
        if "install(EXPORT topoexecAdapterTargets" not in cmake:
            violations.append("topoexec_adapters_otel must export through topoexecAdapterTargets when enabled")

    if "TOPOEXEC_BUILD_PROMETHEUS_ADAPTER" in cmake:
        prometheus_links = cmake_call_body(cmake, "target_link_libraries(topoexec_adapters_prometheus")
        if "topoexec_adapter_sdk" not in prometheus_links:
            violations.append("topoexec_adapters_prometheus must consume topoexec_adapter_sdk")
        for token in ("topoexec_runtime", "topoexec_yaml", "CLI11", "YAML_CPP", "nlohmann_json"):
            if token in prometheus_links:
                violations.append(f"topoexec_adapters_prometheus must not directly link {token}")
        if "install(EXPORT topoexecAdapterTargets" not in cmake:
            violations.append("topoexec_adapters_prometheus must export through topoexecAdapterTargets when enabled")
        if "find_package(prometheus" in cmake.lower() or "prometheus-cpp" in cmake.lower():
            violations.append("topoexec_adapters_prometheus preview must not find/link an external Prometheus SDK")

    if "TOPOEXEC_BUILD_ROS2_ADAPTER" in cmake:
        ros2_links = cmake_call_body(cmake, "target_link_libraries(topoexec_adapters_ros2")
        if "topoexec_adapter_sdk" not in ros2_links:
            violations.append("topoexec_adapters_ros2 must consume topoexec_adapter_sdk")
        for token in ("topoexec_runtime", "topoexec_yaml", "CLI11", "YAML_CPP", "nlohmann_json"):
            if token in ros2_links:
                violations.append(f"topoexec_adapters_ros2 must not directly link {token}")
        if "install(EXPORT topoexecAdapterTargets" not in cmake:
            violations.append("topoexec_adapters_ros2 must export through topoexecAdapterTargets when enabled")
        lowered_cmake = cmake.lower()
        if "find_package(rclcpp" in lowered_cmake or "ament_" in lowered_cmake or "rosidl" in lowered_cmake:
            violations.append("topoexec_adapters_ros2 preview must not find/link ROS 2 packages")

    yaml_links = cmake_call_body(cmake, "target_link_libraries(topoexec_yaml")
    if "topoexec_runtime" not in yaml_links:
        violations.append("topoexec_yaml must link topoexec_runtime")
    if "PkgConfig::YAML_CPP" not in yaml_links:
        violations.append("topoexec_yaml must keep YAML dependency local to YAML target")

    cli_links = cmake_call_body(cmake, "target_link_libraries(topoexec_cli")
    if "topoexec_yaml" not in cli_links:
        violations.append("topoexec_cli should consume topoexec_yaml instead of runtime internals")
    if "topoexec_runtime" in cli_links:
        violations.append("topoexec_cli should not bypass topoexec_yaml by linking runtime directly")

    if "install(DIRECTORY include/ DESTINATION include)" not in cmake:
        violations.append("installed include directory audit expects include/ installation rule")

    return violations


def run_policy(root: Path) -> list[str]:
    return audit_files(root) + audit_cmake(root)


def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        (root / "include/topoexec/runtime").mkdir(parents=True)
        (root / "include/topoexec/runtime/bad.hpp").write_text(
            "#pragma once\n// API stability: stable-v0.2\n#include <rclcpp/rclcpp.hpp>\n#include <CLI/CLI.hpp>\n",
            encoding="utf-8",
        )
        (root / "include/topoexec/common").mkdir(parents=True)
        (root / "include/topoexec/common/bad.hpp").write_text(
            "#pragma once\n// API stability: experimental\n#include \"topoexec/runtime/graph.hpp\"\n",
            encoding="utf-8",
        )
        (root / "tools/topoexec").mkdir(parents=True)
        (root / "tools/topoexec/main.cpp").write_text(
            '#include "topoexec/runtime/channel.hpp"\n', encoding="utf-8"
        )
        (root / "src").mkdir()
        (root / "src/graph.cpp").write_text("// runtime source\n#include <yaml-cpp/yaml.h>\n", encoding="utf-8")
        (root / "cmake").mkdir()
        (root / "CMakeLists.txt").write_text(
            """
add_library(topoexec_runtime
  src/graph.cpp
  src/graph_io.cpp
)
target_link_libraries(topoexec_runtime PUBLIC topoexec_core PRIVATE PkgConfig::YAML_CPP)
add_library(topoexec_yaml src/graph_io.cpp)
target_link_libraries(topoexec_yaml PUBLIC topoexec_runtime PRIVATE PkgConfig::YAML_CPP)
add_executable(topoexec_cli tools/topoexec/main.cpp)
target_link_libraries(topoexec_cli PRIVATE topoexec_runtime CLI11::CLI11)
install(DIRECTORY include/ DESTINATION include)
""",
            encoding="utf-8",
        )
        violations = run_policy(root)
        required_fragments = [
            "adapter SDK",
            "runtime YAML/CLI dependency",
            "common-layer dependency",
            "CLI semantic-bypass include",
            "topoexec_runtime must not compile YAML graph_io.cpp",
            "topoexec_cli should consume topoexec_yaml",
        ]
        missing = [fragment for fragment in required_fragments if not any(fragment in item for item in violations)]
        if missing:
            sys.stderr.write("self-test failed; missing expected violations: " + ", ".join(missing) + "\n")
            sys.stderr.write("observed violations:\n" + "\n".join(violations) + "\n")
            return 1
    print("ok: architecture policy self-test caught planted violations")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.source_dir is None:
        parser.error("--source-dir is required unless --self-test is used")

    violations = run_policy(args.source_dir)
    if violations:
        sys.stderr.write("\n".join(violations) + "\n")
        return 1
    print("ok: architecture boundaries held for installed headers, runtime targets, CLI, and adapter tokens")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
