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

## AVM 1.1 — associative query facilities — complete ✅

Epic: #83.

### Gate 9 — constrained Relations Model queries ✅

Implemented through #84/#85.

Public query shape:

```text
relation? / subject? / object?
-> existing find/outgoing/incoming paths
-> structural decode/filter
-> deterministic RelationMatch list
```

Properties:

- at least one constraint;
- no `intern` / `create_point` in queries;
- no guessed full-store enumeration;
- deterministic ordering/deduplication;
- InMemory/Persistent/reopen equivalence;
- installed public package consumption.

### Gate 10 — measured query/index evolution ✅ decision

Fan-out scaling was measured at 1/8/64/256 in #86/#87. Existing-index query cost grows with candidate expansion, but no concrete AVM workload/SLA currently justifies another physical/persistent index.

Decision:

- keep canonical `find/outgoing/incoming` as the only primitive lookup/index contract;
- defer extra indexes until real workload evidence demonstrates a need;
- any future extension must compare against the recorded scaling baseline and prove InMemory/Persistent conformance.

No backend-specific map is promoted into semantic code merely for convenience.

## AVM 1.2 — link-native structural standard library

Epic: #88.

### Gate 11 — read-only structural primitives ✅

Implemented through #89/#90.

Native observational kernel:

```text
link_begin(expr)
link_end(expr)
identity_equal(a,b)
link_exists(a,b)
```

Properties:

- arguments are evaluated ordinary AVM expressions;
- handlers observe `LinkStore::get/find` only;
- no missing-link materialization;
- canonical Boolean results for predicates;
- `link_exists` does not overload `nil` as a missing sentinel;
- persisted 1.1 vocabulary can upgrade without changing older LinkIds.

### Gate 12 — explicit canonical pair effect ✅

Implemented through #91/#92.

```text
pair_intern(a,b) -> canonical LinkId(a,b)
```

Properties:

- the handler delegates to canonical `LinkStore::intern`;
- missing pair creates exactly one link;
- existing/repeated materialization is idempotent;
- no point synthesis in the effect handler;
- vocabulary migration supports complete 15-ID, 19-ID and current 20-ID generations with prevalidation before writes;
- persistent reopen preserves the materialized LinkId.

### Gate 13 — standard-library composition (current)

Child: #93.

Goal: prove that higher-level structural operations are ordinary AVM functions instead of new native handlers.

Initial compositions:

```text
is_self_link(x) = identity_equal(link_begin(x), link_end(x))

pair_matches(x,b,e) = AND(identity_equal(link_begin(x), b),
                          identity_equal(link_end(x), e))
```

Requirements:

- function bodies are ordinary link-native expressions;
- function handles have no native handlers;
- composed functions may call other composed functions;
- definitions survive persistent reopen through explicit handles;
- effect accounting keeps existing link-native binding/call-frame materialization visible instead of hiding it in an ephemeral host-language stack;
- no new production relation identity or storage API merely for a reducible library operation.

Completion of this gate closes AVM 1.2.

## Later AVM 1.x directions

After the structural standard-library foundation:

1. additional thin protocol/front-end adapters;
2. debugger/REPL and execution observability;
3. packaging/integration tooling;
4. visualization/GUI;
5. production physical backends and persistence experiments;
6. standard-library growth by link-native composition, with new native primitives only for irreducible observation/effect boundaries.

Every extension must reuse the existing `LinkStore -> Relations Model -> Executor` architecture.

## Dependency rule

If a gate depends on an unfinished PR, independent preparation may proceed, but dependent code is not merged before the dependency is green. Legacy code is deleted after consumer migration; Git stores history.
