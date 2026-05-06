"""Small stdlib-only Python preview client for TopoExec CLI automation."""

from __future__ import annotations

import json
import subprocess
import tempfile
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, Mapping, Sequence

JsonObject = Mapping[str, Any]


class TopoExecError(RuntimeError):
    """Base error for the Python automation preview."""


@dataclass(frozen=True)
class CommandResult:
    """Captured JSON command result from the CLI-backed preview."""

    argv: tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str
    data: JsonObject

    @property
    def ok(self) -> bool:
        return self.returncode == 0 and bool(self.data.get("ok", True))


class TopoExecCommandError(TopoExecError):
    """Raised when a CLI command fails and check=True was requested."""

    def __init__(self, result: CommandResult):
        self.result = result
        message = (
            result.stderr.strip()
            or result.stdout.strip()
            or f"TopoExec command failed: {result.argv!r}"
        )
        super().__init__(message)


@dataclass(frozen=True)
class GraphDocument:
    """A graph path or in-memory YAML document for CLI-backed automation."""

    path: Path | None = None
    text: str | None = None
    name: str = "topoexec-python-preview.yaml"

    @classmethod
    def from_file(cls, path: str | Path) -> "GraphDocument":
        return cls(path=Path(path))

    @classmethod
    def from_text(cls, text: str, *, name: str = "topoexec-python-preview.yaml") -> "GraphDocument":
        if not name.endswith((".yaml", ".yml")):
            name = f"{name}.yaml"
        return cls(text=text, name=name)


GraphInput = str | Path | GraphDocument


class TopoExecClient:
    """Preview client that shells out to the TopoExec CLI and parses JSON output."""

    def __init__(self, executable: str | Path = "topoexec") -> None:
        self.executable = str(executable)

    def validate(
        self, graph: GraphInput, *, strict_diagnostics: bool = False, check: bool = True
    ) -> CommandResult:
        args = ["graph", "validate", "--format", "json"]
        if strict_diagnostics:
            args.append("--strict-diagnostics")
        return self._graph_command(args, graph, check=check)

    def plan(self, graph: GraphInput, *, check: bool = True) -> CommandResult:
        return self._graph_command(["graph", "plan", "--format", "json"], graph, check=check)

    def run(
        self, graph: GraphInput, *, steps: int = 1, until_idle: bool = False, check: bool = True
    ) -> CommandResult:
        return self._bounded_runtime_command("run", graph, steps=steps, until_idle=until_idle, check=check)

    def metrics(
        self, graph: GraphInput, *, steps: int = 1, until_idle: bool = False, check: bool = True
    ) -> CommandResult:
        return self._bounded_runtime_command("metrics", graph, steps=steps, until_idle=until_idle, check=check)

    def trace(
        self, graph: GraphInput, *, steps: int = 1, until_idle: bool = False, check: bool = True
    ) -> CommandResult:
        return self._bounded_runtime_command("trace", graph, steps=steps, until_idle=until_idle, check=check)

    def _bounded_runtime_command(
        self, subcommand: str, graph: GraphInput, *, steps: int, until_idle: bool, check: bool
    ) -> CommandResult:
        if steps < 0:
            raise ValueError("steps must be non-negative")
        args = ["graph", subcommand, "--format", "json", "--steps", str(steps)]
        if until_idle:
            args.append("--until-idle")
        return self._graph_command(args, graph, check=check)

    def _graph_command(self, args: Sequence[str], graph: GraphInput, *, check: bool) -> CommandResult:
        with materialized_graph(graph) as graph_path:
            return self._json_command([*args, str(graph_path)], check=check)

    def _json_command(self, args: Sequence[str], *, check: bool) -> CommandResult:
        argv = (self.executable, *args)
        completed = subprocess.run(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        try:
            data = json.loads(completed.stdout) if completed.stdout.strip() else {}
        except json.JSONDecodeError as error:
            raise TopoExecError(
                f"TopoExec command did not produce JSON: {argv!r}\nstdout={completed.stdout}\nstderr={completed.stderr}"
            ) from error
        result = CommandResult(
            argv=tuple(argv),
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
            data=data,
        )
        if check and completed.returncode != 0:
            raise TopoExecCommandError(result)
        return result


@contextmanager
def materialized_graph(graph: GraphInput) -> Iterator[Path]:
    if isinstance(graph, GraphDocument):
        if graph.path is not None:
            yield graph.path
            return
        if graph.text is None:
            raise ValueError("GraphDocument requires path or text")
        filename = Path(graph.name).name or "topoexec-python-preview.yaml"
        if not Path(filename).suffix:
            filename = f"{filename}.yaml"
        with tempfile.TemporaryDirectory(prefix="topoexec-python-preview-") as directory:
            path = Path(directory) / filename
            path.write_text(graph.text, encoding="utf-8")
            yield path
            return
    yield Path(graph)
