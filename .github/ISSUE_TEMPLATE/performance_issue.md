---
name: Performance issue
about: Report a benchmark, latency, throughput, or allocation concern
labels: performance
---

## Workload

## Measurement command

```bash
./build/topoexec graph bench examples/minimal.yaml --steps 1 --runs 10 --format json
```

## Result

## Expected comparison or regression baseline

## Debug pack

- [ ] Benchmark JSON output
- [ ] Machine context: OS, compiler, CMake, CPU model, build type
- [ ] Exact command line and graph
- [ ] Whether `./scripts/goal_check.sh bench` or `live-perf` reproduces
