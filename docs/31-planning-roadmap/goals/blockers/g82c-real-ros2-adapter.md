# G82c Blocker: Real ROS 2 Adapter

Status: blocked/deferred.

## Decision needed

Resolve ROS distro/client-library dependency decision, executor/lifecycle mapping, QoS policy, packaging boundary, and fake-boundary plus integration validation plan.

## Options

1. Keep deferred.
2. Open a design/RFC-only goal.
3. Open an implementation goal with explicit scope, allowed files, acceptance, validation, and rollback/blocker handling.

## Recommendation

Do not add rclcpp/ament/rosidl dependencies yet. Continue dependency-free ROS 2 boundary preview docs/tests.

## Required evidence before opening

- Adoption signal or human owner directive.
- Scope and non-goals.
- Dependency and packaging impact.
- API/runtime/test/doc impact.
- Focused validation commands.
- Release/changelog/status/backlog update plan.

## Safe independent work

Keep G75-G80 release/adoption/package/reliability gates green and collect feedback through G80 triage.
