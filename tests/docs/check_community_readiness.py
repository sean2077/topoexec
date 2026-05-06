#!/usr/bin/env python3
"""Check community contribution surfaces stay complete and runtime-safe."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


REQUIRED_CONTRIBUTING_SECTIONS = [
    "## Project philosophy",
    "## Non-goals",
    "## Contribution lanes",
    "## How to propose a semantic change",
    "## How to add a component or example",
    "## How to add a metric",
    "## How to add a schema field",
    "## Governance",
    "### Release cadence",
    "### API change review",
    "### Adapter acceptance policy",
    "## Issue and PR structure",
]

REQUIRED_TEMPLATES = {
    ".github/ISSUE_TEMPLATE/bug_report.md": ["## Reproduction", "## Validation tried"],
    ".github/ISSUE_TEMPLATE/semantic_mismatch.md": ["## Contract that appears wrong", "## Minimal graph"],
    ".github/ISSUE_TEMPLATE/adapter_request.md": ["## Dependency boundary", "topoexec::runtime"],
    ".github/ISSUE_TEMPLATE/performance_issue.md": ["## Measurement command", "graph bench"],
    ".github/ISSUE_TEMPLATE/design_proposal.md": ["## Compatibility impact", "## Alternatives rejected"],
    ".github/ISSUE_TEMPLATE/schema_change.md": ["## v1 vs v2 classification", "docs/33-specs-rfcs/schema-v2-notes.md"],
    ".github/ISSUE_TEMPLATE/component_example.md": ["## Runtime semantics demonstrated", "## Dependency boundary"],
    ".github/ISSUE_TEMPLATE/metric_change.md": ["## Cardinality and stability", "## Required updates"],
}

REQUIRED_PR_SECTIONS = [
    "## Goal",
    "## Scope matrix",
    "## Compatibility and non-goals",
    "## Validation",
    "## Review checklist",
    "## Agent-generated PRs",
    "docs/61-api/api-change-checklist.md",
    "docs/33-specs-rfcs/schema-v2-notes.md",
    "topoexec::runtime",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(root: Path, relative: str) -> str:
    path = root / relative
    require(path.exists(), f"missing {relative}")
    return path.read_text(encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir

    root_contributing = read(root, "CONTRIBUTING.md")
    require("docs/44-coding-standards/contributing.md" in root_contributing, "root CONTRIBUTING must link docs/44-coding-standards/contributing.md")
    require("./scripts/agent_check.sh" in root_contributing, "root CONTRIBUTING must mention required full gate")
    code_of_conduct = read(root, "CODE_OF_CONDUCT.md")
    require("Expected behavior" in code_of_conduct, "CODE_OF_CONDUCT must describe expected behavior")
    require("Enforcement" in code_of_conduct, "CODE_OF_CONDUCT must describe enforcement")

    contributing = read(root, "docs/44-coding-standards/contributing.md")
    for section in REQUIRED_CONTRIBUTING_SECTIONS:
        require(section in contributing, f"docs/44-coding-standards/contributing.md missing {section}")
    for phrase in [
        "How to propose a semantic change",
        "How to add a component or example",
        "How to add a metric",
        "How to add a schema field",
        "Production adapters, network exporters, editor extensions, and plugin discovery",
    ]:
        require(phrase in contributing, f"docs/44-coding-standards/contributing.md missing contributor guidance: {phrase}")

    pr_template = read(root, ".github/PULL_REQUEST_TEMPLATE.md")
    for section in REQUIRED_PR_SECTIONS:
        require(section in pr_template, f"PR template missing {section}")

    for relative, markers in REQUIRED_TEMPLATES.items():
        text = read(root, relative)
        for marker in markers:
            require(marker in text, f"{relative} missing {marker}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        sys.stderr.write(f"community readiness check failed: {error}\n")
        raise SystemExit(1)
