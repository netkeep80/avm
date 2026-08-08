# AVM execution kernel

The AVM 1.0 execution kernel runs a Relations Model entity by its `LinkId`. It does not parse JSON and it does not dispatch operators by textual names.

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
relation LinkId -> bootstrap/native handler or link-native program structure
    |
    v
result LinkId
```

## Execution context

The context is explicit and carries the instantiated Relations Model entity and call-state needed by execution. Hidden global C++ state is not an execution contract.

At minimum, relation dispatch is derived from the canonical decoded entity:

```text
entity
relation
subject
object
parent/context when applicable
```

Program bindings and call frames belong to the associative model rather than a second external environment.

## Bootstrap boundary

A native handler may be registered by a relation `LinkId`:

```text
relation identity -> C++ handler
```

This is a bootstrap mechanism, not a second program database. Native handlers provide the minimal bridge needed to execute the associative vocabulary. New language behavior must preserve relation-identity dispatch and must not create a parallel JSON/string-dispatch runtime.

## Invariants

1. `Executor::execute` accepts an entity `LinkId`, not a JSON expression.
2. Relation dispatch uses relation identity, not strings such as `"Not"` or `"If"`.
3. Entities are decoded through the canonical Relations Model codec.
4. Unknown relations fail explicitly and observational operations do not mutate the store.
5. A native handler returns an existing/canonical `LinkId` according to the operation contract.
6. Execution context and call state are explicit or represented in links; they are not hidden singleton state.
7. The executor depends on `LinkStore` semantics, not on a physical storage backend.
8. JSON/program-session adapters remain outside the execution kernel.

## Removed historical path

The historical `rel_t` universe and JSON-centric `eval()`/`interpret()` compatibility runtime were removed after their consumers migrated. They are not supported AVM 1.0 APIs and should not be reintroduced as a fallback path; Git history retains them for archaeology.
