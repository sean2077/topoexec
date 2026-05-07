#!/usr/bin/env python3
"""Generate or check README/example showcase assets from real TopoExec examples.

The script intentionally uses only the Python standard library and the local
`topoexec` CLI so README visuals can be rebuilt in CI without a frontend stack.
It writes Mermaid source, lightweight SVG topology cards, summary JSON, and
optional metrics/trace summary cards under docs/assets/generated/.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_METADATA = {
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


def repo_path(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def load_examples(examples_root: Path) -> list[tuple[Path, dict[str, Any]]]:
    examples: list[tuple[Path, dict[str, Any]]] = []
    for metadata_path in sorted(examples_root.glob("[0-9][0-9]-*/example.json")):
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        missing = REQUIRED_METADATA - set(metadata)
        if missing:
            raise SystemExit(f"{metadata_path}: missing metadata keys: {sorted(missing)}")
        graph = metadata_path.parent / metadata["graph"]
        if not graph.exists():
            raise SystemExit(f"{metadata_path}: graph file does not exist: {graph}")
        examples.append((metadata_path.parent, metadata))
    if not examples:
        raise SystemExit(f"no curated example metadata found under {examples_root}")
    return examples


def run_command(argv: list[str], *, cwd: Path = ROOT, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if expect_success and proc.returncode != 0:
        raise SystemExit(f"command failed ({proc.returncode}): {' '.join(argv)}\n{proc.stdout}")
    if not expect_success and proc.returncode == 0:
        raise SystemExit(f"command unexpectedly succeeded: {' '.join(argv)}")
    return proc


def command_args(topoexec: Path, command: dict[str, Any]) -> list[str]:
    argv = command["argv"]
    if not argv or argv[0] != "graph":
        raise SystemExit(f"unsupported example command: {argv!r}")
    return [str(topoexec)] + argv


def plan_for(topoexec: Path, graph: Path) -> dict[str, Any]:
    proc = run_command([str(topoexec), "graph", "plan", repo_path(graph), "--format", "json"])
    return json.loads(proc.stdout)


def render_mermaid(topoexec: Path, graph: Path) -> str:
    proc = run_command([str(topoexec), "graph", "render", repo_path(graph), "--format", "mermaid"])
    return proc.stdout.rstrip() + "\n"


def text_el(x: int, y: int, text: str, *, size: int = 13, weight: str = "400", color: str = "#1f2937") -> str:
    return f'<text x="{x}" y="{y}" font-family="Inter,Segoe UI,Arial,sans-serif" font-size="{size}" font-weight="{weight}" fill="{color}">{html.escape(text)}</text>'


def truncate(value: str, length: int) -> str:
    return value if len(value) <= length else value[: max(0, length - 1)] + "…"


def graph_svg(metadata: dict[str, Any], plan: dict[str, Any]) -> str:
    components = plan.get("components", [])
    edges = plan.get("edges", [])
    order = plan.get("region_order") or [component["id"] for component in components]
    by_id = {component["id"]: component for component in components}
    ordered = [by_id[cid] for cid in order if cid in by_id]
    for component in components:
        if component["id"] not in {c["id"] for c in ordered}:
            ordered.append(component)

    node_w = 190
    node_h = 72
    gap_x = 36
    gap_y = 38
    cols = 3 if len(ordered) <= 6 else 4
    rows = max(1, (len(ordered) + cols - 1) // cols)
    width = max(760, 48 + cols * node_w + (cols - 1) * gap_x + 48)
    height = 128 + rows * node_h + (rows - 1) * gap_y + 80
    positions: dict[str, tuple[int, int]] = {}
    body: list[str] = []

    body.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">')
    body.append(f'<title id="title">{html.escape(metadata["title"])} topology</title>')
    body.append(f'<desc id="desc">Generated TopoExec graph visual with {len(components)} components and {len(edges)} edges.</desc>')
    body.append('<rect width="100%" height="100%" rx="18" fill="#f8fafc"/>')
    body.append('<rect x="14" y="14" width="%d" height="%d" rx="16" fill="#ffffff" stroke="#dbeafe"/>' % (width - 28, height - 28))
    body.append(text_el(36, 50, metadata["title"], size=22, weight="700", color="#0f172a"))
    body.append(text_el(36, 76, metadata["summary"], size=13, color="#475569"))
    body.append(text_el(36, 100, f'{len(components)} components · {len(edges)} edges · {metadata["category"]}', size=12, weight="600", color="#2563eb"))

    start_x = 48
    start_y = 128
    for idx, component in enumerate(ordered):
        col = idx % cols
        row = idx // cols
        x = start_x + col * (node_w + gap_x)
        y = start_y + row * (node_h + gap_y)
        positions[component["id"]] = (x, y)

    marker = '<defs><marker id="arrow" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto" markerUnits="strokeWidth"><path d="M0,0 L0,6 L7,3 z" fill="#94a3b8"/></marker></defs>'
    body.append(marker)
    for edge in edges[:24]:
        src = edge.get("from", "").split(".")[0]
        dst = edge.get("to", "").split(".")[0]
        if src not in positions or dst not in positions:
            continue
        sx, sy = positions[src]
        dx, dy = positions[dst]
        x1 = sx + node_w
        y1 = sy + node_h // 2
        x2 = dx
        y2 = dy + node_h // 2
        if dx <= sx:
            x1 = sx + node_w // 2
            y1 = sy + node_h
            x2 = dx + node_w // 2
            y2 = dy
        body.append(f'<path d="M{x1},{y1} C{(x1+x2)//2},{y1} {(x1+x2)//2},{y2} {x2},{y2}" fill="none" stroke="#94a3b8" stroke-width="1.5" marker-end="url(#arrow)" opacity="0.75"/>')

    for component in ordered:
        x, y = positions[component["id"]]
        role = component.get("boundary_role", "processing")
        fill = "#eff6ff" if role == "processing" else "#ecfdf5"
        stroke = "#60a5fa" if role == "processing" else "#34d399"
        body.append(f'<rect x="{x}" y="{y}" width="{node_w}" height="{node_h}" rx="12" fill="{fill}" stroke="{stroke}" stroke-width="1.4"/>')
        body.append(text_el(x + 14, y + 28, truncate(component["id"], 22), size=14, weight="700", color="#0f172a"))
        body.append(text_el(x + 14, y + 50, truncate(component.get("type", ""), 28), size=11, color="#475569"))

    body.append(text_el(36, height - 30, "Generated from example metadata and `topoexec graph render/plan`; no hand-drawn diagram.", size=11, color="#64748b"))
    body.append("</svg>\n")
    return "\n".join(body)


def summary_svg(title: str, lines: list[str], accent: str = "#2563eb") -> str:
    width = 760
    height = 120 + 24 * max(1, len(lines))
    body = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img">']
    body.append('<rect width="100%" height="100%" rx="18" fill="#f8fafc"/>')
    body.append(f'<rect x="18" y="18" width="{width-36}" height="{height-36}" rx="16" fill="#ffffff" stroke="#e2e8f0"/>')
    body.append(f'<circle cx="48" cy="50" r="12" fill="{accent}"/>')
    body.append(text_el(72, 57, title, size=20, weight="700", color="#0f172a"))
    y = 92
    for line in lines:
        body.append(text_el(42, y, line, size=13, color="#334155"))
        y += 24
    body.append('</svg>\n')
    return "\n".join(body)


def metrics_summary(topoexec: Path, graph: Path) -> tuple[dict[str, Any], str]:
    proc = run_command([str(topoexec), "graph", "metrics", repo_path(graph), "--steps", "1", "--format", "json"])
    data = json.loads(proc.stdout)
    summary = {
        "metric_schema_version": data.get("metric_schema_version"),
        "graph_name": data.get("graph_name"),
        "component_count": data.get("component_count"),
        "channel_count": data.get("channel_count"),
        "channel_publish_count": data.get("channel_publish_count"),
        "channel_delivery_count": data.get("channel_delivery_count"),
        "channel_drop_count": data.get("channel_drop_count"),
        "health_event_count": data.get("health_event_count"),
    }
    lines = [f"{key}: {value}" for key, value in summary.items()]
    return summary, summary_svg(f"Metrics summary · {graph.stem}", lines, "#16a34a")


def trace_summary(topoexec: Path, graph: Path) -> tuple[dict[str, Any], str]:
    proc = run_command([str(topoexec), "graph", "trace", repo_path(graph), "--steps", "1", "--format", "json"])
    data = json.loads(proc.stdout)
    events = data.get("trace", []) or []
    if not events and data.get("trace_events"):
        events = data.get("trace_events", [])
    kinds: dict[str, int] = {}
    for event in events:
        if isinstance(event, dict):
            kind = str(event.get("event") or event.get("name") or event.get("kind") or "unknown")
        else:
            kind = str(event)
        kinds[kind] = kinds.get(kind, 0) + 1
    summary = {
        "trace_schema_version": data.get("trace_schema_version"),
        "graph_name": data.get("graph_name"),
        "ok": data.get("ok"),
        "trace_event_count": data.get("trace_event_count", len(events)),
        "event_kinds": kinds,
    }
    lines = [f"{key}: {value}" for key, value in summary.items() if key != "event_kinds"]
    lines.extend(f"{key}: {value}" for key, value in sorted(kinds.items())[:6])
    return summary, summary_svg(f"Trace summary · {graph.stem}", lines, "#7c3aed")


def write(path: Path, content: str, *, check: bool, binary: bool = False) -> bool:
    if check:
        if not path.exists():
            print(f"missing generated file: {repo_path(path)}", file=sys.stderr)
            return False
        old = path.read_bytes() if binary else path.read_text(encoding="utf-8")
        if old != (content.encode("utf-8") if binary else content):
            print(f"generated file is stale: {repo_path(path)}", file=sys.stderr)
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    if binary:
        path.write_bytes(content.encode("utf-8"))
    else:
        path.write_text(content, encoding="utf-8")
    return True


def write_json(path: Path, data: Any, *, check: bool) -> bool:
    return write(path, json.dumps(data, indent=2, sort_keys=True) + "\n", check=check)


def copy_generated(src: Path, dst: Path, *, check: bool) -> bool:
    if check:
        if not dst.exists():
            print(f"missing generated file: {repo_path(dst)}", file=sys.stderr)
            return False
        if src.read_bytes() != dst.read_bytes():
            print(f"generated file is stale: {repo_path(dst)}", file=sys.stderr)
            return False
        return True
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)
    return True


def generate(args: argparse.Namespace) -> bool:
    examples_root = (ROOT / args.examples_root).resolve()
    output_root = (ROOT / args.output_root).resolve()
    topoexec = (ROOT / args.topoexec).resolve() if not Path(args.topoexec).is_absolute() else Path(args.topoexec)
    if not topoexec.exists():
        raise SystemExit(f"topoexec executable not found: {topoexec}")
    examples = load_examples(examples_root)
    ok = True
    showcase: list[dict[str, Any]] = []
    generated_by_id: dict[str, Path] = {}

    for example_dir, metadata in examples:
        graph = example_dir / metadata["graph"]
        outdir = output_root / "examples" / metadata["id"]
        validate_proc = run_command([str(topoexec), "graph", "validate", repo_path(graph)], expect_success=True)
        mermaid = render_mermaid(topoexec, graph)
        plan = plan_for(topoexec, graph)
        graph_card = graph_svg(metadata, plan)
        summary = {
            "id": metadata["id"],
            "title": metadata["title"],
            "category": metadata["category"],
            "difficulty": metadata["difficulty"],
            "graph": repo_path(graph),
            "component_count": len(plan.get("components", [])),
            "edge_count": len(plan.get("edges", [])),
            "region_order": plan.get("region_order", []),
            "commands": metadata["commands"],
            "validate_first_line": validate_proc.stdout.splitlines()[0] if validate_proc.stdout.splitlines() else "",
            "assets": metadata["assets"],
            "tags": metadata["tags"],
        }
        ok &= write(outdir / "graph.mmd", mermaid, check=args.check)
        ok &= write(outdir / "graph.svg", graph_card, check=args.check)
        ok &= write_json(outdir / "summary.json", summary, check=args.check)
        generated_by_id[metadata["id"]] = outdir / "graph.svg"
        if metadata["assets"].get("generate_metrics_summary"):
            metrics, svg = metrics_summary(topoexec, graph)
            ok &= write_json(outdir / "metrics_summary.json", metrics, check=args.check)
            ok &= write(outdir / "metrics_summary.svg", svg, check=args.check)
        if metadata["assets"].get("generate_trace_summary"):
            trace, svg = trace_summary(topoexec, graph)
            ok &= write_json(outdir / "trace_summary.json", trace, check=args.check)
            ok &= write(outdir / "trace_summary.svg", svg, check=args.check)
        showcase.append({
            "id": metadata["id"],
            "title": metadata["title"],
            "category": metadata["category"],
            "summary": metadata["summary"],
            "graph_asset": repo_path(outdir / "graph.svg"),
            "show_in_readme": metadata.get("show_in_readme", False),
            "readme_priority": metadata.get("readme_priority", 999),
        })

    readme_dir = output_root / "readme"
    readme_assets = {
        "hero.svg": "minimal-pipeline",
        "minimal-pipeline.svg": "minimal-pipeline",
        "trigger-semantics.svg": "trigger-semantics",
        "observability.svg": "metrics-trace-observe",
        "composite-loop.svg": "composite-loop-solver",
    }
    for filename, example_id in readme_assets.items():
        src = generated_by_id.get(example_id)
        if src is not None:
            ok &= copy_generated(src, readme_dir / filename, check=args.check)
    ok &= write_json(readme_dir / "showcase.json", sorted(showcase, key=lambda item: item["readme_priority"]), check=args.check)
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--topoexec", default=os.environ.get("TOPOEXEC", "build/topoexec"))
    parser.add_argument("--examples-root", default="examples")
    parser.add_argument("--output-root", default="docs/assets/generated")
    parser.add_argument("--check", action="store_true", help="fail if generated files are missing or stale")
    args = parser.parse_args()
    ok = generate(args)
    if not ok:
        return 1
    mode = "check" if args.check else "generate"
    print(json.dumps({"ok": True, "mode": mode, "output_root": args.output_root}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
