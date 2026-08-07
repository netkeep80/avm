# RawCarrier: `raw(A)` is not `den(A)`

`RawCarrier` stores opaque source material independently from AVM denotation links.

The boundary enforces the invariant:

```text
raw(A) can exist while den(A) does not exist
```

and, conversely, a realized denotation can remain after its original raw carrier entry has been deleted.

## Contract

```text
RawDocumentId put(RawBytes)
optional<RawBytes> get(RawDocumentId) const
bool contains(RawDocumentId) const
bool erase(RawDocumentId)
size_t size() const
```

The interface has no `LinkStore` parameter, no `ProjectionDescription`, no executor and no parser API. Therefore a raw load cannot materialize a link as an accidental side effect.

`RawBytes` is binary, not text. Zero bytes and arbitrary byte values are valid payload content. Interpretation and character encoding belong to an external protocol adapter.

## Identity

`RawDocumentId` identifies a carrier record only. It is not a `LinkId` and does not define denotation identity.

The in-memory reference carrier currently allocates a fresh document ID for every `put`, even for byte-identical payloads. Another backend may choose a different deduplication policy. That storage policy must not change the denotation produced by an external projector.

## Lifecycle independence

The supported lifecycle is deliberately asymmetric:

```text
put(raw)                 -> only RawCarrier changes
project(raw, context)    -> external operation; produces ProjectionDescription
find_projection(...)     -> only observes LinkStore
realize_projection(...)  -> explicitly changes LinkStore
erase(raw)               -> only RawCarrier changes
```

Deleting raw source never cascades into `LinkStore`. Deleting realized denotation, when such an operation is introduced, must likewise be explicit and must not be inferred from raw carrier lifetime.

## Relationship to Anum

For a future Anum adapter:

```text
RawCarrier
   |
   v
external Anum L3 parser / validator / projector(context)
   |
   v
ProjectionDescription
   |
   +--> find_projection(const LinkStore&, ...)
   `--> realize_projection(LinkStore&, ...)
```

AVM does not know whether the bytes are Anum, JSON, a binary protocol or something else. This is intentional: parser/protocol semantics remain above the L4 memory boundary.

## Reference implementation

`InMemoryRawCarrier` is a test/reference backend only. Persistent raw storage, files, PMM integration, hashes and retention policies belong to backend work rather than to the semantic memory contract.
