# AVM 1.0 architecture contract

This document defines the engineering boundaries for the AVM 1.0 foundation. It intentionally does not define the whole Meta-Theory of Links (MTS); it defines how AVM stores and executes associative structures.

## Layers

```text
A0  Link model       LinkId -> (begin, end)
A1  LinkStore        canonical identity, queries and explicit writes
A2  Relations Model  (relation, subject, object) <-> nested dyads
A3  Execution        context + relation dispatch + program evaluation
A4  Projection       JSON, Anum and other external representations
A5  Backend          in-memory, LinksPlatform, PMM or another store
```

A higher layer must not silently redefine a lower layer.

## A0. Link model

The physical primitive of the AVM 1.0 core is a directed dyad:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

`LinkId` is an opaque identity. Semantic code must not derive meaning from a C++ pointer value or from the physical layout of a backend.

An independent identity is bootstrapped as a self-link `(x, x)`. This keeps the reference in-memory implementation link-only instead of introducing a second physical "atom" record type.

## A1. LinkStore

The canonical storage contract separates reads from writes.

| Operation | May mutate | Meaning |
|---|---:|---|
| `create_point()` | yes | Introduce a new independent self-link identity |
| `intern(begin,end)` | yes | Return the canonical link for a pair, creating it when missing |
| `find(begin,end)` | no | Find an existing exact pair |
| `get(id)` | no | Read endpoints |
| `outgoing(begin)` | no | Read links that begin at an identity |
| `incoming(end)` | no | Read links that end at an identity |

The important invariant is:

```text
find(a, b) never creates Link(a, b)
```

and canonicalization means:

```text
intern(a, b) == intern(a, b)
```

inside one logical store.

## A2. Relations Model compatibility

`jsonRVM` represents an executable entity as the triplet:

```text
(relation, subject, object)
```

AVM uses one canonical nesting order:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

therefore:

```text
(relation, subject, object)
= (relation, (subject, object))
```

The Relations Model codec is responsible for this mapping. `LinkStore` itself does not know what a relation, subject or object is.

## A3. Execution

The target executor accepts a root/entity `LinkId`, not a JSON AST. It decodes a Relations Model entity, constructs an explicit execution context and dispatches by relation identity.

Native C++ handlers are allowed as a bootstrap vocabulary, but their keys are relation `LinkId`s in the associative store. User-defined program bodies must eventually have an associative representation rather than living only in `std::map` or `nlohmann::json` objects.

The current `interpret(const json&)`, string operator dispatch, `func_env` and `param_stack` are compatibility mechanisms. They are not the target AVM 1.0 execution model and must be removed after their observable behavior is covered by the associative path.

## A4. Projection

JSON and Anum are external representations.

```text
external representation
-> parser / codec / projection
-> LinkStore + Relations Model
-> execution
```

The executor must not depend on `nlohmann::json` as its internal instruction type.

For Anum, AVM follows the separation already used by the Anum/MTS work:

```text
raw(A) != den(A)
load(A) does not imply materializing den(A)
find(A) is non-mutating
realize(A) is an explicit materializing operation
```

The Anum parser and context projection belong outside the storage layer.

## A5. Backend

Storage backends implement the same `LinkStore` semantics. They do not define VM relations, JSON rules or Anum semantics.

The first backend is `InMemoryLinkStore`. Persistent adapters are added only after the core identity/query contract is covered by conformance tests.

## Core invariants

1. One physical core primitive: a directed dyad.
2. Opaque link identity.
3. Canonical identity for an exact pair.
4. Read/query operations do not materialize missing links.
5. Relations Model triplets use exactly `(relation, (subject, object))`.
6. JSON is a projection, not the VM instruction storage.
7. Execution consumes link identities.
8. User-defined code and call state migrate into the associative model.
9. Backend implementation is independent from VM semantics.
10. Legacy paths are deleted after migration; Git is the history store.

## Migration sequence

The implementation sequence is tracked by epic #31 and issues #21-#27:

```text
architecture contract
-> LinkStore
-> Relations Model codec
-> execution context/kernel
-> program-as-links
-> Anum boundary
-> persistence + end-to-end integration
```
