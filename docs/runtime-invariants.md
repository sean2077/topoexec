# Runtime Invariant Coverage

This document maps the invariants from `docs/plans/plan.md` G3 to concrete CI tests. Add a row here whenever a runtime semantic invariant is added, moved, or intentionally deferred.

| # | Invariant | Coverage |
| --- | --- | --- |
| 1 | `GraphContext::publish()` never directly calls downstream components. | `tests/test_runtime.cpp`: `Runtime.PublishStagesWithoutRecursiveDownstreamExecute`. |
| 2 | `immediate` edges in a DAG are visible in the same epoch. | `Runtime.ImmediateFeedForwardIsVisibleInSameEpochAndMetricsMatch`; `Runtime.RunModeExecutesEventRuntimeAndRoutesChannels`. |
| 3 | `delay` edges are visible only at the next epoch. | `Runtime.DelayEdgeCommitsAtNextEpochBoundary`; `Graph.DelayEdgeBreaksImmediateCycle`. |
| 4 | `state` readers see the old snapshot in the current epoch and the new snapshot next epoch. | `Runtime.StateAndAsyncEdgesCommitAfterCurrentEpoch`; `Graph.StateEdgesRejectMultipleWritersToSameTarget`. |
| 5 | `async` edges do not participate in immediate SCCs and are visible as deferred events. | `Runtime.StateAndAsyncEdgesCommitAfterCurrentEpoch`; `Runtime.AsyncTaskReadyTriggersDownstreamOnLaterEpoch`; `Graph.NonImmediateFeedbackEdgesDoNotCreateImmediateSccs`. |
| 6 | Undeclared immediate SCCs reject. | `Graph.ImmediateCycleRequiresCompositeLoop`; `Graph.FixedSeedImmediateCyclesRejectAndAcceptExactCompositeLoop`. |
| 7 | CompositeLoop declarations must exact-match the immediate SCC. | `Graph.PartialCompositeLoopDeclarationIsRejected`; `Graph.OverlappingImmediateCyclesCollapseIntoOneCompositeLoopRegion`; `Graph.ComponentCannotHaveMultipleCompositeLoopOwners`. |
| 8 | CompositeLoop external outputs commit after loop completion. | `Runtime.CompositeLoopRegionOwnsInternalFixedPointIterations`; `Runtime.CompositeLoopConvergenceStopsBeforeMaxIterations`; `Runtime.CompositeLoopBudgetOverrunStopsLoopAndReportsMetric`. |
| 9 | Latest overwrite does not create an unbounded backlog. | `tests/test_channel.cpp`: `Channel.LatestChannelDeliversOnlyNewestPayload`; app smoke `app_overload_latest_vs_queue_runs`. |
| 10 | Queue capacity handles full channels by overflow policy. | `Channel.QueueDropsOldestWhenFull`; `Channel.QueueDropNewestRejectsIncomingPayloadWhenFull`; `Channel.QueueBlockReturnsWouldBlockWithoutDroppingExistingPayload`; `Channel.QueueFailFastReturnsCapacityError`. |
| 11 | `move_only + multi-reader` is invalid. | `Graph.MoveOnlyPolicyRequiresSingleReader`. |
| 12 | Multiple state writers are invalid unless a future merge policy exists. | `Graph.StateEdgesRejectMultipleWritersToSameTarget`. |
| 13 | `batch`, `time_sync`, and `all_inputs` trigger consumption order is deterministic. | `Runtime.AllInputsWaitsForEveryRequiredPort`; `Runtime.TimeSyncWaitsForInputsAndUsesTimeSyncTriggerKind`; `Runtime.TimeSyncDropsOldestOutOfSlopSampleUntilInputsAlign`; `Runtime.BatchTriggerPreservesPartialBatchUntilThreshold`; `Runtime.BatchTriggerFlushesPartialBatchAfterWindowExpires`. |
| 14 | Stop token is observed between scheduler iterations. | `Runtime.StopTokenStopsBeforeExecutingComponentsAndCleansUp`. |
| 15 | Worker lanes do not break compiled region boundaries. | `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently`; `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations`; graph region-order tests in `tests/test_graph.cpp`. |
| 16 | Non-reentrant components do not overlap. | `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations`. |
| 17 | Reentrant components can overlap but do not exceed lane `max_threads`. | `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently`. |
| 18 | Metrics match actual behavior. | `Runtime.ImmediateFeedForwardIsVisibleInSameEpochAndMetricsMatch`; async max-inflight tests; `cli_golden_outputs` metrics golden. |
| 19 | Trace events are emitted with legal durations and stable semantic fields. | `Runtime.RunModeExecutesEventRuntimeAndRoutesChannels`; `cli_golden_outputs` trace golden and Chrome trace smoke. |
| 20 | Schema unknown fields reject. | `Graph.RejectsUnknownRootFields`; `cli_reject_invalid_unknown_field`; `schema_v1_contract_smoke`. |

## Maintenance rule

If a runtime change modifies any invariant above, update the named tests or add a new focused test in the same commit. `./scripts/agent_check.sh` is the gate that keeps this mapping executable.
