# AVM 1.0 functions, bindings and call frames

This layer moves function runtime state out of the legacy C++ `func_env` / `param_stack` side channel and into the same `LinkStore` used by program and data entities.

## Function definitions

A function still uses the program-model encoding introduced by #35:

```text
parameters = List(formal1, formal2, ...)
payload    = Link(parameters, bodyRoot)
definition = (function, handle, payload)
```

The handle is an independently created LinkId, so a recursive body can refer to its own function before the immutable definition entity is materialized.

Executing a definition validates the stored shape and returns `nil`. Definition availability itself is structural: a link-native call references the handle directly. A future JSON compatibility importer is responsible for preserving textual declaration order while resolving names to handles.

## Bindings

Each evaluated actual argument is bound to its formal parameter with a Relations Model entity:

```text
(binding, formal, actualValue)
```

There is no `map<string, value>` in the new runtime. Formal identity is a LinkId and actual values are LinkIds.

## Call frames

Bindings are collected into a canonical link list and attached to an immutable call frame:

```text
bindings = List(binding1, binding2, ...)
payload  = Link(functionHandle, bindings)
frame    = (frame, parentFrameOrNil, payload)
```

`ExecutionContext` carries the current frame LinkId. Every recursive execution of a child expression preserves that frame unless a function call deliberately creates a child frame.

Nested calls therefore form a parent-frame chain entirely in the associative store. Parameter resolution walks the chain from the current frame toward `nil`, validating that every referenced binding really has `binding_relation`.

## Call execution

For `(call, unit, payload)` the runtime performs:

1. decode function handle and argument-expression list;
2. locate the immutable function definition;
3. verify arity;
4. verify the current frame depth against the configured recursion limit;
5. evaluate actual arguments in the caller frame;
6. materialize binding entities;
7. materialize a child frame linked to the caller frame;
8. execute the function body with the child frame.

This preserves lexical parameter identity without a global C++ stack. Recursive calls work because the function body already contains the same handle LinkId.

## Recursion guard

`BootstrapRuntime` accepts a maximum call depth, defaulting to 1000. Depth is derived by traversing parent-frame links rather than by reading a C++ container size. Malformed frame chains, non-frame parents and invalid frame payloads fail explicitly.

The frame vocabulary identity itself is a self-link, like all bootstrap identities. It must not be confused with a frame instance; `decode_call_frame` rejects it explicitly. This invariant is covered by tests because the relation identity naturally appears in `LinkStore::outgoing(frame_relation)`.

## Test coverage

`function_runtime_tests` covers:

- one- and two-parameter functions;
- Boolean expressions inside a function body;
- nested calls;
- finite recursion;
- unbounded recursion hitting the depth guard;
- arity mismatch;
- undefined functions;
- unbound parameters;
- malformed frames and definitions;
- physical presence of binding/frame entities in `LinkStore`.

`frame_runtime_tests` separately covers:

- decoded root-frame structure;
- formal→actual binding rows;
- executing a parameter against an explicit frame LinkId;
- parent/child frame linkage for nested calls;
- rejection of the frame vocabulary self-link;
- rejection of non-binding entries and invalid parent frames;
- rejection of a zero recursion-depth configuration.

Together with the existing suites, this keeps function semantics independently testable without JSON or the legacy LinksPlatform path.
