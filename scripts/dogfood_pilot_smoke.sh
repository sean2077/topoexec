#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOPOEXEC="${TOPOEXEC:-$ROOT_DIR/build/topoexec}"
GRAPH="${TOPOEXEC_DOGFOOD_GRAPH:-examples/90-dogfood-pilot/dogfood_robot_cell.yaml}"
ASSERTIONS="${TOPOEXEC_DOGFOOD_ASSERTIONS:-examples/90-dogfood-pilot/assertions.yaml}"
STEPS="${TOPOEXEC_DOGFOOD_STEPS:-20}"
RUN_DIR="${TOPOEXEC_DOGFOOD_RUN_DIR:-/tmp/topoexec-dogfood-pilot-record}"

cd "$ROOT_DIR"
rm -rf "$RUN_DIR"

"$TOPOEXEC" graph validate "$GRAPH" >/dev/null
"$TOPOEXEC" graph plan "$GRAPH" --format json > /tmp/topoexec-dogfood-plan.json
"$TOPOEXEC" graph render "$GRAPH" --format mermaid > /tmp/topoexec-dogfood-graph.mmd
"$TOPOEXEC" graph run "$GRAPH" --steps "$STEPS" > /tmp/topoexec-dogfood-run.txt
"$TOPOEXEC" graph metrics "$GRAPH" --steps "$STEPS" --format json > /tmp/topoexec-dogfood-metrics.json
"$TOPOEXEC" graph trace "$GRAPH" --steps "$STEPS" --format json > /tmp/topoexec-dogfood-trace.json
"$TOPOEXEC" graph trace "$GRAPH" --steps "$STEPS" --format chrome > /tmp/topoexec-dogfood-trace.chrome.json
"$TOPOEXEC" graph observe "$GRAPH" --steps "$STEPS" --observe-level summary \
  --assert "$ASSERTIONS" --record "$RUN_DIR" --format ndjson > /tmp/topoexec-dogfood-observe.ndjson
python3 tools/topoexec_live_server.py replay "$RUN_DIR" --smoke >/dev/null
"$TOPOEXEC" graph bench "$GRAPH" --steps 3 --runs 2 --format json > /tmp/topoexec-dogfood-bench.json

python3 - <<'PY'
from __future__ import annotations
import json
from pathlib import Path
metrics = json.loads(Path('/tmp/topoexec-dogfood-metrics.json').read_text(encoding='utf-8'))
trace = json.loads(Path('/tmp/topoexec-dogfood-trace.json').read_text(encoding='utf-8'))
observe = [json.loads(line) for line in Path('/tmp/topoexec-dogfood-observe.ndjson').read_text(encoding='utf-8').splitlines() if line.strip()]
assert metrics.get('ok') is True
assert metrics.get('runtime_error_count', 0) == 0
assert metrics.get('channel_publish_count', 0) > 0
assert metrics.get('channel_delivery_count', 0) > 0
assert metrics.get('channel_drop_count', 0) >= 0
assert metrics.get('channel_overwrite_count', 0) >= 0
assert metrics.get('channel_reject_count', 0) == 0
assert trace.get('ok') is True
assert trace.get('trace_event_count', 0) > 0
assert observe[-1].get('runtime_ok') is True
assert observe[-1].get('assertions_ok') is True
assert observe[-1].get('observer_dropped_event_count') == 0
print(json.dumps({
  'ok': True,
  'graph': 'examples/90-dogfood-pilot/dogfood_robot_cell.yaml',
  'steps': 20,
  'channel_drop_count': metrics.get('channel_drop_count'),
  'channel_overwrite_count': metrics.get('channel_overwrite_count'),
  'channel_reject_count': metrics.get('channel_reject_count'),
  'trace_event_count': trace.get('trace_event_count'),
  'observe_records': len(observe),
}))
PY

echo '[dogfood] synthetic pilot smoke passed'
