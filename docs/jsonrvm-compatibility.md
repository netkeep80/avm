# jsonRVM / AVM compatibility

Parent issue: #123. Epic: #122.

## Provenance

This audit is pinned to:

```text
repository: netkeep80/jsonRVM
commit:     843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime:    jsonRVM 3.0.0
```

The machine-readable companion is `compat/jsonrvm-semantics.json`.

The goal is not source compatibility with the old C++/JSON interpreter. The goal is to identify which observable Relations Model semantics must survive in AVM, which old syntax belongs in a projection frontend, which operations are explicit host effects, and which details must be dropped as implementation artifacts.

## Relations Model entity

`jsonRVM` represents a triune entity with three roles:

```text
(relation, subject, object)
```

The JSON projection uses:

```json
{
  "$rel": "relation",
  "$sub": "subject",
  "$obj": "object"
}
```

The conceptual model additionally treats relation as controller, subject as view/receiver and object as model/input. That execution meaning is richer than structural encoding alone and is the subject of #124.

## Canonical dyadic projection

AVM uses nested ordered pairs in one fixed order:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

Therefore:

```text
(relation, subject, object)
=
(relation, (subject, object))
```

No separate physical triplet record is required by the AVM core.

For a materialized triplet `t`:

```text
encode(t) -> entity LinkId
decode(entity LinkId) -> t
```

preserves all three roles exactly. Because both inner and outer pairs are canonical links, repeated encoding returns the same identities inside one logical store.

`find_relation_entity(relation, subject, object)` is intentionally different from encode/materialize: it finds the inner `(subject, object)` pair first and then the outer pair, without creating either on a miss.

This is the same boundary later required by Anum:

```text
description/query != realization/write
```

## Central finding of the semantic audit

The historical blocker was **not** the mathematical encoding of a triplet as dyads. That part is straightforward and already works in AVM.

The hard part is that `jsonRVM` makes `nlohmann::json` serve several roles at once:

1. program syntax;
2. runtime scalar/collection values;
3. mutable execution context;
4. the in-memory Relations Model/database view.

As a result, execution semantics are entangled with JSON references, mutable containers, host concurrency and external modules. A dyad store alone does not solve that problem.

AVM 1.0–1.4 has already removed the worst coupling by making `LinkId` the execution identity and by storing programs, functions and frames as links. AVM 1.5 therefore focuses on **semantic migration**, not another storage rewrite.

## Classification rules

Every audited behavior receives one of five categories in `compat/jsonrvm-semantics.json`.

### `canonical-semantic`

The behavior belongs to Relations Model / VM meaning and should survive independently of surface syntax. Examples: triune roles, parent execution contexts, ordered sequence execution and pure arithmetic semantics.

### `projection-syntax`

The old form is useful source syntax, but AVM must compile/project it to canonical links before execution. Examples: `$ent`, `$$obj`, JSON `$ref`, named/path addressing and markup-oriented surface structures.

### `effect-adapter`

The behavior touches external process state and needs an explicit capability/effect boundary. Examples: filesystem, HTTP, clock/sleep, stdout, dynamic plugins and lazy external entity retrieval.

### `implementation-artifact`

The behavior exists because `jsonRVM` uses mutable JSON as its runtime. It must not become AVM semantics. Examples: JSON type tags, container mutexes and literal `operator[]`-driven member creation.

### `defer`

The idea can be valuable but is unsafe to port before lower-level contracts exist. The main current example is automatic parallel execution of object-like projections: AVM first needs purity/effect ordering.

## What jsonRVM actually executes

### Triune execution context

`vm_ctx` carries references to:

```text
ent
rel
sub
obj
$
```

where `$` is a parent/outer execution context. The old runtime frequently mutates `$.sub` or `$.rel`, and some relations execute nested entities in `$.$` or `$.$.$` contexts.

Current AVM structurally decodes all three entity roles, but most bootstrap expressions intentionally use `subject = unit`. That is a good minimal kernel, not yet a full replacement for the original triune execution meaning. This is why #124 comes before broad vocabulary migration.

### Context pronouns and parent traversal

`jsonRVM::string_ref_to` recognizes `$ent/$rel/$sub/$obj` and hard-coded `$$`, `$$$`, `$$$$` variants. The `relative_addressing.json` fixture exercises all four roles across multiple parent depths.

The **meaning** is worth preserving. The textual representation and hard-coded four-level depth are not. AVM should model parent traversal compositionally and let a frontend compile `$...` syntax into canonical reference structures (#125/#126).

### Named and absolute references

The `absolute_addressing.json` fixture demonstrates named entities, nested paths and `add_entity`. The old resolver can also call `database_api::get_entity` when a named entity is absent.

That one code path currently conflates:

- pure lookup;
- syntax/path parsing;
- external lazy retrieval;
- lvalue creation/mutation.

AVM must split those concerns:

```text
parse/project reference
 -> non-mutating canonical resolve/find
 -> optional explicit external lookup effect
 -> optional explicit realization/write
```

A read miss must never create links.

### Sequence, projection and foreach

The old dictionary describes executable arrays as lambda-vectors evaluated in order. `base.rm.h` also implements `foreachobj` and `foreachsub` by creating child contexts for collection elements.

This behavior belongs to canonical semantics, but JSON arrays do not. AVM already has canonical link lists and `sequence`; #127 extends that foundation to deterministic projection and foreach semantics.

### Parallel object execution

The old implementation/project analysis describes automatic parallel execution for object-like projections using C++ execution policies and mutex-protected mutable JSON containers.

That scheduler behavior is deliberately **not** copied now. Parallelism changes observable results when operations materialize links or perform effects. AVM first classifies operations as pure, observational, materializing or externally effectful; parallel scheduling can be added only where order independence is explicit.

### Pure value vocabulary

`import_relations_model_to` registers a broad base vocabulary:

- conversions: `int`, `integer`, `float`, `double`, `null`;
- data operations: `where`, `union`, `size`, `get`, `set`, `erase`, `sequence/integer`;
- arithmetic: `*`, `:`, `+`, `-`, `pow`, `sqrt`, `sum`;
- logic/comparison: `^`, `==`, `!=`, `<`, `>`, `&&`;
- strings: conversion, append, find, split, join;
- control: foreach, if variants, then/else, while, typed switch, throw/catch;
- rendering: print, tag, XML, HTML;
- time: sleep and steady-clock values.

This vocabulary cannot be ported operation-by-operation before AVM defines canonical value denotations. `nlohmann::json` coercion and type behavior is not automatically normative. #128 owns that boundary.

### External vocabulary

Filesystem, HTTP and dynamic dictionary loading are separate modules. They are useful capabilities, not evidence that core execution should call host APIs implicitly. #129 introduces explicit capabilities and deterministic fake providers before any large effect-vocabulary migration.

## Initial compatibility matrix

| ID | Old behavior | Category | AVM now | Decision | Gate |
|---|---|---|---|---|---|
| RM-TRIUNE-001 | `ent/rel/sub/obj` roles | canonical | partial | preserve full triune contract | #124 |
| CTX-CURRENT-001 | current context roles | canonical | partial | canonical context access | #125 |
| CTX-PARENT-001 | `$` parent chain | canonical | partial | unbounded compositional traversal | #125 |
| REF-PRONOUN-001 | `$ent`, `$$obj`, ... | projection | missing | compile to reference links | #126 |
| REF-ABSOLUTE-001 | named/path addressing | projection | missing | canonical reference algebra | #126 |
| REF-LAZY-DB-001 | lazy DB retrieval | effect | missing | explicit lookup capability | #129 |
| REF-LVALUE-001 | create-on-write traversal | projection/write | missing | split resolve and write | #126 |
| EXEC-ARRAY-SEQ-001 | ordered executable arrays | canonical | partial | canonical link sequence | #127 |
| EXEC-OBJECT-PAR-001 | parallel object projection | defer | missing | wait for effect/purity model | #127 |
| EXEC-FOREACH-OBJ-001 | `foreachobj` | canonical | missing | deterministic link-list map | #127 |
| EXEC-FOREACH-SUB-001 | `foreachsub` | canonical | missing | after triune contract | #127 |
| CTRL-IF-001 | Boolean conditional | canonical | partial | differential parity baseline | #131 |
| CTRL-WHILE-001 | while | canonical | missing | after ordering contract | #127 |
| CTRL-SWITCH-001 | typed switch | canonical | missing | canonical keys/values | #128 |
| ERROR-THROW-CATCH-001 | program error handling | canonical | partial | deterministic semantic failures | #131 |
| VALUE-COPY-001 | copy/view | canonical | partial | identity + explicit projection | #124 |
| VALUE-ARITH-001 | arithmetic | canonical | missing | canonical numeric denotation first | #128 |
| VALUE-COMPARE-001 | compare/Boolean | canonical | partial | structural/value equality | #128 |
| VALUE-COLLECTION-001 | size/where/union | canonical | partial | canonical list semantics | #128 |
| VALUE-GETSET-001 | JSON get/set/erase | artifact | none | do not port literally | #126 |
| VALUE-STRING-001 | string operations | canonical | missing | canonical byte/text denotation | #128 |
| VALUE-TYPE-PRED-001 | JSON type predicates | artifact | n/a | only justified structural predicates | #128 |
| DISPLAY-PRINT-001 | stdout | effect | tooling only | explicit effect/frontend | #129 |
| DISPLAY-MARKUP-001 | tag/XML/HTML | projection | missing | after text/projection semantics | #128 |
| EFFECT-TIME-001 | clock/sleep | effect | missing | explicit clock capability | #129 |
| EFFECT-FS-001 | filesystem | effect | missing | explicit FS capability | #129 |
| EFFECT-HTTP-001 | HTTP | effect | missing | fake provider first | #129 |
| EFFECT-DLL-001 | native dictionary loading | effect | missing | plugin/capability boundary | #129 |
| IMPL-JSON-MUTEX-001 | JSON mutexes | artifact | n/a | drop | #127 |
| IMPL-JSON-AST-001 | JSON as code/data/context | artifact | removed | keep removed | #130 |

The JSON manifest is authoritative for tooling; this table is the human summary.

## Differential corpus: first selected cases

### CASE-RELATIVE-ADDRESSING

Source: `modules/console/test/relative_addressing.json`.

It exercises every context role and parent-depth form from `$...` through `$$$$...`. The old fixture is large, so migration should split it into smaller golden cases with explicit expected semantic identities.

Expected AVM property: resolving a context pronoun is observational and leaves `LinkStore::size()` unchanged.

### CASE-ABSOLUTE-ADDRESSING

Source: `modules/console/test/absolute_addressing.json`.

This fixture deliberately mixes pure named lookup and `add_entity` behavior. AVM must separate them instead of preserving the old coupling.

### CASE-BOOLEAN-BRANCH

AVM already has canonical Boolean values and link-native `if`, so this is the cheapest differential execution baseline once a frozen legacy input/output fixture is extracted.

### CASE-FOREACH-CONTEXT

A small old-style array/foreach program should prove child-context propagation before larger projection migration.

### CASE-ARITHMETIC

Freeze only simple integer cases after #128 chooses an integer denotation. Raw JSON numeric representation is not the equality criterion.

### CASE-MISSING-REFERENCE

Capture semantic failure category/context, but do not require byte-for-byte JSON exception formatting.

## Explicit non-goals of compatibility

AVM does **not** promise:

- the same numeric LinkIds in independent stores;
- the same internal JSON tree;
- the same C++ exception type/text;
- `nlohmann::json` coercion quirks;
- automatic JSON member creation on reads;
- a hard four-level context limit;
- implicit database/network/filesystem access;
- automatic parallel execution before an effect model exists.

## Relation to Anum / MTS serialization

`anum_docs` gives a useful boundary rule that AVM applies to both future frontends:

```text
raw(A) != den(A)
find(A) does not create den(A)
realize(A) is explicit
interpret(F) does not imply realize(F)
```

JSON text and Anum raw structure may be different, but after projection they must be able to denote the same canonical link structures. That is #130.

## Next implementation order

1. #124: make non-unit subject semantically meaningful without mutable host references.
2. #125: canonical current/parent execution contexts.
3. #126: reference algebra and non-mutating lookup.
4. #127/#128: projection/sequence and canonical values.
5. #129: explicit effects.
6. #130: frontend convergence.
7. #131: end-to-end differential program.

The purpose of this inventory is to keep those gates honest: every migrated feature points back to an observable legacy behavior, while every discarded behavior has an explicit reason instead of disappearing accidentally.
