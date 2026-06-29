# Competitive Landscape and Positioning

Last updated: 2026-06-29.

Status: research-backed positioning analysis. This page is a decision input for
the [Ecosystem decision gate](ecosystem-decision-gate.md) and the
[Goal backlog](goals/backlog.md). It records where TopoExec sits relative to
comparable projects, whether its differentiating niche is real, and what the
landscape implies for the roadmap. It does not open any conditional track or
change runtime/API scope.

## Bottom line

Every individual TopoExec feature has prior art: deterministic execution,
epoch-delayed feedback breaking, bounded feedback loops, trigger/readiness
policies, and an in-process C++ graph all exist elsewhere, some with decades of
formal theory behind them. The defensible niche is not any single feature; it is
the *combination and packaging*: a small, dependency-light, embeddable C++
library that treats the dataflow graph as a **testable semantic contract**
(deterministic, golden-output, CLI-inspectable) with **bounded feedback
(CompositeLoop) as a first-class construct** — without forcing adoption of a
middleware, runtime, or new language. No surveyed project occupies that exact
cell. The niche is real but narrow, the value proposition is abstract until a
user hits the pain, and each axis taken alone has a stronger competitor.

## The positioning map

The landscape separates cleanly along two axes: whether the tool gives the graph
a deterministic / feedback *semantic contract*, and whether it is a small
embeddable C++ library or requires its own runtime, language, or middleware.

| | Has determinism / feedback contract | Throughput-oriented, no contract |
| --- | --- | --- |
| **Embeddable C++ library** | **TopoExec (open cell)** | RaftLib, Taskflow, oneTBB Flow Graph, Pipeflow |
| **Needs own runtime / language / middleware** | Lingua Franca (Reactors), Ptolemy II, LUSTRE/SCADE | Flink, Akka Streams, DORA, ROS 2 |

No surveyed competitor shares TopoExec's cell. People who want a determinism and
feedback contract today must adopt a heavier language/runtime/codegen toolchain;
people who want a small embeddable C++ library today get throughput-oriented,
non-deterministic, DAG-only graphs. TopoExec fills the gap between them.

## Competitive landscape by layer

Comparisons below were assembled from a multi-source research pass with
adversarial verification of the load-bearing facts (see [Provenance](#provenance)).
Language, in-process vs distributed, determinism, and feedback handling were
verified against primary sources. License and adoption notes are general-context
and were not part of the adversarially-verified claim set.

### 1. In-process C++ dataflow / task-graph libraries (closest by form)

These are the closest competitors by *shape* — same language, same in-process
embedding model.

| Project | In-process | Deterministic | Feedback handling | Key contrast with TopoExec |
| --- | --- | --- | --- | --- |
| RaftLib | Yes (C++17 library) | No (throughput) | DAG only; no documented cycle/feedback support | Closest by form, but feedforward-only, no determinism or feedback contract |
| Taskflow (cpp-taskflow) | Yes (header-only C++20) | No (work-stealing) | Imperative *conditional tasking* (a condition task returns an integer selecting the next branch) | Mature, fast, CPU-GPU heterogeneous; cycles are in-graph control flow for parallelism, not declarative fixed-point feedback; no edge-visibility / bounded-queue / determinism semantics |
| oneTBB Flow Graph | Yes (C++ library) | No (TBB task scheduler) | Fixed node taxonomy (functional / buffering / service); no fixed-point feedback construct | Industrial; the graph is a parallelism vehicle, not a semantic contract |
| Pipeflow | Yes (built on Taskflow) | No (work-stealing) | Pipeline parallelism | Throughput tool; benchmarks faster than oneTBB (24% / 10% on VLSI placement / timing-analysis workloads), but determinism is not its axis |

Unifying pattern (verified): these libraries treat the graph as a vehicle for
extracting parallelism. Their execution is work-stealing and non-deterministic,
their topology is DAG-only, and their documentation contains *no* notion of
determinism guarantees, edge-visibility semantics, bounded queues, or fixed-point
feedback convergence. This is TopoExec's clearest contrast against same-language
competitors.

### 2. Robotics / perception dataflow stacks

| Project | Form | Deterministic | Key contrast with TopoExec |
| --- | --- | --- | --- |
| ROS 2 (rclcpp executor) | Distributed middleware (DDS) | No (executor scheduling) | Large ecosystem; inter-process first; heavy dependency surface |
| DORA (dora-rs) | Distributed multi-daemon (Zenoh pub-sub) + shared-memory + in-process operators; Rust core, **polyglot** (Rust / Python / C / C++ nodes via native APIs) | No (no determinism claim; feedback not explicitly addressed) | Robotics-flavored same-category tool, but distributed-first with no determinism or feedback contract. Note: DORA is polyglot, not "Rust-only" — it explicitly supports C++ nodes |
| NVIDIA Holoscan / GXF | In-process entity-component graph with an explicit scheduler | Single-threaded Greedy scheduler is predictable (time-compressed under a ManualClock) | The highest concept overlap: GXF offers Greedy single-thread *and* Multithread (dispatcher + worker pool) schedulers — mirroring TopoExec's `event_loop` / `thread_pool` split — and its SchedulingTerms (MessageAvailable, Periodic, Count, Boolean, TargetTime, …) map almost one-to-one onto TopoExec trigger policies. The gap: GXF is tied to NVIDIA's GPU ecosystem and is not a lightweight, vendor-neutral embeddable library |
| eCAL / iceoryx | IPC / zero-copy transport layers | n/a | Transport substrate, not a graph runtime — complementary, not competing |

Key takeaway: GXF is the nearest architectural neighbor (in-process graph,
explicit scheduler choice, condition-based triggers), but it is a vendor GPU
stack, not a "link it into my app" library. TopoExec's edge here is lightweight,
vendor-neutral embedding.

### 3. Deterministic coordination models (strongest on the determinism axis)

| Project | Relationship to TopoExec |
| --- | --- |
| Lingua Franca / Reactor model (Lohstroh, Menard, Bateni, Lee, ACM TECS 2021) | The most serious "could subsume TopoExec" threat on the determinism axis. Reactors achieve deterministic concurrency while preserving the actor style, coordinated by a *logical model of time* (associable with physical time) — the same idea as TopoExec's tick/epoch execution. It already spans multicore *and* distributed configurations without losing determinism (TopoExec is in-process only). The one structural difference: LF is a coordination language with code generation (you write LF plus a target language), not a plain C++ library you embed — that is both the differentiator and the warning |
| Ptolemy II (SDF domain) | Academic canon: the SDF domain computes a static actor execution order before runtime — the direct ancestor of TopoExec's compile-time compression into ordered execution regions. But Ptolemy is a Java research platform, not an embeddable C++ runtime |

### 4. Synchronous dataflow (SDF) and formal semantics (vocabulary and lineage)

These are not products to compete with; they supply the formal vocabulary and
design discipline that justify TopoExec's "deterministic" claim.

- **Tagged Signal Model** (Lee & Sangiovanni-Vincentelli): a denotational
  framework where a process is a set of behaviors and composition is the
  intersection of behaviors; a system is *determinate* if, given the inputs, it
  has exactly one or exactly zero behaviors. It formally distinguishes *timed*
  (totally ordered tags) from *untimed* (partially ordered tags) systems — the
  theory TopoExec's edge-visibility / tick semantics informally instantiate.
- **LUSTRE / SCADE**: synchronous dataflow languages whose synchrony hypothesis
  (each reaction is instantaneous relative to external events) plus
  bounded-memory restriction yields deterministic programs — the same
  determinism-via-synchronous-semantics stance, but as a language rather than a
  runtime.
- **Synchronous programming micro/macrostep separation**: the established
  mechanism for generating deterministic single-threaded code from concurrent
  synchronous programs — the theory under TopoExec's compile-time execution
  regions.
- **Dataflow firing rules** (Ptolemy): firing rules give the preconditions for an
  actor firing, with technical conditions ensuring determinacy — the formal basis
  for trigger / readiness policies.

Implication: TopoExec should *claim this lineage* in its docs and positioning. It
is low-cost credibility and a precise differentiator against the throughput
libraries in layer 1. It should *not* be turned into a formal-verification
product line — that high ground is already held by Lingua Franca and Ptolemy.

### 5. Streaming / orchestration engines (boundary, not competitors)

Useful as contrast and as evidence the niche is real:

- **Apache Flink**: processing-time mode is explicitly non-deterministic (results
  depend on record arrival speed). Its FLIP-15 iteration redesign documents the
  exact feedback problems CompositeLoop targets, and Flink ML breaks feedback by
  epoch increment (a feedback record's epoch = triggering input's epoch + 1) —
  the same mechanism as TopoExec's `delay` edge visibility. But Flink is JVM,
  distributed, and heavyweight.
- **Akka Streams**: treats graph cycles as a hazard — naive cycles can deadlock as
  elements accumulate in the loop until internal buffers fill and permanently
  backpressure the source. This is the best argument *for* making bounded feedback
  a first-class declarative construct, as CompositeLoop does.
- **Airflow / Dagster / Temporal / Prefect / Argo**: batch/durable orchestration
  DAGs for long-running business processes, retries, and persistence. Orthogonal
  to TopoExec's bounded in-process ticks; listed only to draw the boundary.
- **Causify DataFlow**: the closest "unified batch/streaming + causality"
  framework is Python-native (pandas / NumPy / scikit-learn), which reinforces
  that the C++20 in-process niche is still open.

## Is the differentiating niche real?

Yes, but it is a *combination/packaging* niche, not a *feature* niche, and it is
narrow. The positioning map shows no competitor in TopoExec's cell: determinism
and feedback contracts ship today only with heavier languages/runtimes/codegen,
while embeddable C++ libraries ship only throughput-oriented, non-deterministic,
DAG-only graphs.

Best-fit scenarios, by strength of fit:

1. **Robotics / embedded perception-estimation-control loops** (estimator /
   controller feedback). CompositeLoop's fixed-point convergence plus iteration
   budget is built for this, and it is exactly what Akka/Flink treat as a hazard
   and RaftLib/Taskflow do not support. This is the sharpest wedge.
2. **Testable deterministic simulation and complex event processing**, where
   "graph behavior is a CI-checkable contract" matters most for regression-tested
   signal/perception pipelines.
3. **In-process, low-overhead, vendor-neutral embedding** as a lighter
   alternative to GXF (NVIDIA-bound) or ROS 2 (DDS ecosystem).

### Moat vs. fatal weakness

Moat (real but narrow):

- The "testable semantic contract" narrative is unclaimed — throughput libraries
  do not talk about determinism, robotics stacks do not talk about golden-output
  testability.
- The design discipline as a coherent whole (bounded everything, observation is
  not control, no hidden recursion) is a quality signal that is hard to replicate
  quickly.
- Lightweight, dependency-light, vendor-neutral embedding versus GXF / ROS 2 /
  Flink.

Fatal weakness (must be faced):

- Every feature taken alone is unremarkable; the appeal lives in coherence and
  quality, which is the hardest thing to market.
- The value proposition is abstract until a user hits the pain firsthand.
- Each axis has a stronger competitor: Lingua Franca on determinism (and it is
  already distributed), Taskflow / oneTBB on C++ task graphs (more mature, faster,
  already in OS distributions), ROS 2 / DORA on robotics (ecosystem dominance). A
  head-on fight on any single axis is a loss.
- Zero adoption today plus a small maintainer surface is a structural risk (see
  [Roadmap implications](#roadmap-implications)).

## Roadmap implications

These are landscape-derived recommendations. They do not open any conditional
track; opening one still requires its blocker resolution per the
[Conditional tracks ledger](conditional-tracks-ledger.md).

### Deferred-track priority

The research supports the existing [Ecosystem decision gate](ecosystem-decision-gate.md)
recommendation to prioritize **G82e package registry publication** first. Library
adoption studies indicate the primary inhibitor is perceived loss of control:
adopters demand evidence of active maintenance and long-term sustainability
before depending on a library, and a small/single maintainer is itself a named
adoption risk. The current bottleneck is trial friction and maintenance trust,
not features — lowering the `find_package`-to-running barrier beats adding
capability.

After packaging, prefer proving one vertical end-to-end over breadth. The
strongest candidate is a **lightweight ROS 2 bridge example or demo** (not the
full adapter deferred under [G82c](goals/blockers/g82c-real-ros2-adapter.md)):
borrow ROS 2's traffic to show a deterministic, testable, bounded feedback control
loop running *inside* a node. A production observability exporter (G82a) is a
reasonable second, since it makes the "observable" pillar concrete with a mostly
self-contained surface. Native Python bindings (G82b) risk diluting the
"embeddable C++" identity into a crowded Python field; LSP (G82d) is lowest
priority because JSON Schema plus diagnostics already cover most authoring needs.

### SDF / formal semantics

Worth mining for narrative and design discipline, not as a product line. Claiming
the SDF / Tagged Signal Model / synchronous-language lineage in the docs is
low-cost credibility and a precise differentiator. A full formal-verification
effort is a research project on ground already held by Lingua Franca and Ptolemy.

### Differentiation narrative

Stop competing as "another C++ task graph" (loses to Taskflow / oneTBB on
maturity and speed) or "another robotics dataflow" (loses to ROS 2 / DORA on
ecosystem). Compete on the one thing none of them package:

> Deterministic, golden-output-testable dataflow with first-class bounded
> feedback — embeddable in one C++ process, no middleware, no new language.

The test-determinism angle ("your graph's behavior is a CI-checkable contract")
is the sharpest wedge; no throughput library, robotics stack, or streaming engine
makes it first-class. The README's "Deterministic / Observable / Testable" is
already correct, but **Testable (as a semantic contract)** deserves to lead rather
than sit third.

### Failure modes

From the open-source failure literature (Coelho & Valente, FSE 2017; 104 failed
projects), the top causes are being usurped by a stronger competitor, technology
becoming obsolete, and the main contributor losing time or interest. Mapped to
TopoExec:

1. **Usurped on a single axis** (Lingua Franca / Taskflow / DORA). Mitigation:
   own the narrow middle cell; never fight head-on on one axis.
2. **Maintainer burnout / time** (a top-three OSS failure cause). Mitigation: keep
   scope tight — the project's existing boundary discipline is the correct defense
   — and lower contribution friction.
3. **Never crossing the adoption chasm** because the value is abstract.
   Mitigation: one undeniable reference use case plus an installable package.
4. **Scope creep** into all deferred tracks at once, diluting the core. The
   project's one-track-at-a-time gate already guards this; keep it.

Survival indicators from the same literature (CI, contribution guidelines) are
already in place, which is a positive signal.

## Suggested comparison additions

[Why TopoExec](../10-user-overview/why-topoexec.md) already contrasts oneTBB,
Dora, GStreamer, ROS 2, and workflow engines. This analysis adds three closer
neighbors worth naming there: Lingua Franca / Reactors (the determinism-axis
boundary), NVIDIA Holoscan / GXF (the highest concept overlap), and Taskflow (the
most mature same-language alternative).

## Provenance

This page was produced from a deep-research pass (five search angles, 22 sources,
103 extracted claims, 25 load-bearing claims verified by independent adversarial
voters, 22 confirmed). Three claims were corrected during verification and are
reflected above: DORA is polyglot rather than Rust-only; the Pipeflow speedup
figures are accurate but do not make it a determinism competitor; and the
Holoscan/GXF architectural comparison holds even though one originally-cited
supporting quote did not. License and adoption notes are general context, not part
of the verified claim set, and should be re-checked before external publication.

### Sources

In-process C++ libraries: [RaftLib](https://github.com/RaftLib/RaftLib) ·
[Taskflow](https://github.com/taskflow/taskflow) and
[Taskflow paper (arXiv:2004.10908)](https://arxiv.org/abs/2004.10908) ·
[oneTBB Flow Graph](https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/2021-6/data-flow-graph.html) ·
[Pipeflow (arXiv:2202.00717)](https://arxiv.org/pdf/2202.00717)

Robotics / perception: [DORA](https://github.com/dora-rs/dora) ·
[NVIDIA Holoscan / GXF](https://docs.nvidia.com/holoscan/sdk-user-guide/relevant_technologies.html)

Deterministic coordination models:
[Reactors / Lingua Franca (Lohstroh et al., 2021)](https://www.icyphy.org/publications/2021_LohstrohEtAl/) ·
[Ptolemy SDF](http://ptolemy.eecs.berkeley.edu/papers/99/HMAD/html/sdf.html)

Formal semantics:
[Tagged Signal Model (Lee & Sangiovanni-Vincentelli)](https://ptolemy.berkeley.edu/papers/96/denotational/denotationalERL.pdf) ·
[Dataflow firing rules](https://ptolemy.berkeley.edu/papers/97/dataflow/) ·
[LUSTRE (Halbwachs et al.)](https://homepage.cs.uiowa.edu/~tinelli/classes/181/Spring08/Papers/Halb91.pdf) ·
[Synchronous programming micro/macrostep](https://dl.acm.org/doi/10.1145/1023833.1023859)

Streaming / boundary:
[Flink FLIP-15 iterations](https://cwiki.apache.org/confluence/pages/viewpage.action?pageId=66853132) ·
[Flink ML iteration (epoch)](https://nightlies.apache.org/flink/flink-ml-docs-master/docs/development/iteration/) ·
[Flink time semantics](https://nightlies.apache.org/flink/flink-docs-stable/docs/concepts/time/) ·
[Akka Streams graphs and cycles](https://doc.akka.io/libraries/akka-core/current/stream/stream-graphs.html)

Adoption and failure:
[Why Modern Open Source Projects Fail (Coelho & Valente, FSE 2017)](https://arxiv.org/pdf/1707.02327)
