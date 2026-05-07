#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build"}"
TOPOEXEC="${TOPOEXEC:-"$BUILD_DIR/topoexec"}"
PYTHON="${PYTHON:-python3}"
WORK_DIR="${TOPOEXEC_LIVE_SMOKE_DIR:-}"

if [[ -z "$WORK_DIR" ]]; then
  WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/topoexec-live-smoke.XXXXXX")"
  CLEANUP_WORK_DIR=1
else
  mkdir -p "$WORK_DIR"
  CLEANUP_WORK_DIR=0
fi

cleanup() {
  if [[ "${CLEANUP_WORK_DIR:-0}" == "1" ]]; then
    rm -rf "$WORK_DIR"
  fi
}
trap cleanup EXIT

cd "$ROOT_DIR"

require_file() {
  if [[ ! -x "$1" && ! -f "$1" ]]; then
    echo "missing required file: $1" >&2
    exit 1
  fi
}

require_file "$TOPOEXEC"

echo "[live] observe NDJSON schema smoke"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 3 --observe-level summary --format ndjson \
  > "$WORK_DIR/observe.ndjson"
"$PYTHON" tests/live/check_observe_schema.py "$WORK_DIR/observe.ndjson"

echo "[live] output filter smoke"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 3 --observe-level summary \
  --include-event component_end --format ndjson > "$WORK_DIR/include-event.ndjson"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 3 --observe-level summary \
  --include-component transform --format ndjson > "$WORK_DIR/include-component.ndjson"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 3 --observe-level summary \
  --exclude-event component_end --format ndjson > "$WORK_DIR/exclude-event.ndjson"
"$PYTHON" - "$WORK_DIR/include-event.ndjson" "$WORK_DIR/include-component.ndjson" "$WORK_DIR/exclude-event.ndjson" <<'PY'
import json
import sys
from pathlib import Path

def events(path):
    return [json.loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines() if line.strip()]

include_event = [record for record in events(sys.argv[1]) if 'display_seq' in record]
if not include_event or any(record.get('kind') != 'component_end' for record in include_event):
    raise SystemExit('include-event did not restrict display events to component_end')
include_component = [record for record in events(sys.argv[2]) if 'display_seq' in record]
if not include_component or any(record.get('component_id') != 'transform' for record in include_component):
    raise SystemExit('include-component did not restrict display events to transform')
exclude_event = [record for record in events(sys.argv[3]) if 'display_seq' in record]
if not exclude_event or any(record.get('kind') == 'component_end' for record in exclude_event):
    raise SystemExit('exclude-event did not remove component_end display events')
print(json.dumps({'ok': True, 'filter_smoke': True}))
PY

echo "[live] assertion pass/fail/pending smoke"
"$PYTHON" tests/live/check_live_assertions.py "$TOPOEXEC" "$WORK_DIR"

echo "[live] record artifact smoke"
rm -rf "$WORK_DIR/artifact"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 3 \
  --assert tests/live/minimal_pass.assert.yaml \
  --record "$WORK_DIR/artifact" --format ndjson > "$WORK_DIR/artifact.ndjson"
"$PYTHON" tests/live/check_live_artifact.py "$WORK_DIR/artifact"

echo "[live] replay smoke"
"$PYTHON" tests/live/check_live_replay.py "$WORK_DIR/artifact"

echo "[live] dashboard smoke"
rm -rf "$WORK_DIR/dashboard-artifact"
"$PYTHON" tests/live/check_live_dashboard.py "$TOPOEXEC" "$WORK_DIR/dashboard-artifact"

echo "[live] observer drop summary smoke"
"$TOPOEXEC" graph observe examples/minimal.yaml --steps 5 --observe-level summary \
  --event-buffer-capacity 1 --format ndjson > "$WORK_DIR/drop.ndjson"
"$PYTHON" - "$WORK_DIR/drop.ndjson" <<'PY'
import json
import sys
from pathlib import Path
records = [json.loads(line) for line in Path(sys.argv[1]).read_text(encoding='utf-8').splitlines() if line.strip()]
kinds = {record.get('kind') for record in records}
if 'observer_drop_summary' not in kinds:
    raise SystemExit('observer_drop_summary was not emitted')
final = next((record for record in reversed(records) if record.get('kind') == 'final_summary'), {})
if int(final.get('observer_dropped_event_count', 0)) <= 0:
    raise SystemExit('final_summary did not report observer_dropped_event_count > 0')
print(json.dumps({'ok': True, 'observer_dropped_event_count': final.get('observer_dropped_event_count')}))
PY

echo "[live] ok: $WORK_DIR"
