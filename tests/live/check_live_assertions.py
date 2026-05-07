#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_live_assertions.py <topoexec> <workdir>", file=sys.stderr)
        return 1
    topoexec = Path(sys.argv[1])
    workdir = Path(sys.argv[2])
    ndjson = workdir / "observe-assert.ndjson"
    with ndjson.open("w", encoding="utf-8") as out:
        subprocess.run(
            [str(topoexec), "graph", "observe", "examples/minimal.yaml", "--steps", "3", "--assert", "tests/live/minimal_pass.assert.yaml", "--format", "ndjson"],
            cwd=Path.cwd(),
            stdout=out,
            check=True,
        )
    records = [json.loads(line) for line in ndjson.read_text(encoding="utf-8").splitlines() if line.strip()]
    kinds = {record.get("kind") for record in records}
    assert "assertion_registered" in kinds
    assert "assertion_pass" in kinds
    assert "assertion_result" in kinds
    assert records[-1]["assertions_ok"] is True

    failed = subprocess.run(
        [str(topoexec), "graph", "observe", "examples/minimal.yaml", "--steps", "1", "--assert", "tests/live/expected_fail.assert.yaml", "--format", "ndjson"],
        cwd=Path.cwd(),
        stdout=subprocess.PIPE,
        text=True,
        check=False,
    )
    assert failed.returncode == 3
    assert '"assertion_fail"' in failed.stdout

    pending = subprocess.run(
        [str(topoexec), "graph", "observe", "examples/minimal.yaml", "--steps", "1", "--assert", "tests/live/pending.assert.yaml", "--format", "ndjson", "--no-fail-on-assertion-fail"],
        cwd=Path.cwd(),
        stdout=subprocess.PIPE,
        text=True,
        check=False,
    )
    assert pending.returncode == 0
    assert '"assertion_pending"' in pending.stdout

    tool_result = workdir / "assertion_result.json"
    tool = subprocess.run(
        [sys.executable, "tools/topoexec_live_assert.py", "tests/live/minimal_pass.assert.yaml", str(ndjson), "--result", str(tool_result)],
        cwd=Path.cwd(),
        check=False,
    )
    assert tool.returncode == 0
    assert json.loads(tool_result.read_text(encoding="utf-8"))["ok"] is True
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
