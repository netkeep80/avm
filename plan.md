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

## AVM 1.2 — link-native structural standard library — complete ✅

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

### Gate 13 — standard-library composition ✅

Implemented through #93/#94.

Higher-level structural operations are ordinary AVM functions instead of new native handlers:

```text
is_self_link(x) = identity_equal(link_begin(x), link_end(x))

pair_matches(x,b,e) = AND(identity_equal(link_begin(x), b),
                          identity_equal(link_end(x), e))
```

Properties:

- function bodies are ordinary link-native expressions;
- function handles have no native handlers;
- composed functions may call other composed functions;
- definitions survive persistent reopen through explicit handles;
- effect accounting keeps existing link-native binding/call-frame materialization visible instead of hiding it in an ephemeral host-language stack;
- no new production relation identity or storage API merely for a reducible library operation.

## AVM 1.3 — execution observability and debugger boundary

Epic: #95.

### Gate 14 — deterministic non-controlling execution observer ✅

Implemented through #96/#97.

Public event model:

```text
Enter(ExecutionContext)
Return(ExecutionContext, result LinkId)
Fail(ExecutionContext)
```

Properties:

- canonical LinkId/context data only;
- no timestamps, host pointers, textual opcode names, JSON/Anum or backend data;
- observer receives no mutable `Executor`, `LinkStore`, context or result reference;
- observer exceptions cannot replace program success/failure;
- nested calls are observed through the same `Executor::execute` recursion;
- observer presence/absence does not change program result or store effects;
- pre-context failures emit no context event;
- installed package consumer validates the API on Linux/Windows/macOS;
- strict warnings, ASan+UBSan and portable matrix are green.

### Gate 15 — deterministic failure phase and unwind observation ✅

Implemented through #98/#99.

`Fail` carries a finite AVM-owned phase taxonomy:

```text
Dispatch
Handler
ResultValidation
```

Properties:

- phase identifies the canonical execution boundary that failed, not exception text/type;
- no `std::exception_ptr`, host exception object or backend/protocol diagnostic in `ExecutionEvent`;
- unknown relation, throwing handler and invalid returned LinkId are distinguishable;
- nested failure is observed through ordinary stack-shaped `Executor::execute` events;
- original exception type/message behavior remains unchanged;
- observer exceptions remain suppressed for every failure phase;
- repeated failure against unchanged state produces identical events;
- observation does not materialize links.

### Gate 16 — bounded reusable execution trace collector ✅

Implemented through #100/#101.

Public tooling contract:

```text
BoundedExecutionTrace(max_events)
  -> stores exact ExecutionEvent prefix
  -> complete when all events fit
  -> truncated when configured capacity is exceeded
```

Properties:

- capacity/storage reservation is established before normal attachment/execution;
- capacity exhaustion never fabricates events and is reported explicitly by `truncated()`;
- zero capacity is defined;
- `reset()` preserves configured capacity while clearing events/truncation;
- event access is immutable;
- collector owns no `Executor`, `LinkStore`, backend, parser/protocol or persistence target;
- collector attachment does not change program results or LinkStore effects;
- failure phases from Gate 15 pass through unchanged;
- installed public package uses the collector on Linux/Windows/macOS;
- strict warnings, ASan+UBSan and portable matrix are green.

### Gate 17 — persistent reopen and backend-neutral trace equivalence ✅

Implemented through #102/#103.

Two identity claims are intentionally distinct:

```text
same PersistentLinkStore after reopen
  -> exact ExecutionEvent equality, including numeric LinkIds

independent InMemory/Persistent stores
  -> equivalence modulo bijective renaming of opaque LinkIds
```

Properties:

- link-native function binding/frame state is converged before baseline capture;
- persistent close/reopen preserves exact success/failure traces and store size;
- repeated reopen remains exact and idempotent;
- backend-neutral comparison preserves equality/aliasing relationships across every LinkId-bearing field;
- deterministic first-occurrence normalization exists only as test/conformance tooling;
- raw LinkId numbers are never treated as globally comparable across independent stores;
- failure phases and event kinds remain exact under normalized comparison;
- truncated traces are rejected from completeness/equivalence claims;
- no new LinkStore API/index, persistence format, event field or semantic registry.

### Gate 18 — trace-enabled CLI consumer (current)

Child: #104.

Use the already existing JSON compatibility frontend and public AVM 1.3 observation API to provide a real tooling consumer without forking execution:

```text
JSON input
  -> existing JsonProgramImporter
  -> LinkId root
  -> attach BoundedExecutionTrace to the existing BootstrapRuntime/Executor
  -> execute(root)
  -> result LinkId
  -> existing JSON result projection
  -> tooling-only trace presentation
```

Requirements:

- legacy `avm program.json` behavior remains unchanged;
- explicit `--trace` mode prints ordered LinkId-based events;
- explicit bounded trace limit reports truncation rather than silently dropping events;
- function execution exposes frame-bearing events;
- failures render retained `Fail(phase)` events and preserve failure exit status/host diagnostics;
- non-expression JSON remains valid in normal value-roundtrip mode but is rejected in trace mode rather than receiving fabricated execution events;
- textual event/phase labels are presentation only, not opcodes or canonical trace identity;
- CLI uses the existing `JsonProgramImporter`, `BootstrapRuntime::execute` and JSON result projection rather than copied evaluator logic;
- stale hard-coded CLI version banner is replaced by the AVM 1.3 public version contract;
- CTest validates trace CLI behavior in the strict CLI lane and portable Linux/Windows/macOS builds.

Completion of this gate closes AVM 1.3 observability: observer, failure taxonomy, bounded collector, persistence/backend conformance and an actual tooling consumer all share the single canonical execution path.

## Later AVM 1.x directions

After the observability boundary:

1. interactive debugger/REPL control semantics, if needed, as a separate design problem;
2. additional thin protocol/front-end adapters;
3. packaging/integration tooling;
4. visualization/GUI;
5. production physical backends and persistence experiments;
6. standard-library growth by link-native composition, with new native primitives only for irreducible observation/effect boundaries.

Every extension must reuse the existing `LinkStore -> Relations Model -> Executor` architecture.

## Dependency rule

If a gate depends on an unfinished PR, independent preparation may proceed, but dependent code is not merged before the dependency is green. Legacy code is deleted after consumer migration; Git stores history.
