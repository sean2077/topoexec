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


def expand(command: str, *, source_dir: Path, build_dir: Path, topoexec: Path) -> str:
    return (
        command.replace("${SOURCE_DIR}", str(source_dir))
        .replace("${BUILD_DIR}", str(build_dir))
        .replace("${TOPOEXEC}", str(topoexec))
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--topoexec", required=True, type=Path)
    args = parser.parse_args()

    docs_dir = args.source_dir / "docs"
    commands: list[tuple[Path, int, str]] = []
    for path in sorted(docs_dir.glob("*.md")):
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
