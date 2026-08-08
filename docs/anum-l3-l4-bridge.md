# Anum L3 → AVM L4 structural denotation bridge

## Status

AVM consumes the canonical storage-neutral `AnumDenotation` v0.2 handoff produced by `netkeep80/anum_docs`.

The production adapter is:

```text
adapters/anum_denotation_bridge.h
```

The old protocol-value-only `AnumL3Projection` bridge is removed. There is one L3→L4 adapter path.

## Responsibility split

Upstream L3 owns:

```text
raw [ ] 1 0 parsing
context validation
root / quote / relative projection
root-opening collapse
recursive structural denotation
canonical inverse serialization
```

AVM owns none of those rules.

AVM receives only a typed result:

```text
structural
raw
quoted-raw
```

For `structural`, the handoff contains:

```text
sorted opaque anchor keys
topologically ordered description-local nodes
start/end refs to anchors or earlier nodes
one root ref
```

## Mapping to ProjectionDescription

The caller supplies an anchor resolver:

```text
opaque upstream anchor key -> LinkId
```

The bridge then performs a purely structural translation:

```text
Anum anchor ref -> ProjectionRef::anchor(resolved LinkId)
Anum node ref   -> ProjectionRef::node(same local id)
Anum node       -> ProjectionNode(start,end)
Anum root       -> ProjectionDescription.root
```

Node order and repeated node references are preserved exactly. The adapter does not intern or deduplicate structure.

`raw` and `quoted-raw` return no `ProjectionDescription`.

## Effects

The bridge has no `LinkStore` parameter and therefore cannot read or mutate AVM memory.

L4 effects remain explicit:

```text
bridge_anum_denotation(...)  # structural translation only
find_projection(...)         # observational / non-materializing
realize_projection(...)      # explicit materialization
```

If a resolver returns a syntactically valid LinkId that is not present in a particular store, the bridge still remains pure. `find_projection` returns no result without mutation, and `realize_projection` rejects the missing physical anchor before creating ordinary projection nodes.

## Validation boundary

AVM validates the transport contract, not Anum grammar:

- anchor keys are sorted, unique and non-empty;
- node IDs are contiguous and topological;
- anchor refs name declared anchors;
- node refs target only earlier nodes;
- structural/non-structural payload shapes do not mix;
- every structural anchor resolves to a non-invalid LinkId.

AVM intentionally does **not** validate:

```text
whether a raw carrier is valid Anum
whether brackets encode a specific nested link
whether root-opening collapse was applied correctly
which ProjectionContext should be used
```

Those are canonical L3 responsibilities.

## Cross-language conformance

Adapter tests vendor versioned snapshots from `netkeep80/anum_docs` v0.2 under:

```text
test/conformance/anum-denotation-conformance-v0.2.json
test/conformance/anum-recursive-denotation-conformance-v0.2.json
test/conformance/anum-v0.2-provenance.json
```

JSON parsing is test-only. `adapters/anum_denotation_bridge.h` remains JSON-free and parser-free.

The tests consume generic anchor-only/nested/shared-substructure vectors and actual recursive L3 expected denotations, then exercise the existing AVM projection lifecycle.

## Identity

Upstream node IDs are description-local positions, not persistent identities. The bridge maps them 1:1 to `ProjectionNodeId` only for one projection description.

Physical identity remains a `LinkStore` concern. Equal pairs may converge during explicit realization; L3 and the bridge do not pre-intern occurrence structure.
