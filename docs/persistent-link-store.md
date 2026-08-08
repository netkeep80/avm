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

A new mutation can fail while updating in-memory indexes or while opening/writing/flushing the snapshot. Once a LinkId has been allocated, AVM treats `insert_link + persist` as one guarded mutation commit. If any part of that region throws, the live `PersistentLinkStore` object enters an explicit **faulted state** rather than continuing to expose a potentially partial or uncommitted in-memory view.

After faulting:

- `faulted()` returns `true` without touching the backend;
- `path()` remains available for diagnostics;
- all `LinkStore` reads and mutations reject access instead of exposing possibly uncommitted in-memory state;
- the original exception is still propagated by the mutation that failed;
- the object is not silently retried or repaired.

An existing-pair `intern(a,b)` is read-like: it returns the already canonical LinkId without rewriting the snapshot, so an unavailable output path does not by itself fault a healthy object until a genuinely new mutation requires persistence.

The correct recovery boundary is to discard the faulted object and explicitly reopen/repair the backing store according to the caller's policy. If a failed direct snapshot write damaged the file, reopen may reject it through the ordinary corruption checks.

This fault-state rule is ordinary **in-process exception safety**, not a crash-consistency guarantee. V1 still has no WAL, fsync protocol, atomic snapshot swap, locking or concurrent-writer support. A process crash or power loss during the direct snapshot rewrite may leave the file incomplete; those durability concerns belong to a production backend such as a later PMM adapter.

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
