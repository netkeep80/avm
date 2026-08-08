# Execution observability contract

AVM 1.3 adds observation to the existing execution path without adding a traced executor, debugger-specific runtime or second semantic representation.

## Boundary

There is still one execution path:

```text
entity LinkId
  -> decode Relations Model entity
  -> ExecutionContext
  -> optional Enter observation
  -> relation-LinkId dispatch
  -> handler / nested Executor::execute calls
  -> result validation
  -> optional Return observation
  -> result LinkId
```

If execution fails after a valid `ExecutionContext` exists, the invocation emits `Fail` and rethrows the original exception. Failures before a context can be decoded do not emit an event.

## Public event model

`ExecutionEvent` contains only deterministic AVM data:

```text
kind = Enter | Return | Fail
context = {
  entity,
  relation,
  subject,
  object,
  optional parent,
  optional frame
}
optional result LinkId
optional failure phase
```

`result` is present only for `Return`. `failure_phase` is present only for `Fail` and uses the finite AVM-owned taxonomy:

```text
Dispatch          — no native handler exists for the decoded relation
Handler           — the selected handler throws
ResultValidation  — the handler returns a LinkId that is absent from LinkStore
```

The phase says where canonical execution failed. It does not encode the exception message, C++ type, backend error or protocol-specific diagnostic.

There are deliberately no timestamps, thread IDs, pointers, textual opcode names, JSON values, Anum values or backend-specific data in the event contract.

## Non-controlling observer

An `ExecutionObserver` receives immutable `ExecutionEvent` values. It is not passed an `Executor`, `LinkStore`, mutable context or mutable result reference.

Observer delivery is isolated inside `Executor`: exceptions thrown by `ExecutionObserver::observe` are suppressed and cannot replace the program result or program exception. This applies to successful return and to every failure phase. An external collector may therefore use ordinary C++ facilities without becoming semantic control flow.

The observer object is caller-owned. `Executor` stores only a non-owning pointer set at construction or by `set_observer`; the caller must keep the observer alive while attached.

## Event ordering and unwind

Each invocation that reaches a valid context emits:

```text
Enter(context)
  ...nested execute events...
Return(context,result)
```

or:

```text
Enter(context)
  ...nested execute events...
Fail(context,phase)
```

Nested calls are observed naturally because handlers already recurse through the same canonical `Executor::execute` method. Failure unwind therefore remains stack-shaped without a second trace stack. If a child handler throws while executing inside a parent handler, the event order is:

```text
Enter(parent)
Enter(child)
Fail(child, Handler)
Fail(parent, Handler)
```

The original child exception continues to propagate unchanged.

## Determinism and execution state

For the same canonical store/program/vocabulary state, an observer sees the same LinkId-based event sequence and the same AVM failure phases. Function calls remain link-native: bindings and call frames may be materialized by existing runtime semantics. Once those canonical structures converge, repeated identical calls produce the same event sequence and do not need a separate host-language trace stack.

Observation itself never materializes links. Attaching or detaching an observer does not change `LinkStore` semantics.

## Bounded tooling collector

`BoundedExecutionTrace` is a reusable host-memory consumer of `ExecutionObserver`. It is a tooling utility, not part of AVM semantic state:

```cpp
avm::BoundedExecutionTrace trace(128);
runtime.executor().set_observer(&trace);
const avm::LinkId result = runtime.execute(root);

for (const avm::ExecutionEvent &event : trace.events())
{
    // inspect deterministic AVM events
}

if (trace.truncated())
{
    // configured capacity was insufficient
}
```

The event capacity is fixed when the collector is constructed. Its `std::vector` storage is reserved in the constructor, before the collector is normally attached to an executor. Construction/reservation may therefore report normal host allocation failures to the caller before execution starts.

During observation, configured capacity exhaustion does not allocate, throw or fabricate an event. The collector retains the exact event prefix that fits and sets `truncated()==true`. A zero-capacity collector stores no events and becomes truncated as soon as any event arrives.

`reset()` clears retained events and truncation state while preserving the configured `max_events()` and reserved storage policy. Event access is exposed as an immutable `std::span<const ExecutionEvent>`.

This bounded policy distinguishes two concepts that must not be conflated:

```text
ExecutionEvent semantics  = canonical AVM observation contract
BoundedExecutionTrace     = optional host-memory tooling storage
```

The collector does not own an `Executor`, `LinkStore`, backend, protocol adapter or persistence target. It never writes traces into links or files implicitly.

## Persistent reopen and backend-neutral equivalence

Trace comparison must respect opaque `LinkId` identity. Raw numeric LinkIds are meaningful only inside one logical store; AVM does not define a global LinkId namespace across independent stores.

Two different conformance claims therefore use different comparison rules.

### Same persistent logical store across reopen

`PersistentLinkStore` preserves its LinkIds across close/reopen. After program construction and link-native call-frame/binding state have converged, the same stored program and explicit bootstrap vocabulary must produce the exact same complete `ExecutionEvent` sequence after reopen, including the numeric values of every LinkId field.

The reopen contract covers:

```text
entity / relation / subject / object
optional parent / frame
optional result
kind / failure_phase
```

Repeated reopen/execution of the converged program must not grow the store merely because observation is attached.

### Independent stores/backends

An independently constructed `InMemoryLinkStore` and `PersistentLinkStore` may assign different numeric LinkIds to equivalent structure. Comparing raw numbers between them would violate opaque identity semantics.

Backend-neutral conformance therefore compares complete traces modulo a bijective renaming of observed LinkIds. The reference test uses canonical first-occurrence renaming:

1. walk events in execution order;
2. visit each LinkId-bearing field in fixed order: `entity`, `relation`, `subject`, `object`, `parent`, `frame`, `result`;
3. assign a local ordinal when an observed LinkId first appears;
4. reuse that ordinal on every later occurrence of the same LinkId;
5. preserve `nullopt`, `ExecutionEventKind` and `ExecutionFailurePhase` exactly.

This normalization preserves equality/aliasing relationships while discarding backend-local numeric allocation choices. It is a conformance helper, not a production identity registry or serialization format.

A truncated trace is not eligible for an equivalence claim: it represents only a retained prefix, so normalization/comparison must reject it or otherwise explicitly decline completeness.

## Trace-enabled CLI consumer

The repository CLI is the first user-facing consumer of the AVM 1.3 observability boundary. Its normal compatibility mode remains unchanged:

```text
avm program.json
```

Trace mode is explicit:

```text
avm --trace program.json
avm --trace-limit 64 program.json
```

The execution path is still the existing JSON compatibility projection and canonical runtime:

```text
JSON input
  -> JsonProgramImporter
  -> LinkId program
  -> attach BoundedExecutionTrace to JsonCompatibilitySession::runtime().executor()
  -> BootstrapRuntime::execute(LinkId)
  -> result LinkId
  -> existing JSON result projection
```

There is no `TraceExecutor`, copied evaluator or trace-specific opcode dispatch. JSON is a frontend format only; the trace collector sees the same `ExecutionEvent` values that any core observer sees.

CLI trace rendering is presentation, not semantic identity. Each line displays the event kind and numeric LinkId fields (`entity`, `relation`, `subject`, `object`, optional `parent`, `frame`, `result`) plus the AVM failure phase. Human labels such as `enter`, `return`, `handler` or `dispatch` are textual renderings of the enums, not execution opcodes.

The final summary reports retained event count and `complete`/`truncated` status. `--trace-limit` exists so resource bounds are explicit rather than silently dropping events.

Trace mode intentionally uses `import_program` + `execute` + `project_result` instead of the convenience `interpret()` wrapper. This allows a failing execution to render its retained `Fail(phase)` events and still propagate the original failure to the CLI top-level diagnostic/exit-status policy. Normal compatibility mode continues to use `interpret()` and therefore keeps its historical behavior.

A non-expression JSON value is valid in normal roundtrip mode but rejected explicitly in trace mode because there is no execution context to observe. AVM does not fabricate execution events for value serialization.

## Diagnostics policy

The deterministic trace contract intentionally stops at AVM-owned failure phase. Human-readable exception text remains runtime diagnostic information, not stable event identity. C++ exception type names, `std::exception_ptr`, stack addresses and backend/protocol errors are not stored in `ExecutionEvent`.

The CLI may display the host exception it catches alongside the deterministic trace, but it does not reinterpret that host diagnostic as canonical AVM semantics.

## Non-goals

AVM 1.3 observability does not provide:

- breakpoints or step control;
- handler replacement or result substitution;
- a global trace singleton;
- implicit trace persistence in `LinkStore` or files;
- an unbounded/implicitly complete trace guarantee;
- profiler timestamps;
- exception objects or exception strings inside deterministic events;
- globally comparable numeric LinkIds across independent stores;
- a production trace-normalization identity registry;
- JSON/Anum-specific canonical trace events;
- an interactive debugger UI or REPL command language.

Those facilities may consume the observer boundary later, but they must not become a second execution path.
