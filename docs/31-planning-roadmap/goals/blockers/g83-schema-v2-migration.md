# G83 Blocker: Schema v2 / Migration Program

Status: blocked/deferred.

## Decision needed

Resolve adoption-driven breaking need, reviewed schema v2 RFC, v1 compatibility policy, migration CLI design, idempotency/roundtrip tests, and release notes plan.

## Options

1. Keep deferred.
2. Open a design/RFC-only goal.
3. Open an implementation goal with explicit scope, allowed files, acceptance, validation, and rollback/blocker handling.

## Recommendation

Do not implement a schema v2 loader or migration CLI yet. Keep schema v1 strict and maintain schema-v2 notes as design boundary.

## Required evidence before opening

- Adoption signal or human owner directive.
- Scope and non-goals.
- Dependency and packaging impact.
- API/runtime/test/doc impact.
- Focused validation commands.
- Release/changelog/status/backlog update plan.

## Safe independent work

Keep G75-G80 release/adoption/package/reliability gates green and collect feedback through G80 triage.
