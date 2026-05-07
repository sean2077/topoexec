#!/usr/bin/env python3
"""Run curated example metadata commands and validate example graph assets."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    "schema_version",
    "id",
    "title",
    "category",
    "summary",
    "difficulty",
    "graph",
    "commands",
    "assets",
    "ci",
    "related_docs",
    "tags",
}


def load_examples(root: Path) -> list[tuple[Path, dict[str, Any]]]:
    examples = []
    for path in sorted(root.glob("[0-9][0-9]-*/example.json")):
        metadata = json.loads(path.read_text(encoding="utf-8"))
        missing = REQUIRED - set(metadata)
        if missing:
            raise SystemExit(f"{path}: missing metadata keys: {sorted(missing)}")
        graph = path.parent / metadata["graph"]
        if not graph.exists():
            raise SystemExit(f"{path}: graph not found: {graph}")
        if not (path.parent / "README.md").exists():
            raise SystemExit(f"{path.parent}: missing README.md")
        examples.append((path.parent, metadata))
    if len(examples) < 8:
        raise SystemExit(f"expected at least 8 curated examples, found {len(examples)}")
    return examples


def run(topoexec: Path, command: dict[str, Any]) -> tuple[int, str]:
    argv = command.get("argv", [])
    if not argv or argv[0] != "graph":
        raise SystemExit(f"unsupported example command: {argv!r}")
    proc = subprocess.run([str(topoexec)] + argv, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc.returncode, proc.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--topoexec", default="build/topoexec")
    parser.add_argument("--examples-root", default="examples")
    args = parser.parse_args()
    topoexec = (ROOT / args.topoexec).resolve() if not Path(args.topoexec).is_absolute() else Path(args.topoexec)
    if not topoexec.exists():
        raise SystemExit(f"topoexec executable not found: {topoexec}")
    failures: list[str] = []
    command_count = 0
    for directory, metadata in load_examples(ROOT / args.examples_root):
        graph = directory / metadata["graph"]
        for doc in metadata.get("related_docs", []):
            if not (ROOT / doc).exists():
                failures.append(f"{metadata['id']}: related doc missing: {doc}")
        for command in metadata["commands"]:
            command_count += 1
            rc, output = run(topoexec, command)
            expect_success = command.get("expect_success", True)
            if expect_success and rc != 0:
                failures.append(f"{metadata['id']}: command failed {command['label']} rc={rc}\n{output[:1000]}")
            if not expect_success and rc == 0:
                failures.append(f"{metadata['id']}: command unexpectedly succeeded {command['label']}")
        if metadata["ci"].get("validate"):
            rc, output = run(topoexec, {"argv": ["graph", "validate", graph.relative_to(ROOT).as_posix()]})
            if rc != 0:
                failures.append(f"{metadata['id']}: primary validate failed\n{output[:1000]}")
        if metadata["ci"].get("render"):
            rc, output = run(topoexec, {"argv": ["graph", "render", graph.relative_to(ROOT).as_posix(), "--format", "mermaid"]})
            if rc != 0 or "flowchart" not in output:
                failures.append(f"{metadata['id']}: primary render failed\n{output[:1000]}")
        if metadata["ci"].get("run"):
            rc, output = run(topoexec, {"argv": ["graph", "run", graph.relative_to(ROOT).as_posix(), "--steps", "2"]})
            if rc != 0 or "ok" not in output.splitlines()[:1]:
                failures.append(f"{metadata['id']}: primary run failed\n{output[:1000]}")
    if failures:
        print("example smoke failures:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "examples": len(load_examples(ROOT / args.examples_root)), "commands": command_count}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
