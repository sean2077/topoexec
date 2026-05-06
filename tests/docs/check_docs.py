#!/usr/bin/env python3
"""Run commands embedded in docs as topoexec-doc-test markers."""

from __future__ import annotations

import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path

MARKER = re.compile(r"<!--\s*topoexec-doc-test:\s*(.*?)\s*-->")

REQUIRED_DOCS = [
    "getting-started.md",
    "concepts.md",
    "case-study-robot-cell.md",
    "runtime-semantics.md",
    "api-overview.md",
    "public-api.md",
    "c-api.md",
    "python-preview.md",
    "plugin-loader.md",
    "schema-v1.md",
    "hierarchical-graphs.md",
    "graph-templates.md",
    "cookbook.md",
    "adapters.md",
    "testing-strategy.md",
    "release-checklist.md",
    "release-runbook.md",
    "beta-readiness-review.md",
    "architecture-diagrams.md",
    "why-topoexec.md",
    "design-principles.md",
]

REQUIRED_SECTIONS = {
    "cookbook.md": [
        "## Low-latency latest pipeline",
        "## Bounded queue command stream",
        "## Delay feedback control",
        "## CompositeLoop solver",
        "## Async request/response",
        "## State/config snapshot",
        "## Large payload ownership",
    ],
    "architecture-diagrams.md": [
        "## Runtime flow",
        "## Publication routing",
        "## Scheduler lanes",
        "## Channel lifecycle",
        "```mermaid",
    ],
    "why-topoexec.md": [
        "## oneTBB",
        "## Dora",
        "## GStreamer",
        "## ROS 2",
        "## Workflow engines",
    ],
    "design-principles.md": [
        "## Bounded everything",
        "## Explicit feedback",
        "## No hidden recursion",
        "## Observation is not control",
    ],
    "c-api.md": [
        "## Status",
        "## Design decisions",
        "## Ownership rules",
        "## Non-goals",
        "## Validation",
    ],
    "python-preview.md": [
        "## Status",
        "## Binding decision",
        "## Supported scope",
        "## Non-goals",
        "## Validation",
    ],
    "plugin-loader.md": [
        "## Status",
        "## Manifest and exports",
        "## Security model",
        "## Unload semantics",
        "## Non-goals",
        "## Validation",
    ],
}

README_REQUIRED_LINKS = [
    "(getting-started.md)",
    "(concepts.md)",
    "(case-study-robot-cell.md)",
    "(runtime-semantics.md)",
    "(api-overview.md)",
    "(public-api.md)",
    "(c-api.md)",
    "(python-preview.md)",
    "(plugin-loader.md)",
    "(schema-v1.md)",
    "(hierarchical-graphs.md)",
    "(graph-templates.md)",
    "(cookbook.md)",
    "(adapters.md)",
    "(testing-strategy.md)",
    "(release-checklist.md)",
    "(release-runbook.md)",
    "(beta-readiness-review.md)",
]


def expand(command: str, *, source_dir: Path, build_dir: Path, topoexec: Path) -> str:
    return (
        command.replace("${SOURCE_DIR}", str(source_dir))
        .replace("${BUILD_DIR}", str(build_dir))
        .replace("${TOPOEXEC}", str(topoexec))
    )


def validate_docs_map(docs_dir: Path) -> list[str]:
    failures: list[str] = []
    for relative in REQUIRED_DOCS:
        if not (docs_dir / relative).exists():
            failures.append(f"missing required docs page: docs/{relative}")

    readme = (docs_dir / "README.md").read_text(encoding="utf-8")
    for link in README_REQUIRED_LINKS:
        if link not in readme:
            failures.append(f"docs/README.md missing required link {link}")

    for relative, sections in REQUIRED_SECTIONS.items():
        path = docs_dir / relative
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for section in sections:
            if section not in text:
                failures.append(f"docs/{relative} missing required section {section}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--topoexec", required=True, type=Path)
    args = parser.parse_args()

    docs_dir = args.source_dir / "docs"
    map_failures = validate_docs_map(docs_dir)
    if map_failures:
        for failure in map_failures:
            sys.stderr.write(f"{failure}\n")
        return 1

    commands: list[tuple[Path, int, str]] = []
    for path in sorted(docs_dir.rglob("*.md")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = MARKER.search(line)
            if match:
                commands.append(
                    (
                        path,
                        number,
                        expand(match.group(1), source_dir=args.source_dir, build_dir=args.build_dir,
                               topoexec=args.topoexec),
                    )
                )

    if not commands:
        sys.stderr.write("no topoexec-doc-test markers found\n")
        return 1

    for path, number, command in commands:
        argv = shlex.split(command)
        completed = subprocess.run(
            argv,
            cwd=args.source_dir,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != 0:
            sys.stderr.write(f"{path.relative_to(args.source_dir)}:{number} failed: {command}\n")
            sys.stderr.write(completed.stdout)
            sys.stderr.write(completed.stderr)
            return completed.returncode
        print(f"ok {path.relative_to(args.source_dir)}:{number} {command}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
