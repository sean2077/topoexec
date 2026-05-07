# G84 Core Runtime Beta Candidate Readiness

Status: readiness review only. This page does not tag, publish, upload, or claim
production readiness. A human release owner must accept the exact candidate
commit and remaining deferrals before any beta tag.

## Verdict

TopoExec can prepare a **core-runtime beta candidate review** after the G75-G80
local evidence, but the public line should remain `v0.2.0-alpha.0` unless a
human release owner explicitly opens a beta tag. This is not adapter/ecosystem
beta readiness and not a v1.0 readiness claim. Scope label: not adapter/ecosystem beta readiness.

## G75-G80 evidence summary

| Area | Evidence |
| --- | --- |
| Release/adoption | Apache-2.0 and `0.2.0` metadata alignment; adoption readiness docs; prerelease notes draft; release-prep dry-run. |
| Dogfood | Synthetic YAML dogfood pilot with metrics, trace, live assertions, replay, and benchmark smoke. |
| Compatibility | `compat` gate covers installed headers, public API inventory, schema v1, CLI JSON, metrics, trace, live observe, and package smoke. |
| Packaging | Package matrix covers runtime-only/default/YAML/CLI/CPack/release/vcpkg/Conan local forms; registry publication blocked. |
| Reliability | Bounded reliability tiers, soak-lite, stress, fuzz, bench, ASAN+UBSAN policy, non-blocking TSAN policy. |
| Adoption loop | Debug pack, triage labels, first-user path, and adoption feedback checks exist. |
| Ecosystem conditionals | G81/G82x/G83 blockers keep adapters/bindings/editor/package publication/schema v2 deferred. |

## Required beta-candidate gate

Run on the exact candidate commit before any beta tag:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
./scripts/goal_check.sh compat
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/goal_check.sh dogfood
./scripts/goal_check.sh reliability
./scripts/goal_check.sh adoption
./scripts/goal_check.sh conditional
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version <beta-tag>
```

The beta tag is blocked if the human release owner does not accept skipped or
failed gates, TSAN policy, longer soak/fuzz scope, and all listed deferrals.

## Beta release-notes draft boundaries

A beta release note must include:

- stable-v0.2 core runtime/API/schema/metrics/trace/live observe boundaries;
- package/install/downstream evidence;
- dogfood/reliability evidence;
- explicit adapter/ecosystem/package-publication/schema-v2/v1.0 deferrals;
- statement that TopoExec is not production-proven or deployed at scale;
- human release-owner acceptance for the exact candidate commit.

## Explicitly not beta-ready

- production OpenTelemetry/Prometheus exporter;
- real ROS 2 client-library adapter;
- native Python bindings;
- stable C ABI beyond ABI version 0;
- sandboxed/stable plugin ecosystem;
- full editor extension or LSP;
- package registry publication;
- schema v2 loader or migration CLI;
- hard real-time scheduling, CPU affinity guarantee, or hard preemption;
- v1.0 stable-compatibility claim.

## Owner decision

If accepted, describe the tag as **core-runtime beta** only. If not accepted,
keep the release ladder on the alpha line and continue collecting adoption and
package evidence.
