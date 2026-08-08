# AVM roadmap

## Planning rule

Work proceeds through dependency-ordered gates. A new layer starts only after its dependencies have a stable contract and green CI gates.

The invariant remains:

```text
external projection
  -> canonical LinkStore
  -> Relations Model codec
  -> LinkId program
  -> BootstrapRuntime / Executor
  -> result LinkId
```

No second semantic path, hidden program database, legacy storage universe or backend-specific semantic index is allowed.

## AVM 1.0 foundation — complete ✅

### Gate 1 — architecture contract ✅

- one physical primitive: directed dyad;
- opaque `LinkId` identity;
- canonical pair identity;
- reads do not materialize data;
- backend semantics separated from VM semantics.

### Gate 2 — canonical LinkStore ✅

- reference `InMemoryLinkStore`;
- explicit `find` vs `intern` separation;
- canonical identity/query conformance;
- semantic code independent of pointer identity/layout.

### Gate 3 — Relations Model codec ✅

```text
(relation, subject, object) = (relation, (subject, object))
```

### Gate 4 — execution kernel ✅

- `Executor` consumes entity `LinkId`;
- relation dispatch by `LinkId`;
- explicit execution context;
- bootstrap native handlers are not a second program database.

### Gate 5 — program-as-links ✅

- programs, bindings and frames represented by links;
- link-native vertical slice;
- legacy pointer-based `rel_t` semantic/storage universe removed.

### Gate 6 — external protocol boundary ✅

- parser/grammar/context remain external;
- AVM consumes structural projection/denotation;
- `raw(A)` separated from `den(A)`;
- `find` observational, `realize` explicit.

The canonical Anum/MTS semantics are maintained in `netkeep80/anum_docs`; AVM contains only the structural L3→L4 bridge.

### Gate 7 — integration hardening ✅

- persistent reopen/identity conformance;
- end-to-end link-native vertical slice;
- benchmark baselines;
- documentation alignment;
- warnings-as-errors, sanitizers and architecture guards.

### Gate 8 — AVM 1.0 release readiness ✅

Validated on the same public package/core contracts:

- Linux, Windows and macOS portable builds/tests;
- installed-package consumer on Linux, Windows and macOS;
- stable public `avm::core` package;
- one documented execution path;
- no legacy semantic path.

## AVM 1.1 — associative query facilities

Current epic: #83.

### Gate 9 — constrained Relations Model queries (current)

Child: #84.

Goal: read-only query facilities over the existing Relations Model and `LinkStore` indexes, without a second storage/index universe.

Public query shape:

```text
relation? / subject? / object?
-> existing find/outgoing/incoming paths
-> structural decode/filter
-> deterministic RelationMatch list
```

Requirements:

- at least one constraint;
- no `intern` / `create_point` in queries;
- no guessed full-store enumeration;
- deterministic ordering/deduplication;
- InMemory/Persistent/reopen equivalence;
- benchmark baselines for each lookup strategy;
- installed public package consumption.

### Gate 10 — measured query/index evolution

Only after Gate 9 benchmarks and real workloads show a need:

- evaluate explicit LinkStore enumeration contract;
- evaluate additional secondary/index operations;
- preserve backend replaceability and read-only semantics;
- require conformance across in-memory/persistent backends.

No backend-specific map is promoted into semantic code merely for convenience.

## Later AVM 1.x directions

After the query foundation:

1. link-native standard library expansion;
2. additional thin protocol/front-end adapters;
3. debugger/REPL and observability;
4. packaging/integration tooling;
5. visualization/GUI;
6. production physical backends and persistence experiments.

Every extension must reuse the existing `LinkStore -> Relations Model -> Executor` architecture.

## Dependency rule

If a gate depends on an unfinished PR, independent preparation may proceed, but dependent code is not merged before the dependency is green. Legacy code is deleted after consumer migration; Git stores history.
