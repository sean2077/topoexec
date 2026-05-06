#!/usr/bin/env python3
"""Deterministic graph-input fuzz smoke for parser/compiler crash safety."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

CRASH_MARKERS = (
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
    "ThreadSanitizer",
    "Segmentation fault",
    "core dumped",
    "terminate called after throwing",
)

SEEDS = [
    b"",
    b"schema_version: 1\n",
    b"schema_version: one\ngraph: []\nlanes: {}\ncomponents: []\nedges: []\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test}\nlanes: []\ncomponents: []\nedges: []\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test}\nlanes: {main: {type: event_loop}}\ncomponents: []\nedges: []\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test}\nlanes: {main: {type: event_loop}}\ncomponents: [{id: a, type: topoexec.boundary.Input, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: missing}}]\nedges: []\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test}\nlanes: {main: {type: event_loop}}\ncomponents: [{id: a, type: topoexec.boundary.Input, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}]\nedges: [{id: e, kind: immediate, from: a.out, to: b.in, policy: {mode: latest}}]\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test, config: {nested: {x: [1, 2, 3]}}}\nlanes: {main: {type: thread_pool, max_threads: 2, queue_capacity: 1}}\ncomponents: []\nedges: []\n",
    b"schema_version: 1\ngraph: {name: fuzz, kind: internal_test}\nlanes: {main: {type: event_loop}}\ncomponents: [{id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}, {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}]\nedges: [{id: ab, kind: immediate, from: a.out, to: b.in, policy: {mode: latest}}, {id: ba, kind: immediate, from: b.out, to: a.in, policy: {mode: latest}}]\n",
    b"{not: yaml: [\n",
    b"schema_version: 1\n\xff\xfegraph: {name: invalid_utf8}\n",
]


def mutated_cases() -> list[bytes]:
    cases = list(SEEDS)
    tokens = [
        b"\x00",
        b"{}",
        b"[]",
        b"schema_version",
        b"kind: async",
        b"capacity: -1",
        b"id: " + b"x" * 256,
        b"type: " + b"y" * 4097,
        b"config: {a: {b: {c: {d: {e: {f: {g: {h: {i: too_deep}}}}}}}}}",
    ]
    for index, seed in enumerate(SEEDS):
        token = tokens[index % len(tokens)]
        split = len(seed) // 2
        cases.append(seed[:split] + token + seed[split:])
    cases.append(b"x" * (1024 * 1024 + 1))
    return cases


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--timeout", default=5.0, type=float)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="topoexec-fuzz-") as tmp:
        tmpdir = Path(tmp)
        for index, content in enumerate(mutated_cases()):
            path = tmpdir / f"case_{index:02d}.yaml"
            path.write_bytes(content)
            try:
                completed = subprocess.run(
                    [str(args.topoexec), "graph", "validate", str(path), "--format", "json"],
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=args.timeout,
                    check=False,
                )
            except subprocess.TimeoutExpired:
                sys.stderr.write(f"timeout while validating fuzz case {index}: {path}\n")
                return 1

            combined = completed.stdout + completed.stderr
            if completed.returncode < 0 or completed.returncode >= 128:
                sys.stderr.write(f"crash-like exit {completed.returncode} for fuzz case {index}: {path}\n")
                sys.stderr.write(combined)
                return 1
            for marker in CRASH_MARKERS:
                if marker in combined:
                    sys.stderr.write(f"crash marker {marker!r} for fuzz case {index}: {path}\n")
                    sys.stderr.write(combined)
                    return 1
            print(f"ok fuzz case {index:02d} rc={completed.returncode}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
