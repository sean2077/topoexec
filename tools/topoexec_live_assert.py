#!/usr/bin/env python3
"""Evaluate TopoExec live assertion YAML against observe NDJSON.

This is a tooling-layer evaluator: it reads existing observe output and never
executes user code or hooks into runtime internals.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

try:
    import yaml  # type: ignore
except ModuleNotFoundError:  # pragma: no cover - exercised in minimal CI envs.
    yaml = None


def _scalar(value: str) -> Any:
    value = value.strip().strip("\"'")
    if value.isdigit():
        return int(value)
    try:
        return float(value)
    except ValueError:
        return value


def _fallback_load_assertions(text: str) -> dict[str, Any]:
    spec: dict[str, Any] = {"assertions": []}
    current: dict[str, Any] | None = None
    nested: str | None = None
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        line = raw.strip()
        if indent == 0 and ":" in line:
            key, value = line.split(":", 1)
            if value.strip():
                spec[key] = _scalar(value)
            continue
        if line.startswith("- "):
            current = {}
            spec["assertions"].append(current)
            nested = None
            item = line[2:]
            if ":" in item:
                key, value = item.split(":", 1)
                current[key] = _scalar(value)
            continue
        if current is None or ":" not in line:
            continue
        key, value = line.split(":", 1)
        if not value.strip():
            current[key] = {}
            nested = key
            continue
        if nested and indent >= 6:
            current.setdefault(nested, {})[key] = _scalar(value)
        else:
            nested = None
            current[key] = _scalar(value)
    return spec


def load_assertion_spec(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    if yaml is not None:
        return yaml.safe_load(text)
    return _fallback_load_assertions(text)


def load_events(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def event_matches(event: dict[str, Any], assertion: dict[str, Any]) -> bool:
    expected_event = assertion.get("event")
    if expected_event and event.get("kind") != expected_event:
        return False
    where = assertion.get("where") or {}
    for key, expected in where.items():
        value = event.get(key)
        if value is None:
            value = (event.get("attributes") or {}).get(key)
        if str(value) != str(expected):
            return False
    return True


def metric_value(events: list[dict[str, Any]], metric: str) -> float:
    final = next((e for e in reversed(events) if e.get("kind") == "final_summary"), {})
    if metric in {"runtime.observer.dropped_event_count", "runtime.live_observe.dropped_event_count"}:
        return float(final.get("observer_dropped_event_count", 0))
    return 0.0


def counter_value(events: list[dict[str, Any]], source: str) -> int:
    if source == "runtime_errors":
        final = next((e for e in reversed(events) if e.get("kind") == "final_summary"), {})
        return int(final.get("runtime_error_count", 0))
    if source in {"observer_drops", "runtime.observer.dropped_event_count"}:
        return int(metric_value(events, source))
    if source == "events":
        return len([e for e in events if "display_seq" in e])
    return int(metric_value(events, source))


def evaluate(assertion_file: Path, events: list[dict[str, Any]]) -> dict[str, Any]:
    spec = load_assertion_spec(assertion_file)
    if str(spec.get("assertion_schema_version")) != "1":
        raise SystemExit("assertion_schema_version must be '1'")
    result: dict[str, Any] = {
        "assertion_schema_version": "1",
        "ok": True,
        "passed": 0,
        "failed": 0,
        "pending": 0,
        "failures": [],
        "events": [],
    }
    observed = [e for e in events if "display_seq" in e]
    for assertion in spec.get("assertions", []):
        assertion_id = assertion.get("id", "unnamed")
        assertion_type = assertion.get("type")
        result["events"].append({"kind": "assertion_registered", "assertion_id": assertion_id})
        passed = False
        pending = False
        reason = ""
        if assertion_type == "counter_equals":
            actual = counter_value(events, assertion.get("source", ""))
            expected = int(assertion.get("value", 0))
            passed = actual == expected
            reason = f"counter expected {expected} got {actual}"
        elif assertion_type in {"metric_equals", "metric_lte", "metric_gte"}:
            actual = metric_value(events, assertion.get("metric", ""))
            expected = float(assertion.get("value", 0))
            if assertion_type == "metric_equals":
                passed = math.isclose(actual, expected)
            elif assertion_type == "metric_lte":
                passed = actual <= expected
            else:
                passed = actual >= expected
            reason = f"metric check expected {expected} got {actual}"
        elif assertion_type in {"eventually", "within_events", "within_epochs"}:
            limit = int(assertion.get("within_events", len(observed)))
            passed = any(event_matches(event, assertion) for event in observed[:limit])
            if not passed and assertion_type == "eventually" and "within_events" not in assertion:
                pending = True
                reason = "eventually condition still pending"
            else:
                reason = "event condition not satisfied"
        elif assertion_type in {"never", "always"}:
            matches = [event_matches(event, assertion) for event in observed]
            passed = not any(matches) if assertion_type == "never" else all(matches)
            reason = "never/always condition failed"
        elif assertion_type == "sequence":
            cursor = 0
            passed = True
            for step in assertion.get("sequence", []):
                found = False
                while cursor < len(observed):
                    if event_matches(observed[cursor], step):
                        cursor += 1
                        found = True
                        break
                    cursor += 1
                if not found:
                    passed = False
                    reason = "sequence step not observed"
                    break
        else:
            reason = f"unsupported assertion type: {assertion_type}"
        if passed:
            result["passed"] += 1
            result["events"].append({"kind": "assertion_pass", "assertion_id": assertion_id})
        elif pending:
            result["ok"] = False
            result["pending"] += 1
            result["events"].append({"kind": "assertion_pending", "assertion_id": assertion_id, "reason": reason})
        else:
            result["ok"] = False
            result["failed"] += 1
            result["failures"].append({"id": assertion_id, "reason": reason})
            result["events"].append({"kind": "assertion_fail", "assertion_id": assertion_id, "reason": reason})
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("assertions", type=Path)
    parser.add_argument("observe_ndjson", type=Path)
    parser.add_argument("--result", type=Path)
    args = parser.parse_args()
    result = evaluate(args.assertions, load_events(args.observe_ndjson))
    output = json.dumps(result, indent=2, sort_keys=True)
    if args.result:
        args.result.write_text(output + "\n", encoding="utf-8")
    print(output)
    return 0 if result["ok"] else 3


if __name__ == "__main__":
    raise SystemExit(main())
