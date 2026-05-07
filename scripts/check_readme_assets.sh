#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python3}"
TOPOEXEC="${TOPOEXEC:-$ROOT_DIR/build/topoexec}"
cd "$ROOT_DIR"

"$PYTHON" scripts/update_examples_index.py --check
"$PYTHON" scripts/render_example_assets.py --topoexec "$TOPOEXEC" --check

"$PYTHON" - <<'PY'
import json
import re
import sys
from pathlib import Path

root = Path.cwd()
required_assets = [
    'docs/assets/generated/readme/hero.svg',
    'docs/assets/generated/readme/minimal-pipeline.svg',
    'docs/assets/generated/readme/trigger-semantics.svg',
    'docs/assets/generated/readme/observability.svg',
    'docs/assets/generated/readme/composite-loop.svg',
    'docs/assets/generated/readme/showcase.json',
]
for asset in required_assets:
    path = root / asset
    if not path.exists() or path.stat().st_size == 0:
        raise SystemExit(f'missing or empty required README asset: {asset}')

metadata_paths = sorted((root / 'examples').glob('[0-9][0-9]-*/example.json'))
if len(metadata_paths) < 8:
    raise SystemExit(f'expected at least 8 curated example metadata files, found {len(metadata_paths)}')
for metadata_path in metadata_paths:
    metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
    outdir = root / 'docs/assets/generated/examples' / metadata['id']
    for name in ('graph.mmd', 'graph.svg', 'summary.json'):
        path = outdir / name
        if not path.exists() or path.stat().st_size == 0:
            raise SystemExit(f'missing generated example asset: {path.relative_to(root)}')
    if metadata['assets'].get('generate_metrics_summary'):
        for name in ('metrics_summary.json', 'metrics_summary.svg'):
            path = outdir / name
            if not path.exists() or path.stat().st_size == 0:
                raise SystemExit(f'missing generated metrics asset: {path.relative_to(root)}')
    if metadata['assets'].get('generate_trace_summary'):
        for name in ('trace_summary.json', 'trace_summary.svg'):
            path = outdir / name
            if not path.exists() or path.stat().st_size == 0:
                raise SystemExit(f'missing generated trace asset: {path.relative_to(root)}')

readme = (root / 'README.md').read_text(encoding='utf-8')
required_phrases = [
    'beta / pre-production',
    'production-proven',
    'examples/00-getting-started/minimal.yaml',
    'docs/assets/generated/readme/hero.svg',
    'scripts/render_example_assets.py --topoexec build/topoexec --check',
]
for phrase in required_phrases:
    if phrase not in readme:
        raise SystemExit(f'README missing required anti-rot/status phrase: {phrase}')

markdown_files = [root / 'README.md', root / 'examples/README.md'] + sorted((root / 'examples').glob('[0-9][0-9]-*/README.md'))
link_re = re.compile(r'!?(?:\[[^\]]*\])\(([^)]+)\)')
missing = []
for md in markdown_files:
    text = md.read_text(encoding='utf-8')
    for match in link_re.finditer(text):
        raw = match.group(1).strip()
        if not raw or raw.startswith(('#', 'http://', 'https://', 'mailto:')):
            continue
        target = raw.split()[0].strip('<>')
        target = target.split('#', 1)[0]
        if not target:
            continue
        candidate = (md.parent / target).resolve()
        try:
            candidate.relative_to(root)
        except ValueError:
            missing.append(f'{md.relative_to(root)} -> outside repo: {raw}')
            continue
        if not candidate.exists():
            missing.append(f'{md.relative_to(root)} -> missing: {raw}')
if missing:
    raise SystemExit('markdown link/asset check failed:\n' + '\n'.join(missing))
print(json.dumps({'ok': True, 'markdown_files': len(markdown_files), 'metadata_files': len(metadata_paths)}))
PY

"$TOPOEXEC" graph validate examples/00-getting-started/minimal.yaml >/dev/null
"$TOPOEXEC" graph render examples/00-getting-started/minimal.yaml --format mermaid >/dev/null
"$TOPOEXEC" graph run examples/00-getting-started/minimal.yaml --steps 1 >/dev/null

echo '[showcase] README assets and quick-start smoke passed'
