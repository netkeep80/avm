# Deferred function definitions

AVM 1.0 distinguishes an executable `Def` node from the immutable function-definition row it materializes.

## Why two shapes exist

Projection/import must be able to construct an entire program graph before execution while preserving declaration order. If the importer inserted the final function-definition row immediately, a `Call` located before `Def` in a sequence could incorrectly find the function.

The projection therefore emits a deferred definition expression:

```text
parameters        = List(formal1, formal2, ...)
definitionPayload = Link(parameters, bodyRoot)
payload           = Link(functionHandle, definitionPayload)
defExpression     = (function_relation, unit, payload)
```

No callable definition exists merely because this expression is present in the store.

## Execution

When `Executor` dispatches `defExpression` to the bootstrap `function_relation` handler, the runtime decodes the node and materializes:

```text
(function_relation, functionHandle, definitionPayload)
```

The stored definition remains immutable and canonical. Re-executing the same deferred definition is idempotent. Executing a different definition for the same handle is an explicit conflict.

This gives a direct ordering property:

```text
Sequence(Def(f), Call(f))  -> succeeds
Sequence(Call(f), Def(f))  -> Call observes no definition and fails
```

## Recursion

The function handle is created before the body graph is built. The body can therefore contain `Call(handle, ...)` even though the final definition row does not exist yet. Executing the `Def` node makes that recursive handle callable without rewriting the body.

## Boundary

The deferred node is part of the link-native AVM program model and has no JSON dependency. The JSON compatibility importer in #47 will use this shape to map textual `Def` syntax while keeping execution exclusively in `BootstrapRuntime` and `Executor`.
