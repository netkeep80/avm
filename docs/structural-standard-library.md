# AVM 1.2 structural standard library

AVM 1.2 separates the structural surface into a small native kernel and an open-ended link-native library.

## Native structural kernel

The bootstrap runtime owns only the primitive relations that cannot be reduced further without already having structural observation/effect semantics:

```text
link_begin(expr)       -> begin pole of evaluated LinkId
link_end(expr)         -> end pole of evaluated LinkId
identity_equal(a,b)    -> canonical Boolean
link_exists(a,b)       -> canonical Boolean, observational
pair_intern(a,b)       -> canonical LinkId(a,b), explicit effect
```

`link_begin`, `link_end`, `identity_equal` and `link_exists` do not materialize the queried structure. `pair_intern` is the explicit canonical structural mutation boundary and delegates to `LinkStore::intern`.

## Derived operations are programs, not opcodes

A standard-library operation that can be expressed with the existing kernel must be represented as an ordinary AVM function definition rather than as another native handler.

For example:

```text
is_self_link(x) = identity_equal(link_begin(x), link_end(x))
```

and:

```text
pair_matches(x,b,e) = AND(identity_equal(link_begin(x), b),
                          identity_equal(link_end(x), e))
```

Both bodies are ordinary Relations Model expression entities created by `ProgramBuilder`. Their function handles are normal `LinkId` values and `Executor::has_native(handle)` is false.

This keeps the semantic direction:

```text
small native kernel
    -> link-native function definitions
    -> further functions may call those functions
```

rather than:

```text
ever-growing C++ opcode/native-handler registry
```

## Function calls and effect accounting

A composed function whose body contains only observational primitives is not necessarily a zero-write operation at the physical `LinkStore` level.

AVM intentionally represents function execution state as links. The existing call runtime materializes canonical binding and call-frame structures. Therefore the first execution of a particular structural call may increase the store even though the function body itself contains no structural mutation primitive.

The correct distinction is:

- **library semantics:** `is_self_link` and `pair_matches` contain no `pair_intern` and introduce no native handler;
- **execution-state semantics:** the existing function machinery may intern bindings, binding lists, frame payloads and frame entities;
- **canonical convergence:** repeating the same structurally identical call reuses those structures where their identities are the same.

Replacing link-native call frames with an ephemeral C++ stack merely to label composed functions “pure” would create a second execution-state model and is intentionally rejected.

## Persistence

Function definitions are already link-native and therefore persist with the store. To use a composed library after reopen, a caller retains the explicit function handles together with the bootstrap vocabulary it already needs to restore the runtime.

There is no hidden standard-library registry and no string-name lookup table. A handle is the identity of the function.

## Extension rule

Before adding any future native relation, ask whether its behavior can be expressed as a function over existing primitives.

If yes, implement it as links. A new native relation is justified only when it introduces a genuinely irreducible observation/effect boundary or when measured requirements demonstrate that the composition cannot satisfy the contract without violating AVM invariants.
