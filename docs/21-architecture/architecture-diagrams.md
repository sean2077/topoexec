# Architecture Diagrams

These diagrams are documentation aids for the current in-process runtime. They do
not imply external adapters or distributed execution.

## Runtime flow

```mermaid
flowchart TD
  Graph[GraphSpec / YAML] --> Validate[Schema + semantic validation]
  Validate --> Plan[Compiled region plan]
  Plan --> Runner[RuntimeRunner]
  Runner --> Epoch[Bounded event-loop epoch]
  Epoch --> Metrics[Metrics]
  Epoch --> Trace[Trace]
  Epoch --> Health[Bounded health events]
```

## Publication routing

```mermaid
sequenceDiagram
  participant C as Component
  participant G as GraphContext
  participant R as Runtime publisher
  participant B as Channel bus
  participant D as Downstream trigger
  C->>G: publish(port, payload)
  G->>R: stage source endpoint
  R->>B: commit after component returns
  B->>D: visible by edge kind and epoch
```

## Scheduler lanes

```mermaid
flowchart LR
  Ready[Ready invocations] --> Priority[Runtime priority ordering]
  Priority --> EventLoop[event_loop lane]
  Priority --> FixedRate[fixed_rate lane]
  Priority --> ThreadPool[thread_pool lane]
  ThreadPool --> Workers[bounded workers]
  Workers --> Results[committed results]
```

## Channel lifecycle

```mermaid
stateDiagram-v2
  [*] --> Created
  Created --> Published: publication committed
  Published --> Delivered: consumer reads
  Published --> Dropped: overflow / stale / deadline
  Delivered --> Observed: metrics + trace
  Dropped --> Observed: metrics + health event
  Observed --> [*]
```
