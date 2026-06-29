# Why Not ...?

TopoExec is not a replacement for every runtime. It is a small C++ semantic
runtime for bounded in-process graphs where graph meaning, observability, and
embedder control matter more than broad ecosystem features.

Its sharpest differentiator is treating the graph as a testable semantic
contract — deterministic, golden-output, and CLI-inspectable — with bounded
feedback as a first-class construct, without requiring a middleware, separate
runtime, or new language. For the full competitor survey and positioning
rationale, see the
[competitive landscape analysis](../31-planning-roadmap/competitive-landscape.md).

## oneTBB

Use oneTBB when the main problem is parallel algorithms, work stealing, and CPU
throughput. Use TopoExec when the graph contract itself matters: edge visibility,
feedback boundaries, payload policy, and runtime diagnostics are first-class.
TopoExec may use worker lanes internally, but it is not a general parallel
algorithm library.

## Taskflow

Use Taskflow when you want a mature, header-only C++ task-graph library for
high-throughput parallel and CPU-GPU heterogeneous work, where cycles are
imperative control-flow decisions inside the graph. Use TopoExec when execution
order must be deterministic, feedback must be a declarative bounded loop with
fixed-point convergence, and the graph is a contract you validate and golden-test
rather than a vehicle for extracting parallelism.

## Dora

Use Dora when you want a dataflow system with its own runtime and inter-process
story. Use TopoExec when you need a small embeddable C++ runtime that stays
inside an application process and keeps adapter boundaries explicit.

## NVIDIA Holoscan / GXF

Use Holoscan/GXF when you are building on NVIDIA hardware and want a GPU-centric
graph execution framework with its scheduler and ecosystem. Its entity-component
graph, single-thread vs multithread scheduler choice, and condition-based
SchedulingTerms are the closest concept overlap with TopoExec. Use TopoExec when
you want the same in-process graph, deterministic lane, and trigger semantics as a
lightweight, vendor-neutral library you link into any C++ application.

## GStreamer

Use GStreamer for mature multimedia pipelines, codecs, and plugin ecosystems.
Use TopoExec for application semantic graphs where triggers, feedback, state,
config, and typed component descriptors are the primary model. TopoExec does not
claim media framework coverage.

## ROS 2

Use ROS 2 for distributed robotics middleware, DDS integration, node lifecycle,
and ecosystem tooling. Use TopoExec as an in-process semantic runtime that may be
adapted to ROS 2 later without making the core runtime depend on ROS.

## Lingua Franca (Reactors)

Use Lingua Franca when you want deterministic concurrency coordinated by logical
time across multicore and distributed targets, and you accept a coordination
language plus code generation into a target language. Use TopoExec when you want
deterministic in-process execution as a plain C++ library you embed directly —
no separate language, code-generation step, or distributed runtime. Lingua Franca
is the strongest neighbor on the determinism axis; TopoExec trades its distributed
reach for direct, dependency-light C++ embedding.

## Workflow engines

Use workflow engines for durable long-running business processes, retries, and
external orchestration. Use TopoExec for bounded runtime ticks, explicit channel
semantics, and low-overhead C++ embedding. TopoExec does not persist workflows or
claim distributed job orchestration.
