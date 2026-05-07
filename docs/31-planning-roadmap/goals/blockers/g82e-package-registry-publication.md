# G82e Blocker: Package Registry Publication

Status: blocked/deferred.

## Decision needed

Resolve human release owner, exact tag/artifacts/checksums, registry credentials, package namespace decisions, clean-machine evidence, and rollback/fix-forward policy.

## Options

1. Keep deferred.
2. Open a design/RFC-only goal.
3. Open an implementation goal with explicit scope, allowed files, acceptance, validation, and rollback/blocker handling.

## Recommendation

Do not publish vcpkg/Conan/PyPI/system packages yet. Continue local package matrix and release-prep evidence.

## Required evidence before opening

- Adoption signal or human owner directive.
- Scope and non-goals.
- Dependency and packaging impact.
- API/runtime/test/doc impact.
- Focused validation commands.
- Release/changelog/status/backlog update plan.

## Safe independent work

Keep G75-G80 release/adoption/package/reliability gates green and collect feedback through G80 triage.
