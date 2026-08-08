# External protocol adapter contract

AVM 1.0 deliberately does **not** define an Anum parser interface. The stable boundary is composition of two neutral AVM contracts:

```text
optional raw source -> RawCarrier
external parse / validate / quote / project(context)
                    -> ProjectionDescription
                    -> find_projection | realize_projection
                    -> LinkStore denotation
```

The protocol implementation stays outside the VM. It may be Anum/MTS, another binary format, JSON, a prebuilt AST or a generated structure. AVM receives only opaque raw bytes when the caller chooses to retain them and a completed structural projection when the caller wants to query or realize denotation.

## Responsibilities of the external adapter

An adapter owns all protocol/model meaning needed before the AVM boundary:

- source grammar and tokenization;
- validation;
- quoting/unquoting or description-level semantics;
- projection context `K`;
- external symbol/identity resolution into existing `Anchor(LinkId)` values;
- construction of a topologically valid `ProjectionDescription`.

The context is explicit adapter input. `LinkStore` does not guess context and `ProjectionDescription` does not perform name lookup.

## Responsibilities of AVM

AVM owns only the L4-facing mechanics:

- optional opaque raw retention through `RawCarrier`;
- a typed structural description made from `Anchor` and projection-local `Node` references;
- read-only `find_projection(const LinkStore&, ...)`;
- explicit `realize_projection(LinkStore&, ...)`;
- canonical dyad identity supplied by `LinkStore::intern`.

Neither raw loading nor projection query creates denotation.

## No mandatory adapter base class

There is intentionally no `AnumParser`, `ProtocolAdapter` virtual base class or template concept in AVM core. Such an abstraction would prescribe the source/AST/context types of external protocols without improving the memory contract.

Replaceability is structural: any component that can produce a valid `ProjectionDescription` over resolved anchors can use AVM. Tests exercise this with two unrelated toy adapters:

1. one consumes opaque binary bytes loaded from `RawCarrier`;
2. another consumes an already parsed toy AST;
3. both project the same relation entity over the same context;
4. after one realization, the other finds exactly the same canonical LinkIds.

That is the interoperability property AVM actually requires.

## Context changes denotation explicitly

If two adapters use the same structural rule but different resolved anchors/context, they may produce a different denotation. This is expected and tested. The store does not choose between contexts and does not silently create missing anchors.

## Mapping to `anum_docs`

The intended integration point follows the layer split documented in `netkeep80/anum_docs` and the invariants of its apamemory roadmap issue #72:

```text
Anum L3
  raw syntax / parser / protocol / quote / context projection
       |
       v
AVM neutral boundary
  RawCarrier (optional) + ProjectionDescription
       |
       v
AVM L4
  find_projection / realize_projection / LinkStore
```

The key invariants align directly:

- `raw(A)` may exist without `den(A)`;
- loading raw does not realize denotation;
- `find(A)` is observational and non-mutating;
- `realize(A)` is the explicit materialization operation;
- context and protocol semantics stay outside memory.

This contract does not claim to define canonical MTS quotation, Anum grammar or the final external-symbol identity policy. Those remain the responsibility of the canonical L3 implementation.

## Consequence for AVM issue #3

The old request to add Anum serialization/deserialization directly to AVM should not reintroduce a parser inside the VM. A future production integration should implement an adapter against the canonical `anum_docs` L3 API and feed this boundary. Once that adapter exists, issue #3 can be closed as superseded or narrowed to that concrete integration package.
