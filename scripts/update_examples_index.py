#!/usr/bin/env python3
"""Generate or check examples/README.md from curated example metadata."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]


def load_examples(examples_root: Path) -> list[tuple[str, dict[str, Any]]]:
    examples: list[tuple[str, dict[str, Any]]] = []
    for metadata_path in sorted(examples_root.glob("[0-9][0-9]-*/example.json")):
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        graph = metadata_path.parent / metadata.get("graph", "")
        if not graph.exists():
            raise SystemExit(f"{metadata_path}: graph file missing: {graph}")
        examples.append((metadata_path.parent.name, metadata))
    if not examples:
        raise SystemExit("no curated examples found")
    return sorted(examples, key=lambda item: int(item[1].get("readme_priority", 999)))


def generate(examples: list[tuple[str, dict[str, Any]]]) -> str:
    rows = []
    path_lines = []
    for idx, (directory, metadata) in enumerate(examples, 1):
        rows.append(
            f"| [`{directory}/`]({directory}/) | `{metadata['id']}` | {metadata['category']} | "
            f"{metadata['summary']} |"
        )
        path_lines.append(f"{idx}. [`{directory}/`]({directory}/) — {metadata['summary']}")
    return f"""# TopoExec Examples

This directory contains the public example gallery for TopoExec. The G74
showcase path is generated from curated `example.json` metadata, while the
legacy top-level YAML files remain in place as compatibility fixtures for
existing tests, docs, and golden outputs.

## Recommended learning path

{chr(10).join(path_lines)}

## Curated example matrix

| Directory | Metadata id | Category | Purpose |
| --- | --- | --- | --- |
{chr(10).join(rows)}

## Metadata convention

Each curated example directory uses a dependency-free `example.json` file that
is validated by `examples/metadata.schema.json` and consumed by showcase tooling.
The metadata declares:

- stable example id, category, title, summary, difficulty, and tags;
- the primary graph file;
- commands to validate, render, run, observe, or benchmark the example;
- generated asset requirements;
- focused CI smoke expectations;
- related documentation links.

A reusable README template lives at `_templates/example-readme.md`. Individual
example READMEs follow these sections: What this example demonstrates, Graph
structure, How to run, Expected result, What to inspect, and Related docs.

## Regenerate index and assets

```bash
python3 scripts/update_examples_index.py
python3 scripts/render_example_assets.py --topoexec build/topoexec
```

Use `--check` on either script in CI to fail when generated content is stale.

## Legacy YAML and C++ apps

The existing top-level YAML graphs and `apps/` C++ examples are still supported
and continue to back tests and deeper docs. G74 showcase pages should link to
the curated directories first, then to legacy fixtures where they explain an
advanced or compatibility-specific contract.

## Quick smoke

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'app_|cli_run_|cli_validate_'
```
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--examples-root", default="examples")
    parser.add_argument("--output", default="examples/README.md")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = generate(load_examples(ROOT / args.examples_root))
    output = ROOT / args.output
    if args.check:
        if not output.exists():
            print(f"missing generated index: {output.relative_to(ROOT)}", file=sys.stderr)
            return 1
        if output.read_text(encoding="utf-8") != content:
            print(f"stale generated index: {output.relative_to(ROOT)}", file=sys.stderr)
            return 1
        print(json.dumps({"ok": True, "mode": "check", "output": args.output}))
        return 0
    output.write_text(content, encoding="utf-8")
    print(json.dumps({"ok": True, "mode": "generate", "output": args.output}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
