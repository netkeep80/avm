# Persistent inspection session contract

AVM 1.4 inspection tooling remains backend-neutral. A persistent session is therefore not a new session class or backend adapter: the same typed `InspectionSession` is constructed over an already opened `PersistentLinkStore` and an explicit persisted `BootstrapVocabulary`.

## Reopen boundary

A caller/tool owns the persistent store and the identities required to use it:

```text
persistent file
  -> PersistentLinkStore open/validate/rebuild indexes
  -> explicit BootstrapVocabulary LinkIds
  -> explicit program/root/function LinkIds
  -> InspectionSession(store, vocabulary)
  -> existing read/query/execute/trace APIs
```

`InspectionSession` does not know a filesystem path and does not open, repair or reinterpret the backend. Corruption/version errors remain the responsibility of `PersistentLinkStore` and occur before a session can be constructed.

## Identity

For one logical persistent store, LinkIds are exact across close/reopen. Tooling therefore reuses the caller-supplied vocabulary/root/handle IDs verbatim.

The persistent-session contract does not define numeric LinkIds as portable across independent stores. The AVM 1.3 backend-neutral renaming rule still applies when comparing independently constructed stores.

## No hidden bootstrap

Constructing an inspection session over a complete explicit vocabulary must not create replacement bootstrap identities. A read-only session that is opened, inspected and destroyed must leave `LinkStore::size()` unchanged.

The session also keeps no hidden symbolic database of selected roots or handles. File path, selected root, command history and trace capacity are host tooling configuration unless a future explicit persistence projection is designed.

## Converged execution state

Function calls can materialize canonical binding/frame links as part of ordinary AVM execution. Persistent trace equality is therefore asserted only after that existing link-native state has converged.

The conformance sequence is:

1. create a persistent store and bootstrap vocabulary;
2. materialize a function/program and execute the function once to converge canonical call state;
3. capture a complete bounded call trace and final store size;
4. close the store;
5. reopen with the exact saved vocabulary/root/handle LinkIds;
6. construct `InspectionSession` and prove construction/read operations do not grow the store;
7. execute an already materialized root without reparsing frontend data;
8. trace the converged function call and require exact `ExecutionEvent` equality;
9. repeat reopen again and require the same identities, trace and store size.

This reuses the AVM 1.3 rule that the same persistent logical store has exact trace identity after reopen.

## Inspection guarantees

Across reopen the session must preserve direct observations of:

- `LinkId -> (begin,end)`;
- exact pair lookup;
- Relations Model decode and constrained query results;
- function definition identity;
- call-frame structural decode after execution;
- deterministic execution result;
- bounded trace events/failure phases.

`reset_trace()` changes only host tooling state and never the persistent store.

## Non-goals

Persistent inspection does not add:

- hidden bootstrap or identity migration;
- a shell sidecar database;
- implicit persistence of trace/history/bookmarks;
- backend-specific VM semantics;
- repair/recovery logic above `PersistentLinkStore`;
- cross-store numeric LinkId equivalence;
- frontend reparsing as a requirement for reopened execution.
