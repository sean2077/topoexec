#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_live_replay.py <artifact-dir>", file=sys.stderr)
        return 1
    artifact = Path(sys.argv[1])
    return subprocess.run([sys.executable, "tools/topoexec_live_server.py", "replay", str(artifact), "--smoke"], check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
