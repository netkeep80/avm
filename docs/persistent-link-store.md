# Persistent reference LinkStore

`PersistentLinkStore` is the first persistence conformance backend for the AVM 1.0 `LinkStore` contract. Its purpose is to prove stable link identity and deterministic reopen semantics. It is deliberately a simple snapshot backend, not a production storage engine.

## Contract

The backend implements exactly the same semantic interface as `InMemoryLinkStore`:

```text
create_point
intern(begin, end)
find(begin, end)
get(id)
outgoing(begin)
incoming(end)
contains(id)
size()
```

The Executor, Relations Model codec and projection layer depend only on `LinkStore`; they do not know whether the store is in-memory or persistent.

## Identity and reopen

Link IDs start at `1` and are persisted explicitly. Records are ordered by LinkId and v1 requires a contiguous namespace. After reopen:

- every persisted LinkId denotes the same `(begin,end)` pair;
- exact pair identity is rebuilt from records;
- outgoing and incoming indexes are rebuilt deterministically;
- `intern(a,b)` reuses the same canonical LinkId;
- `find` and all other read operations do not rewrite the snapshot.

A point remains an ordinary self-link `(id,id)`.

## Snapshot format v1

All integers are unsigned little-endian values. The file layout is:

```text
8 bytes   magic = "AVMLNK1\0"
u32       version = 1
u32       reserved = 0
u64       record_count
repeat record_count times:
    u64   LinkId
    u64   begin LinkId
    u64   end LinkId
```

The loader rejects:

- wrong or truncated magic;
- unsupported version or non-zero reserved field;
- truncated integers/records;
- non-contiguous or duplicate LinkIds;
- duplicate canonical `(begin,end)` pairs;
- endpoints that do not exist in the completed snapshot;
- impossible partial self-references;
- trailing bytes after the declared records.

This strictness prevents a corrupt file from being silently accepted as a different aset.

## Mutation durability

For v1 every successful new point or new canonical pair rewrites the complete snapshot. Reusing an existing pair performs no write.

This gives simple deterministic persistence semantics suitable for conformance tests, but it is **not** a crash-consistency guarantee. In particular v1 has no WAL, fsync protocol, atomic snapshot swap, locking or concurrent-writer support. Those concerns belong to a production backend such as a later PMM adapter.

## Why this backend exists

AVM needs a concrete proof that persistence does not leak into VM semantics. The reference backend therefore optimizes for auditability:

```text
same LinkStore contract
        |
        +-- InMemoryLinkStore
        |
        +-- PersistentLinkStore -- close/reopen --> same LinkIds and links
```

A future PMM or LinksPlatform adapter should pass the same backend-neutral conformance suite rather than changing Executor behavior.
