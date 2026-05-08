#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/release_prepare.sh --version <vX.Y.Z[-suffix]> [options]

Prepare a reproducible TopoExec release candidate without publishing it.

Options:
  --version <tag>          Required annotated-tag name, for example v0.2.0-alpha.0.
  --artifacts-dir <dir>    Output directory for artifacts (default: dist/release-candidate/<version>).
  --build-dir <dir>        Build directory for package artifacts (default: build-release-candidate).
  --notes-out <file>       Release notes path (default: <artifacts-dir>/release-notes-<version>.md).
  --dry-run                Print actions and write notes, but do not run gates or create artifacts.
  --skip-gates             Do not run validation gates. Intended only for script smoke/dry-run rehearsals.
  --skip-artifacts         Do not create tarballs/schema/checksum artifacts.
  --allow-dirty           Allow a dirty working tree. Intended only for local script smoke.
  --allow-existing-tag     Allow an existing local tag only for dry-run smoke rehearsals.
  -h, --help               Show this help.

This script never creates or pushes tags. It verifies tag policy and prints the
annotated-tag command for a human-approved follow-up step.
USAGE
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION=""
ARTIFACTS_DIR=""
BUILD_DIR="build-release-candidate"
NOTES_OUT=""
DRY_RUN=0
SKIP_GATES=0
SKIP_ARTIFACTS=0
ALLOW_DIRTY=0
ALLOW_EXISTING_TAG=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      VERSION="${2:-}"
      shift 2
      ;;
    --artifacts-dir)
      ARTIFACTS_DIR="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --notes-out)
      NOTES_OUT="${2:-}"
      shift 2
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --skip-gates)
      SKIP_GATES=1
      shift
      ;;
    --skip-artifacts)
      SKIP_ARTIFACTS=1
      shift
      ;;
    --allow-dirty)
      ALLOW_DIRTY=1
      shift
      ;;
    --allow-existing-tag)
      ALLOW_EXISTING_TAG=1
      shift
      ;;
    -h|--help|help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "$VERSION" ]]; then
  echo "--version is required" >&2
  usage >&2
  exit 2
fi
if [[ ! "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]]; then
  echo "release version must be an annotated tag name like v0.2.0-alpha.0: $VERSION" >&2
  exit 2
fi

cd "$ROOT_DIR"
ARTIFACTS_DIR="${ARTIFACTS_DIR:-dist/release-candidate/${VERSION}}"
NOTES_OUT="${NOTES_OUT:-${ARTIFACTS_DIR}/release-notes-${VERSION}.md}"
COMMIT="$(git rev-parse --verify HEAD)"
SHORT_COMMIT="$(git rev-parse --short HEAD)"

if [[ "$ALLOW_DIRTY" -ne 1 && -n "$(git status --short)" ]]; then
  echo "working tree is dirty; commit or stash changes before release prep" >&2
  git status --short >&2
  exit 1
fi

if [[ "$ALLOW_EXISTING_TAG" -eq 1 && "$DRY_RUN" -ne 1 ]]; then
  echo "--allow-existing-tag is only valid with --dry-run" >&2
  exit 2
fi

if git rev-parse -q --verify "refs/tags/${VERSION}" >/dev/null && [[ "$ALLOW_EXISTING_TAG" -ne 1 ]]; then
  echo "tag already exists locally: ${VERSION}; fix forward with a new tag" >&2
  exit 1
fi
if git rev-parse -q --verify "refs/tags/${VERSION}" >/dev/null && [[ "$ALLOW_EXISTING_TAG" -eq 1 ]]; then
  echo "dry-run: allowing existing local tag ${VERSION} for smoke rehearsal"
fi

if ! grep -q '^## Unreleased' CHANGELOG.md; then
  echo "CHANGELOG.md must contain a ## Unreleased section" >&2
  exit 1
fi
for release_doc in docs/43-ci-build-release-tools/release-checklist.md docs/43-ci-build-release-tools/versioning.md docs/43-ci-build-release-tools/release-progression.md; do
  if ! grep -Fq "$VERSION" "$release_doc"; then
    echo "${release_doc} must mention intended version ${VERSION}" >&2
    exit 1
  fi
done

mkdir -p "$(dirname "$NOTES_OUT")"
python3 - "$VERSION" "$COMMIT" "$SHORT_COMMIT" "$NOTES_OUT" <<'PY'
from __future__ import annotations

import sys
from pathlib import Path

version, commit, short_commit, notes_out = sys.argv[1:5]
changelog = Path("CHANGELOG.md").read_text(encoding="utf-8").splitlines()
section: list[str] = []
in_unreleased = False
for line in changelog:
    if line.startswith("## "):
        if in_unreleased:
            break
        in_unreleased = line == "## Unreleased"
        continue
    if in_unreleased:
        section.append(line)
while section and not section[0].strip():
    section.pop(0)
while section and not section[-1].strip():
    section.pop()
if not section:
    print("CHANGELOG.md Unreleased section is empty", file=sys.stderr)
    raise SystemExit(1)
notes = [
    f"# TopoExec {version} release candidate notes",
    "",
    f"Candidate commit: `{commit}` (`{short_commit}`)",
    "",
    "## Draft notes from CHANGELOG.md Unreleased",
    "",
    *section,
    "",
    "## Human approval checklist",
    "",
    "- [ ] CI is green for the exact candidate commit.",
    "- [ ] Required local release gates are attached to the release issue/PR.",
    "- [ ] Artifacts and SHA256SUMS were generated from the exact commit.",
    "- [ ] Known limitations were copied from docs/43-ci-build-release-tools/release-checklist.md.",
    "- [ ] Annotated tag is created only after approval; no retagging.",
    "",
]
Path(notes_out).write_text("\n".join(notes), encoding="utf-8")
PY

echo "release notes draft: ${NOTES_OUT}"

if [[ "$DRY_RUN" -eq 1 ]]; then
  echo "dry-run: validation gates and artifact creation are disabled"
  SKIP_GATES=1
  SKIP_ARTIFACTS=1
fi

if [[ "$SKIP_GATES" -ne 1 ]]; then
  git diff --check
  ./scripts/agent_check.sh
  ./scripts/goal_check.sh format
  ./scripts/goal_check.sh tidy
  ./scripts/goal_check.sh package
  ./scripts/goal_check.sh golden
  ./scripts/goal_check.sh docs
  ./scripts/goal_check.sh stress
  ./scripts/goal_check.sh bench
  TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
else
  echo "skip-gates: release validation gates were not run"
fi

if [[ "$SKIP_ARTIFACTS" -ne 1 ]]; then
  mkdir -p "$ARTIFACTS_DIR"
  SOURCE_TARBALL="${ARTIFACTS_DIR}/topoexec-${VERSION}-source.tar.gz"
  git archive --format=tar.gz --prefix="topoexec-${VERSION}/" -o "$SOURCE_TARBALL" HEAD

  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build "$BUILD_DIR" -j
  cpack -G TGZ --config "$BUILD_DIR/CPackConfig.cmake" -B "$ARTIFACTS_DIR"
  cpack -G TGZ --config "$BUILD_DIR/CPackSourceConfig.cmake" -B "$ARTIFACTS_DIR"

  cp schema/topoexec.schema.v1.json "${ARTIFACTS_DIR}/topoexec.schema.v1.${VERSION}.json"
  (
    cd "$ARTIFACTS_DIR"
    rm -f SHA256SUMS
    find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%f\0' |
      sort -z |
      xargs -0 sha256sum > SHA256SUMS
  )
  echo "artifacts: ${ARTIFACTS_DIR}"
else
  echo "skip-artifacts: release artifacts were not generated"
fi

mkdir -p "$ARTIFACTS_DIR"
cat > "${ARTIFACTS_DIR}/tag-command-${VERSION}.txt" <<TAG
# Human-approved follow-up only; do not retag existing releases.
git tag -a ${VERSION} ${COMMIT} -m "${VERSION}"
git push origin ${VERSION}
TAG

echo "tag command draft: ${ARTIFACTS_DIR}/tag-command-${VERSION}.txt"
echo "release prep complete for ${VERSION} at ${COMMIT}"
