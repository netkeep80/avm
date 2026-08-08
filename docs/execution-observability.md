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
```

`result` is present only for `Return`.

There are deliberately no timestamps, thread IDs, pointers, textual opcode names, JSON values, Anum values or backend-specific data in the event contract.

## Non-controlling observer

An `ExecutionObserver` receives immutable `ExecutionEvent` values. It is not passed an `Executor`, `LinkStore`, mutable context or mutable result reference.

Observer delivery is isolated inside `Executor`: exceptions thrown by `ExecutionObserver::observe` are suppressed and cannot replace the program result or program exception. This lets an external collector use ordinary C++ facilities while keeping observer failure outside semantic control flow.

The observer object is caller-owned. `Executor` stores only a non-owning pointer set at construction or by `set_observer`; the caller must keep the observer alive while attached.

## Event ordering

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
Fail(context)
```

Nested calls are observed naturally because handlers already recurse through the same canonical `Executor::execute` method.

## Determinism and execution state

For the same canonical store/program/vocabulary state, an observer sees the same LinkId-based event sequence. Function calls remain link-native: bindings and call frames may be materialized by existing runtime semantics. Once those canonical structures converge, repeated identical calls produce the same event sequence and do not need a separate host-language trace stack.

Observation itself never materializes links. Attaching or detaching an observer does not change `LinkStore` semantics.

## Non-goals

AVM 1.3 gate 1 does not provide:

- breakpoints or step control;
- handler replacement or result substitution;
- a global trace singleton;
- implicit trace persistence in `LinkStore`;
- a core `std::vector` trace collector;
- profiler timestamps;
- JSON/Anum-specific trace events;
- debugger UI or REPL commands.

Those facilities may consume the observer boundary later, but they must not become a second execution path.
