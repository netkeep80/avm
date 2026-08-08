# Typed inspection session contract

AVM 1.4 begins with a host-side inspection session that composes existing AVM 1.x APIs. It is tooling, not a new semantic layer.

## Boundary

The typed session has one path to canonical state and execution:

```text
InspectionSession
  -> existing LinkStore read operations
  -> existing Relations Model query/decode helpers
  -> existing function/frame structural decoders
  -> existing BootstrapRuntime::execute
  -> existing ExecutionObserver / BoundedExecutionTrace
```

There is no `DebugExecutor`, shell-specific relation registry, copied evaluator or shadow program database.

The implementation lives under `tools/` and is intentionally not added to the installed `avm::core` header set in this gate. The stable public core contracts remain the dependencies that the tool consumes.

## Stable ownership

`InspectionSession` owns its `BootstrapRuntime` and bounded trace collector while the caller owns the `LinkStore`.

This follows the stable-address ownership contract established for `Executor`/`BootstrapRuntime`: native handlers may close over their runtime owner, so the runtime is neither copied nor moved. Consequently the inspection session is also neither copyable nor movable.

The session takes `BootstrapVocabulary` by value when constructed and passes that explicit identity set to the runtime. Constructing a session over a complete current vocabulary must not allocate replacement bootstrap identities or otherwise grow the store.

The session does not attach its collector to an externally owned runtime. `Executor::set_observer` is a non-owning setter without an observer stack/getter; owning the runtime prevents tooling from silently clobbering another caller's observer.

## Read-only inspection

The following operations are observational:

```text
inspect_link(id)
find_pair(begin,end)
outgoing(begin)
incoming(end)
query_relations(constraints)
decode_relation(entity)
function_definition(handle)
call_frame(frame)
```

They delegate to existing `LinkStore`, Relations Model and program-model helpers. They do not call `intern` or `create_point`, invent missing identities, or maintain a second entity index.

A missing exact pair remains absent. A malformed/missing entity may throw the same structural error exposed by the underlying helper, but inspection itself must not materialize repair data.

## Execution and tracing

`execute(root)` delegates directly to the owned `BootstrapRuntime`.

`trace_execute(root)` resets the session-owned `BoundedExecutionTrace`, attaches it to the same runtime executor for one execution, and detaches it on both success and exception paths. The original result or exception remains the runtime result; retained trace events use the AVM 1.3 event/failure contract unchanged.

Trace capacity is fixed when the session is constructed. Capacity exhaustion remains explicit through `trace_truncated()` and never becomes an implicit unbounded collector.

After a failed traced execution, the retained `Fail(phase)` events remain available for inspection. A subsequent ordinary `execute` is not observed because the collector has been detached.

## Effects

The session itself does not reinterpret execution effects. A program executed through the session may perform the same canonical effects it performs through direct `BootstrapRuntime::execute` (for example frame/binding materialization or explicit `pair_intern`).

Therefore the tooling distinction is:

```text
inspection method   -> observational, no store growth
execute/trace       -> exactly the existing program/runtime effect contract
```

## Follow-up layers

A textual/scripted command parser is a separate gate. Command strings will map to these typed operations and remain presentation syntax rather than AVM relation identities.

Persistent-session/reopen behavior is also a separate gate. It must reuse explicit persisted vocabulary/root identities and preserve the existing persistent trace/LinkId contracts.

## Non-goals

This gate does not add:

- breakpoint, step or continue control;
- handler replacement or result substitution;
- a symbolic LinkId registry;
- JSON/Anum parsing inside the session;
- implicit store enumeration;
- implicit trace persistence;
- backend-specific semantic behavior;
- new `LinkStore` methods or indexes.
