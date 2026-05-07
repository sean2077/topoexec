#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_live_dashboard.py <topoexec> <artifact-dir>", file=sys.stderr)
        return 1
    topoexec = Path(sys.argv[1])
    artifact = Path(sys.argv[2])
    for asset in [
        Path("tools/topoexec_live_dashboard/index.html"),
        Path("tools/topoexec_live_dashboard/app.js"),
        Path("tools/topoexec_live_dashboard/style.css"),
    ]:
        text = asset.read_text(encoding="utf-8")
        assert "cdn" not in text.lower()
        assert "WebSocket" not in text
    result = subprocess.run(
        [
            sys.executable,
            "tools/topoexec_live_server.py",
            "run",
            "examples/minimal.yaml",
            "--topoexec",
            str(topoexec),
            "--steps",
            "3",
            "--record",
            str(artifact),
            "--smoke",
        ],
        check=False,
    )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
