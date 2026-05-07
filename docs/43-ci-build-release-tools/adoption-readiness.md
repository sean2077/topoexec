# Release and Adoption Readiness

Status: local prerelease-candidate readiness checklist for the next human-owned
release step. This page prepares `v0.2.0-alpha.0`; it does not publish, tag, or
claim production deployment.

## Target path for a first external user

The intended first-user path is deliberately short and reproducible:

```text
README -> clone/download -> configure/build -> install -> find_package -> run example -> read docs -> report issue
```

The local smoke sequence for that path is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix /tmp/topoexec-install
cmake -S tests/cmake/runtime_smoke -B /tmp/topoexec-runtime-smoke \
  -DCMAKE_PREFIX_PATH=/tmp/topoexec-install
cmake --build /tmp/topoexec-runtime-smoke -j
/tmp/topoexec-runtime-smoke/topoexec_runtime_smoke
./build/topoexec graph validate examples/00-getting-started/minimal.yaml
./build/topoexec graph run examples/00-getting-started/minimal.yaml --steps 1
```

## Metadata alignment checklist

| Surface | Expected value for the next prerelease line |
| --- | --- |
| Source license | Apache-2.0 in `LICENSE`, README, vcpkg draft, and Conan draft. |
| CMake package version | `0.2.0`, matching the `v0.2.0-alpha.0` candidate line. |
| Runtime semantic contract | `0.2`; this is separate from schema version. |
| Graph schema | schema v1 only; schema v2 implementation is deferred. |
| Project status | beta / pre-production, not production-proven or deployed at scale. |

## Candidate artifacts

Use the release runbook to prepare local artifacts from a clean exact candidate
commit:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

The script writes draft release notes, source and CPack archives, schema copies,
checksums, and a human-only tag command. It refuses to publish, push, or retag.
A dirty worktree may use `--dry-run --allow-dirty` only to rehearse the script.

## Documentation site readiness

Local docs/site validation is separate from public URL ownership:

```bash
./scripts/docs_build_site.sh
```

Do not add a public GitHub Pages URL to README or release notes until the Pages
workflow deploys successfully and the repository Pages source is set to GitHub
Actions by a human owner.

## Known limitations for adoption messaging

Keep these limitations visible in release notes and onboarding docs:

- no production ROS 2 client-library package;
- no production OpenTelemetry/Prometheus exporter or remote dashboard;
- no native Python bindings, stable C ABI, or sandboxed plugin ecosystem;
- no schema v2 loader or migration CLI;
- no package-registry publication;
- no hard real-time scheduling, CPU affinity guarantee, or hard preemption;
- package-manager recipes under `packaging/` are drafts.
