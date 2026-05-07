#!/usr/bin/env python3
"""Check reliability program docs/scripts remain bounded and discoverable."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REQUIRED_DOC_PHRASES = [
    "## Test tiers",
    "## Bounded soak-lite",
    "## Performance regression policy",
    "## Fuzz corpus and sanitizer policy",
    "## Failure artifact convention",
    "ThreadSanitizer remains non-blocking",
    "no global performance threshold",
]

REQUIRED_SCRIPT_TOKENS = [
    "TOPOEXEC_STRESS_PROFILE=soak",
    "TOPOEXEC_SOAK_LITE_DURATION_SECONDS",
    "TOPOEXEC_SOAK_LITE_MAX_ITERATIONS",
    "TOPOEXEC_SOAK_LITE_SCALE",
    "TOPOEXEC_SOAK_LITE_STEPS",
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
        doc = (root / "docs/24-testing/reliability-program.md").read_text(encoding="utf-8")
        for phrase in REQUIRED_DOC_PHRASES:
            require(phrase in doc, f"reliability doc missing {phrase}")
        script = (root / "scripts/soak_lite_smoke.sh").read_text(encoding="utf-8")
        for token in REQUIRED_SCRIPT_TOKENS:
            require(token in script, f"soak_lite script missing bounded token {token}")
        goal_check = (root / "scripts/goal_check.sh").read_text(encoding="utf-8")
        for mode in ["stress", "fuzz", "bench", "sanitizer", "reliability"]:
            require(mode in goal_check, f"goal_check missing reliability mode/token {mode}")
        fuzz_corpus = sorted((root / "tests/fuzz/corpus/graph_inputs").glob("*.yaml"))
        require(len(fuzz_corpus) >= 4, "fuzz corpus should keep minimized YAML cases")
        testing_strategy = (root / "docs/24-testing/testing-strategy.md").read_text(encoding="utf-8")
        require("reliability-program.md" in testing_strategy, "testing strategy must link reliability program")
    except Exception as exc:  # noqa: BLE001
        print(f"reliability program check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "fuzz_corpus_cases": len(fuzz_corpus)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
