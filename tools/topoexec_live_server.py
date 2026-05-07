#!/usr/bin/env python3
"""Local observe-only TopoExec live dashboard server."""
from __future__ import annotations

import argparse
import json
import secrets
import subprocess
import sys
import tempfile
import time
import urllib.parse
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DASHBOARD = ROOT / "tools" / "topoexec_live_dashboard"
DEFAULT_RAW_LIMIT = 1000


def load_artifact(path: Path, raw_limit: int = DEFAULT_RAW_LIMIT) -> dict[str, Any]:
    manifest_path = path / "manifest.json"
    observe_path = path / "observe.ndjson"
    if not manifest_path.exists() or not observe_path.exists():
        raise SystemExit(f"artifact must contain manifest.json and observe.ndjson: {path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    records = [json.loads(line) for line in observe_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    final = next((record for record in reversed(records) if record.get("kind") == "final_summary"), {})
    return {
        "artifact_dir": str(path),
        "manifest": manifest,
        "records": records,
        "raw_events": records[-raw_limit:],
        "final_summary": final,
        "observe_level": manifest.get("observe_level", final.get("observe_level", "summary")),
        "observer_dropped_event_count": final.get("observer_dropped_event_count", 0),
        "exactness": final.get("exactness", "partial"),
    }


def run_artifact(args: argparse.Namespace) -> Path:
    record_dir = Path(args.record or tempfile.mkdtemp(prefix="topoexec-live-"))
    topoexec = Path(args.topoexec)
    command = [
        str(topoexec),
        "graph",
        "observe",
        str(args.graph),
        "--steps",
        str(args.steps),
        "--observe-level",
        args.observe_level,
        "--record",
        str(record_dir),
        "--format",
        "ndjson",
    ]
    if args.assertions:
        command.extend(["--assert", str(args.assertions)])
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return record_dir


class LiveHandler(BaseHTTPRequestHandler):
    server: "LiveServer"

    def _token_ok(self) -> bool:
        query = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        return query.get("token", [""])[0] == self.server.token

    def _reject(self) -> None:
        self.send_error(HTTPStatus.FORBIDDEN, "invalid or missing token")

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path not in {"/", "/index.html", "/app.js", "/style.css"} and not self._token_ok():
            self._reject()
            return
        if parsed.path in {"/", "/index.html"}:
            self._send_file(DASHBOARD / "index.html", "text/html; charset=utf-8")
        elif parsed.path == "/app.js":
            self._send_file(DASHBOARD / "app.js", "text/javascript; charset=utf-8")
        elif parsed.path == "/style.css":
            self._send_file(DASHBOARD / "style.css", "text/css; charset=utf-8")
        elif parsed.path == "/snapshot":
            self._send_json({k: v for k, v in self.server.artifact.items() if k != "records"})
        elif parsed.path == "/bundle":
            files = sorted(p.name for p in Path(self.server.artifact["artifact_dir"]).iterdir() if p.is_file())
            self._send_json({"files": files})
        elif parsed.path == "/events":
            self._send_events()
        else:
            self.send_error(HTTPStatus.NOT_FOUND)

    def log_message(self, fmt: str, *args: Any) -> None:
        if self.server.verbose:
            super().log_message(fmt, *args)

    def _send_file(self, path: Path, content_type: str) -> None:
        body = path.read_bytes()
        if path.name == "index.html":
            body = body.replace(b"__TOKEN__", self.server.token.encode())
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, value: Any) -> None:
        body = json.dumps(value, indent=2).encode()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_events(self) -> None:
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        frame_ms = max(1, int(self.server.frame_ms))
        for record in self.server.artifact["records"]:
            self.wfile.write(b"event: observe\n")
            self.wfile.write(b"data: " + json.dumps(record).encode() + b"\n\n")
            self.wfile.flush()
            if self.server.replay_delay:
                time.sleep(frame_ms / 1000.0)


class LiveServer(ThreadingHTTPServer):
    def __init__(self, address: tuple[str, int], artifact: dict[str, Any], token: str, frame_ms: int, replay_delay: bool, verbose: bool):
        super().__init__(address, LiveHandler)
        self.artifact = artifact
        self.token = token
        self.frame_ms = frame_ms
        self.replay_delay = replay_delay
        self.verbose = verbose


def smoke(path: Path) -> int:
    artifact = load_artifact(path)
    required_assets = [DASHBOARD / "index.html", DASHBOARD / "app.js", DASHBOARD / "style.css"]
    missing = [str(asset) for asset in required_assets if not asset.exists()]
    if missing:
        raise SystemExit(f"missing dashboard assets: {missing}")
    if not artifact["records"] or artifact["records"][0].get("kind") != "symbol_table":
        raise SystemExit("observe.ndjson does not start with symbol_table")
    if artifact["final_summary"].get("kind") != "final_summary":
        raise SystemExit("observe.ndjson missing final_summary")
    print(json.dumps({"ok": True, "records": len(artifact["records"]), "artifact": str(path)}))
    return 0


def serve(path: Path, args: argparse.Namespace) -> int:
    artifact = load_artifact(path, args.raw_limit)
    token = secrets.token_urlsafe(16)
    server = LiveServer((args.host, args.port), artifact, token, args.ui_frame_ms, args.replay_delay, args.verbose)
    host, port = server.server_address
    url = f"http://{host}:{port}/?token={token}"
    print(url)
    if args.open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        return 130
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run", help="run graph observe, record, then serve")
    run.add_argument("graph", type=Path)
    run.add_argument("--topoexec", default=str(ROOT / "build" / "topoexec"))
    run.add_argument("--steps", type=int, default=100)
    run.add_argument("--observe-level", choices=["summary", "detailed", "debug"], default="summary")
    run.add_argument("--assert", dest="assertions", type=Path)
    run.add_argument("--record", type=Path)
    replay = sub.add_parser("replay", help="serve a recorded live artifact")
    replay.add_argument("artifact", type=Path)
    for cmd in (run, replay):
        cmd.add_argument("--host", default="127.0.0.1")
        cmd.add_argument("--port", type=int, default=8765)
        cmd.add_argument("--ui-frame-ms", type=int, default=50)
        cmd.add_argument("--raw-limit", type=int, default=DEFAULT_RAW_LIMIT)
        cmd.add_argument("--open", action="store_true")
        cmd.add_argument("--smoke", action="store_true")
        cmd.add_argument("--replay-delay", action="store_true")
        cmd.add_argument("--verbose", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    artifact = run_artifact(args) if args.command == "run" else args.artifact
    if args.smoke:
        return smoke(artifact)
    return serve(artifact, args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
