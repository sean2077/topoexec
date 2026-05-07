# G81 Blocker: Ecosystem Track Selection

Status: blocked on adoption signal and human owner decision.

## Decision needed

Choose whether to open exactly one G82 integration track after G75-G80, or keep
all ecosystem tracks deferred.

## Options

1. G82a production observability exporter.
2. G82b native Python binding.
3. G82c real ROS 2 adapter.
4. G82d editor/LSP.
5. G82e package registry publication.
6. Keep all G82 tracks deferred until external adoption feedback exists.

## Recommendation

Keep all G82 tracks deferred now. If a human release owner wants one near-term
track after `v0.2.0-alpha.0`, choose G82e package registry publication first
because it improves adoption without new runtime dependencies or semantic scope.

## Impact

- API/runtime: no code change in G81.
- Tests: rely on `policy`, `package`, `compat`, `adoption`, and release gates.
- Docs: keep README/release/backlog deferrals visible.
- Release: no tag, publication, credentials, adapter implementation, binding,
  editor, or schema v2 work is authorized by this blocker.

## Safe independent work

Continue release/adoption/package/reliability/documentation evidence and collect
real issue feedback. Do not implement G82a-e until the decision is resolved.
