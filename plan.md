# План развития AVM

## 1. Принцип приоритизации

Текущий приоритет AVM — завершить архитектурный фундамент виртуальной машины, а не расширять количество прикладных функций.

Veto-инварианты:

```text
один canonical LinkStore identity universe
storage != semantics
JSON != VM
program == link data
find/query не мутирует
realize/write явный
raw != denotation
backend не определяет Executor semantics
```

Если новая возможность нарушает один из этих инвариантов, она не должна приниматься даже при локально работающем результате.

## 2. AVM 1.0 Architecture Foundation

Главный epic: #31. Финальный integration gate: #27.

### Выполненные этапы

1. **#21 — Architecture contract.** Зафиксированы слои, canonical dyad и mutability rules.
2. **#22 — Canonical LinkStore.** Введены `LinkId`, `Link`, exact/outgoing/incoming indexes и non-mutating `find`.
3. **#23 — Relations Model codec.** `(rel,sub,obj)` кодируется как `Link(rel, Link(sub,obj))`.
4. **#24 — Execution kernel.** Executor получает `LinkId` и dispatch выполняется по relation identity.
5. **#25 — Program is data.** Boolean logic, `If`, functions, bindings, frames и recursion перенесены в links; старый JSON semantic interpreter удалён.
6. **#26 — Anum/MTS boundary.** Raw carrier, neutral projection, `find_projection` и `realize_projection`; parser/grammar остаются снаружи AVM.
7. **#59 — Persistent reference backend.** Versioned snapshot, stable `LinkId` после reopen, восстановление индексов и corruption checks.

### Оставшиеся gates #27

#### #60 — Persistent execution vertical slice

Доказать один сквозной path:

```text
JSON projection
 -> materialized executable links
 -> root LinkId
 -> Executor
 -> result LinkId
 -> close/reopen persistent store
 -> execute saved root LinkId again without JSON re-import
```

Проверяются Boolean operations, lazy `If`, `Def/Call` и representative recursion на общем `LinkStore` contract.

#### #61 — Performance baseline

Добавить воспроизводимый benchmark для:

- new/existing `intern`;
- exact find hit/miss;
- outgoing/incoming;
- Relations Model encode/decode;
- minimal execution;
- persistent reopen/index rebuild.

CI должен сохранять результаты как artifact, но shared runner wall-clock не является correctness veto.

#### #67 — Актуализация документации

README, `analysis.md` и этот plan должны описывать фактический link-native runtime, а не удалённый JSON interpreter и `src/main.cpp`.

#### #68 — Удаление второго `rel_t` storage path

Исторический `rel_t` сейчас используется только data-codec/compatibility consumers, но поддерживает отдельную pointer-based identity universe. Нужно:

1. сохранить conformance JSON roundtrip;
2. реализовать нужный data projection поверх canonical LinkStore;
3. мигрировать CLI/tests;
4. удалить `rel_t::db`, legacy `eval`, static pointer vocabulary и ненужный third-party linkage;
5. добавить CI guard от возврата второго storage path.

Только после этого #27 и epic #31 можно считать архитектурно завершёнными.

## 3. Следующий этап после AVM 1.0 foundation

После закрытия #27 приоритеты должны определяться через отдельные issues и измеряемые use cases.

### 3.1. Production backend adapters

Reference persistence уже задаёт contract. Следующие кандидаты:

- LinksPlatform adapter;
- PMM adapter после стабилизации соответствующего persistent memory API.

Требование: никакой allocator/backend-specific semantics в Executor.

### 3.2. Canonical Anum/MTS integration

Issue #3 не должен приводить к созданию второго Anum parser внутри AVM. Интеграция должна идти через уже зафиксированную границу:

```text
canonical external Anum implementation
 -> project(context)
 -> ProjectionDescription
 -> find / realize
 -> LinkStore
```

Quotation/context semantics определяет canonical protocol implementation, а не storage layer AVM.

### 3.3. Public library API

После стабилизации core можно выделять поддерживаемый API для:

- управления store lifecycle;
- projection/import/export;
- execute by `LinkId`;
- query/find;
- explicit realization.

Публичный API не должен закреплять historical `rel_t` types.

### 3.4. Persistence hardening

Для production backend отдельно решаются:

- crash-atomic commit;
- WAL/journal;
- concurrency model;
- transactions;
- recovery;
- mmap/IO strategy;
- stable root/bootstrap discovery.

Reference snapshot backend не следует постепенно превращать в production database случайными локальными патчами.

## 4. Возможные последующие направления

Эти направления остаются потенциально полезными, но не являются foundation blockers.

### Runtime и язык

- расширение bootstrap/stdlib;
- richer query model;
- debugging/introspection;
- tracing execution contexts;
- native/JIT compilation только после измерений.

### Инструменты

- REPL;
- visualization;
- examples/tutorials;
- API documentation;
- package managers;
- static analysis.

### Storage и distributed systems

- production PMM/LinksPlatform backends;
- transactions/versioning;
- replication/distribution.

### Knowledge/modeling applications

- knowledge bases;
- semantic search;
- inference experiments;
- simulation/modeling;
- educational tooling.

Каждое такое направление должно строиться поверх завершённого core, а не менять его фундаментальные identity/mutability semantics под конкретное приложение.

## 5. CI/CD как часть архитектуры

Обязательные проверки foundation:

- formatting/source guards;
- запрет удалённых semantic side-channels;
- запрет Anum parser coupling в production core;
- C++20 warnings-as-errors;
- core-only tests;
- independent JSON projection tests;
- compatibility facade regression tests, пока facade существует;
- ASan + UBSan;
- portable Linux/Windows/macOS;
- benchmark smoke/artifacts после #61.

Tests являются доказательством инвариантов, а не статистикой. Поэтому документация не хранит вручную число unit tests.

## 6. Критерий завершения AVM 1.0 foundation

Foundation считается завершённым, когда одновременно выполнено:

- один canonical LinkStore production storage path;
- executable program реально хранится в links;
- Executor работает по `LinkId`;
- Relations Model triplet roundtrip доказан;
- Boolean/If/functions/recursion проходят link-native tests;
- persistent reopen сохраняет identity и позволяет повторно исполнять materialized program;
- raw/denotation и Anum protocol boundaries явны;
- старый `rel_t` pointer storage удалён после migration consumers;
- документация соответствует коду;
- CI portable/sanitizer gates зелёные;
- есть performance baseline без необоснованных performance claims.

После этого развитие прикладных возможностей перестаёт конкурировать с незавершённым фундаментом и может идти отдельными вертикальными slices.
