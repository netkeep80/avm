# AVM execution kernel

The AVM 1.0 execution kernel runs a Relations Model entity by its `LinkId`.
It does not parse JSON and it does not dispatch operators by textual names.

## Execution path

```text
entity LinkId
    |
    v
decode_relation_entity
    |
    v
ExecutionContext
    |
    v
relation LinkId -> native bootstrap handler
    |
    v
result LinkId
```

## Execution context

The minimal context carries the instantiated Relations Model entity:

```text
entity
relation
subject
object
parent (optional)
```

The context is explicit. It replaces the idea that execution state can live only in hidden global C++ structures.

## Native bootstrap boundary

A native handler is registered by a relation `LinkId`:

```text
relation identity -> C++ handler
```

This is deliberately a bootstrap mechanism. It lets the first executable relations exist before user-defined program structures have been migrated into the associative store.

The native registry must not become a second program database. User-defined function bodies, parameters, bindings and call frames are migrated into links by the next stage of the AVM 1.0 roadmap.

## Invariants

1. `Executor::execute` accepts an entity `LinkId`, not a JSON expression.
2. Relation dispatch uses a relation `LinkId`, not a string such as `"Not"` or `"If"`.
3. The entity is decoded through the canonical Relations Model codec.
4. Unknown relations fail explicitly and do not mutate the store.
5. A native handler must return an existing `LinkId`.
6. Parent context is explicit and can later support nested execution/call frames.
7. The executor depends on `LinkStore` semantics, not on a physical storage backend.

## Compatibility path

The existing `interpret(const json&)`, `resolve_operator`, `func_env` and `param_stack` remain temporarily for behavioral compatibility tests. They are not part of the target kernel.

They will be removed after their behavior is represented and executed through links as tracked by issue #25.
