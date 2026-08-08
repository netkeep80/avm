# AVM 1.0 execution vertical slice

This document describes the executable path exercised by `vertical_slice_tests`.

## One semantic path

```text
JSON expression
    |
    v
JsonProgramImporter
    |
    v
LinkStore (InMemoryLinkStore or PersistentLinkStore)
    |
    v
root LinkId
    |
    v
BootstrapRuntime / Executor
    |
    v
result LinkId
```

JSON is used only by the importer. `Executor` receives a `LinkId`; it does not receive or retain a JSON AST.

The same materialized program entities are executable through the abstract `LinkStore` contract. The test covers NOT/AND/OR, lazy `If`, `Def`/`Call`, and a representative recursive call.

## Persistent reopen

The persistent scenario has two distinct phases.

### Initial process/store lifetime

1. Open an empty `PersistentLinkStore`.
2. Create `BootstrapRuntime`; this introduces a bootstrap vocabulary with explicit `LinkId` identities.
3. Import JSON expressions into links.
4. Execute the imported roots and verify their observable results.
5. Retain only structural identities needed to reopen the runtime: `BootstrapVocabulary` and root `LinkId` values.
6. Close the store.

### Reopened store lifetime

1. Reopen the same `PersistentLinkStore`.
2. Construct `BootstrapRuntime(store, saved_vocabulary)`.
3. Verify runtime restoration does not change `store.size()`; truth-table materialization must reuse canonical links already present.
4. Execute the saved root `LinkId` values directly.
5. Repeat a second reopen to catch accidental one-reopen-only behavior.

No JSON parse or import is performed in the reopen phase. The persisted executable graph is therefore the program state being executed, rather than an external AST being reconstructed on every start.

## Bootstrap identity is persistent state

A bootstrap vocabulary is not recreated on reopen. Relation identities such as `and_relation`, `if_relation`, `function_relation`, and `call_relation` are part of the materialized program's semantic address space.

`BootstrapRuntime` therefore has two construction modes:

```cpp
BootstrapRuntime(store);              // create a new vocabulary
BootstrapRuntime(store, vocabulary);  // restore an existing vocabulary
```

The restore path validates that every vocabulary `LinkId` exists and that all bootstrap identities are distinct. Invalid restored metadata is rejected before execution.

A future production persistence layer may store a named/rooted reference to bootstrap metadata. The reference backend intentionally does not invent that policy: issue #59 proves stable link identity, while this vertical slice proves that the runtime can be restored when the correct structural identities are supplied.

## Backend-neutral invariant

`BootstrapRuntime`, `Executor`, Relations Model helpers and `ProgramBuilder` depend on `LinkStore`, not on `PersistentLinkStore` internals. Persistence format, index rebuild and filesystem behavior remain behind the backend boundary.

The vertical slice is compiled into `avm_core_tests`, so it participates in:

- C++20 warnings-as-errors;
- ASan + UBSan on Linux;
- Release portable runs on Linux, Windows and macOS.

This is a correctness/conformance test, not a durability or performance claim. Crash consistency, WAL/journaling and production backend tuning remain separate work.
