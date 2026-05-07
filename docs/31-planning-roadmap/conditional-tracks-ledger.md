# Conditional Tracks Ledger

Status: G82x/G83 conditional tracks are deferred. This ledger records entry
criteria and blocker files for every conditional track so future work starts from
an explicit decision instead of accidental scope expansion.

| Track | Status | Blocker file | Entry criteria |
| --- | --- | --- | --- |
| G82a production observability exporter | blocked/deferred | `goals/blockers/g82a-production-observability-exporter.md` | Real exporter user need, dependency/transport decision, hot-path budget, and validation plan. |
| G82b native Python binding | blocked/deferred | `goals/blockers/g82b-native-python-binding.md` | CLI-backed preview proven insufficient, binding ownership/performance target, API stability decision. |
| G82c real ROS 2 adapter | blocked/deferred | `goals/blockers/g82c-real-ros2-adapter.md` | ROS distro/dependency/executor/lifecycle decision and hardware-free plus integration validation plan. |
| G82d editor/LSP | blocked/deferred | `goals/blockers/g82d-editor-lsp.md` | JSON Schema/diagnostic workflow proven insufficient and maintenance/distribution owner identified. |
| G82e package registry publication | blocked/deferred | `goals/blockers/g82e-package-registry-publication.md` | Human release owner, exact tag/artifacts/checksums, registry credentials, clean-machine package evidence. |
| G83 schema v2 / migration | blocked/deferred | `goals/blockers/g83-schema-v2-migration.md` | Adoption-driven breaking need, reviewed v2 RFC, migration CLI design, v1 compatibility policy. |

## Global rule

Do not implement a conditional track from this ledger until its blocker is
resolved and the goal record states scope, allowed files, non-goals, acceptance,
validation, and blocker handling. The safe default is to continue G75-G80/G84
release/adoption/core-runtime readiness evidence.
