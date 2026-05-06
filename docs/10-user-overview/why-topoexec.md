# Why Not ...?

TopoExec is not a replacement for every runtime. It is a small C++ semantic
runtime for bounded in-process graphs where graph meaning, observability, and
embedder control matter more than broad ecosystem features.

## oneTBB

Use oneTBB when the main problem is parallel algorithms, work stealing, and CPU
throughput. Use TopoExec when the graph contract itself matters: edge visibility,
feedback boundaries, payload policy, and runtime diagnostics are first-class.
TopoExec may use worker lanes internally, but it is not a general parallel
algorithm library.

## Dora

Use Dora when you want a dataflow system with its own runtime and inter-process
story. Use TopoExec when you need a small embeddable C++ runtime that stays
inside an application process and keeps adapter boundaries explicit.

## GStreamer

Use GStreamer for mature multimedia pipelines, codecs, and plugin ecosystems.
Use TopoExec for application semantic graphs where triggers, feedback, state,
config, and typed component descriptors are the primary model. TopoExec does not
claim media framework coverage.

## ROS 2

Use ROS 2 for distributed robotics middleware, DDS integration, node lifecycle,
and ecosystem tooling. Use TopoExec as an in-process semantic runtime that may be
adapted to ROS 2 later without making the core runtime depend on ROS.

## Workflow engines

Use workflow engines for durable long-running business processes, retries, and
external orchestration. Use TopoExec for bounded runtime ticks, explicit channel
semantics, and low-overhead C++ embedding. TopoExec does not persist workflows or
claim distributed job orchestration.
