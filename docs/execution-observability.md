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

## Diagnostics policy

The deterministic trace contract intentionally stops at AVM-owned failure phase. Human-readable exception text remains runtime diagnostic information, not stable event identity. C++ exception type names, `std::exception_ptr`, stack addresses and backend/protocol errors are not stored in `ExecutionEvent`.

A later debugger/REPL layer may display the exception it catches alongside the deterministic trace, but it must not reinterpret that host diagnostic as canonical AVM semantics.

## Non-goals

AVM 1.3 observability does not provide:

- breakpoints or step control;
- handler replacement or result substitution;
- a global trace singleton;
- implicit trace persistence in `LinkStore` or files;
- an unbounded/implicitly complete trace guarantee;
- profiler timestamps;
- exception objects or exception strings inside deterministic events;
- JSON/Anum-specific trace events;
- debugger UI or REPL commands.

Those facilities may consume the observer boundary later, but they must not become a second execution path.
