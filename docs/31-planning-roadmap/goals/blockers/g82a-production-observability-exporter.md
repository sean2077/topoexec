# G82a Blocker: Production Observability Exporter

Status: blocked/deferred.

## Decision needed

Resolve production OpenTelemetry/Prometheus/exporter user need, dependency choice, transport boundary, and proof that G73 live observe hot paths remain free of JSON/file/socket/UI work.

## Options

1. Keep deferred.
2. Open a design/RFC-only goal.
3. Open an implementation goal with explicit scope, allowed files, acceptance, validation, and rollback/blocker handling.

## Recommendation

Do not implement production telemetry exporters yet. Continue with dependency-free preview mappings and collect exporter requests.

## Required evidence before opening

- Adoption signal or human owner directive.
- Scope and non-goals.
- Dependency and packaging impact.
- API/runtime/test/doc impact.
- Focused validation commands.
- Release/changelog/status/backlog update plan.

## Safe independent work

Keep G75-G80 release/adoption/package/reliability gates green and collect feedback through G80 triage.
