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
    "00-start-here/project-map.md",
    "01-quickstart/getting-started.md",
    "10-user-overview/concepts.md",
    "10-user-overview/why-topoexec.md",
    "11-user-guide/case-study-robot-cell.md",
    "11-user-guide/cookbook.md",
    "11-user-guide/graph-templates.md",
    "11-user-guide/hierarchical-graphs.md",
    "12-integrations/adapter-boundaries.md",
    "12-integrations/plugin-loader.md",
    "12-integrations/python-preview.md",
    "20-development-overview/maintainer-map.md",
    "21-architecture/architecture-diagrams.md",
    "21-architecture/design-principles.md",
    "21-architecture/runtime-architecture.md",
    "21-architecture/runtime-semantics.md",
    "22-codebase/codebase-map.md",
    "24-testing/testing-strategy.md",
    "33-specs-rfcs/schema-v1.md",
    "33-specs-rfcs/schema-v2-notes.md",
    "41-development-tools/editor-schema.md",
    "43-ci-build-release-tools/beta-readiness-review.md",
    "43-ci-build-release-tools/release-checklist.md",
    "43-ci-build-release-tools/release-runbook.md",
    "45-doc-standards/documentation-system.md",
    "61-api/api-overview.md",
    "61-api/c-api.md",
    "61-api/public-api.md",
    "94-doc-migrations/2026-05-doc-reorganization.md",
    "94-doc-migrations/2026-05-process-ledger-cleanup.md",
]

REQUIRED_SECTIONS = {
    "11-user-guide/cookbook.md": [
        "## Low-latency latest pipeline",
        "## Bounded queue command stream",
        "## Delay feedback control",
        "## CompositeLoop solver",
        "## Async request/response",
        "## State/config snapshot",
        "## Large payload ownership",
    ],
    "21-architecture/architecture-diagrams.md": [
        "## Runtime flow",
        "## Publication routing",
        "## Scheduler lanes",
        "## Channel lifecycle",
        "```mermaid",
    ],
    "10-user-overview/why-topoexec.md": [
        "## oneTBB",
        "## Dora",
        "## GStreamer",
        "## ROS 2",
        "## Workflow engines",
    ],
    "21-architecture/design-principles.md": [
        "## Bounded everything",
        "## Explicit feedback",
        "## No hidden recursion",
        "## Observation is not control",
    ],
    "61-api/c-api.md": [
        "## Status",
        "## Design decisions",
        "## Ownership rules",
        "## Non-goals",
        "## Validation",
    ],
    "12-integrations/python-preview.md": [
        "## Status",
        "## Binding decision",
        "## Supported scope",
        "## Non-goals",
        "## Validation",
    ],
    "12-integrations/plugin-loader.md": [
        "## Status",
        "## Manifest and exports",
        "## Security model",
        "## Unload semantics",
        "## Non-goals",
        "## Validation",
    ],
    "33-specs-rfcs/schema-v2-notes.md": [
        "## Status",
        "## Decision rules",
        "## Candidate feature classification",
        "## Breaking vs additive changes",
        "## Migration plan",
        "## Non-goals",
        "## Validation",
    ],
    "41-development-tools/editor-schema.md": [
        "## Status",
        "## Schema discovery",
        "## VS Code workspace settings",
        "## Inline modeline",
        "## Diagnostics for editor integrations",
        "## LSP design boundary",
        "## Validation",
    ],
}

README_REQUIRED_LINKS = [
    "(00-start-here/project-map.md)",
    "(01-quickstart/getting-started.md)",
    "(10-user-overview/concepts.md)",
    "(11-user-guide/cookbook.md)",
    "(12-integrations/adapter-boundaries.md)",
    "(20-development-overview/maintainer-map.md)",
    "(21-architecture/runtime-architecture.md)",
    "(22-codebase/codebase-map.md)",
    "(24-testing/testing-strategy.md)",
    "(31-planning-roadmap/goals/backlog.md)",
    "(33-specs-rfcs/schema-v1.md)",
    "(41-development-tools/cli.md)",
    "(43-ci-build-release-tools/build-and-package.md)",
    "(44-coding-standards/contributing.md)",
    "(45-doc-standards/documentation-system.md)",
    "(61-api/api-overview.md)",
    "(62-schemas-protocols/metrics.md)",
    "(94-doc-migrations/2026-05-doc-reorganization.md)",
    "(94-doc-migrations/2026-05-process-ledger-cleanup.md)",
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
