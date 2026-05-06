# Release Runbook

This runbook turns the release checklist into an executable, repeatable
preparation flow. It prepares a candidate; it does **not** publish, tag, retag,
or upload a public release.

## Roles and authority

- Agents may run dry-run release preparation, generate local artifacts, and draft
  release notes.
- A human must approve the exact candidate commit before creating or pushing a
  tag.
- Published tags are immutable. If a release is wrong, fix forward with a new
  prerelease tag.

## Dry-run smoke

Check that the release-prep tool is installed and argument parsing works:

```bash
./scripts/release_prepare.sh --help
```

<!-- topoexec-doc-test: ${SOURCE_DIR}/scripts/release_prepare.sh --help -->

CTest also covers a non-publishing dry run:

```bash
./scripts/goal_check.sh release
```

## Candidate preparation

From a clean working tree on the exact candidate commit:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

The script checks the clean tree, rejects an existing local tag of the same name,
validates that release docs mention the requested version, runs the required
local gates including `clang-format` and `clang-tidy`, writes a release notes
draft, creates source/CPack/schema artifacts, and writes `SHA256SUMS`.

For a local rehearsal that skips long gates but still builds artifacts:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0 --skip-gates
```

For a script-only smoke in a dirty agent worktree:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0 --dry-run --allow-dirty
```

`--dry-run` writes release notes and a tag-command draft but intentionally skips
validation gates and artifact creation.

## Artifact outputs

Default output directory:

```text
dist/release-candidate/<version>/
```

Expected files after a full prep:

- `release-notes-<version>.md`: notes extracted from `CHANGELOG.md` Unreleased
  plus human approval checklist.
- `topoexec-<version>-source.tar.gz`: source archive from `git archive HEAD`.
- CPack binary/source TGZ files copied from the release build.
- `topoexec.schema.v1.<version>.json`: schema artifact.
- `SHA256SUMS`: checksum file for generated artifacts.
- `tag-command-<version>.txt`: human-approved follow-up command only.

## CI dry run

`.github/workflows/release-dry-run.yml` is manual (`workflow_dispatch`). It
prepares artifacts and uploads them as workflow artifacts. By default it uses
`--skip-gates` because normal CI already covers matrix validation; set
`run_gates=true` when rehearsing the full local gate sequence in the workflow.

The workflow still does not create or push tags.

## Documentation artifacts

When a release advertises generated docs or a Pages update, build those
artifacts from the exact candidate commit:

```bash
cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON
cmake --build build-docs --target topoexec_doxygen
./scripts/docs_build_site.sh
```

The Doxygen HTML lives under `build-docs/docs/doxygen/html/`; the assembled
Pages payload lives under `site/` with API reference files copied to `site/api/`.
Do not add a public Pages URL to release notes or README until the Pages
workflow has deployed successfully with GitHub Actions as the repository Pages
source.

## Human tag step

After local evidence and CI are attached to the release issue/PR and approved:

```bash
git tag -a v0.2.0-alpha.0 <candidate-commit> -m "v0.2.0-alpha.0"
git push origin v0.2.0-alpha.0
```

Never delete/recreate a public tag. If the wrong commit was tagged, make a new
fix-forward prerelease.
