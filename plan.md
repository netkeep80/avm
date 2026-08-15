# План развития AVM

Этот файл — **status/roadmap overview**, а не замена архитектурным контрактам. Нормативные границы подсистем находятся в `docs/`; карта документации — [docs/README.md](docs/README.md).

## Правило планирования

Работа ведётся последовательными evidence-backed gates с явными зависимостями. Новый semantic слой начинается только после того, как нижележащие contracts доказаны тестами и полным CI.

Неизменный runtime path:

```text
внешняя проекция / semantic adapter
 -> canonical denotation / ProjectionDescription
 -> find | explicit realize
 -> LinkStore
 -> программа как LinkId
 -> BootstrapRuntime / Executor
 -> LinkId / ExecutionOutcome
```

Запрещены второй semantic path, скрытая база программ, compatibility Executor, legacy storage universe и backend-specific semantic index.

## Завершённые этапы

### AVM 1.0 — архитектурный фундамент ✅

Доказаны:

- `LinkId -> (begin,end)` как единый физический primitive;
- canonical pair identity;
- `find` как наблюдение и `intern/realize` как явная materialization;
- `(relation,subject,object) = Link(relation,Link(subject,object))`;
- один link-native `Executor`;
- programs/functions/bindings/call frames как links;
- parser-independent projection boundary;
- structural Anum L3→L4 adapter;
- `PersistentLinkStore` и reopen identity;
- удаление pointer-based `rel_t`, JSON semantic interpreter и protocol-only Anum bridge;
- portable/package-consumer/warnings-as-errors/ASan+UBSan/benchmark gates.

### AVM 1.1 — read-only Relations queries без материализации ✅

`RelationQuery` использует только существующие `find/outgoing/incoming/get/contains`, не materialize-ит данные и не создаёт отдельный semantic index.

### AVM 1.2 — структурная standard library ✅

Приняты link-native primitives:

```text
link_begin
link_end
identity_equal
link_exists
pair_intern
```

Derived behavior по возможности выражается обычными AVM functions, а не новыми native handlers.

### AVM 1.3 — наблюдаемость execution ✅

Принят deterministic read-only observer contract:

```text
Enter(ExecutionContext)
Return(ExecutionContext,result)
Fail(ExecutionContext,phase)
```

`BoundedExecutionTrace` не является VM state и не управляет исполнением.

### AVM 1.4 — инструменты inspection ✅

`InspectionSession`, persistent inspection и scripted `avm-inspect` используют существующие canonical APIs и один `Executor`. Textual commands остаются presentation layer.

Русский язык закреплён как нормативный для project-owned документации.

### AVM 1.5 — доказанный перенос Relations Model semantics ✅

Epic #122 завершён как ограниченный доказанный subset, а не как бесконечный operator-porting backlog.

Pinned historical oracle:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Завершённые semantic gates:

- #123 — semantic inventory и frozen corpus;
- #124 — triune execution contract;
- #125 — immutable semantic context;
- #126 — current/parent reference algebra и frontend compiler boundary;
- #127 — ordered sequence/projection/foreach semantics;
- #128 — canonical Boolean/Integer/Text/list denotation;
- #130 — JSON ↔ Anum common denotation convergence;
- #131/#213 — persistent release proof без remigration/reprojection после reopen.

Доказанный pure corpus:

```text
CASE-ARITHMETIC                  -> 2
CASE-SEQUENCE-ORDER              -> 3
CASE-PURE-RELATION-COMPOSITION   -> 5
CASE-FOREACH-CONTEXT             -> [1,2,3]
CASE-BOOLEAN-BRANCH              -> 42
CASE-MISSING-REFERENCE           -> typed source failure
```

Ключевые invariants:

```text
ExecutionContext.relation != semantic relation_state
result LinkId != implicit relation_state := result
find/resolve/query != realize/write/effect
textual frontend name != canonical LinkId
```

После canonical realization source syntax не является runtime dependency AVM.

См. [доказательства AVM 1.5](docs/avm-1.5-release-proof.md).

## Capability/effect boundary завершён ✅

Gate #129 завершён после pure AVM 1.5 release proof.

Первый evidence-driven slice выбран по frozen semantic inventory: `REF-LAZY-DB-001`, historical lazy external entity retrieval.

Доказанный contract:

```text
canonical effect RelationEntity
 -> ordinary Executor
 -> explicit capability policy
 -> explicit provider
 -> existing LinkId | deterministic failure
```

Принципиальные свойства:

- program structure не выдаёт authority;
- capability policy задаётся host/session явно;
- denied capability не вызывает provider;
- pure programs работают без provider;
- provider не materialize-ит arbitrary graph;
- foreign `LinkId` отклоняется;
- effect request/success/failure наблюдаются через существующий read-only observer contract;
- generic `Executor` остаётся effect-neutral.

Этот gate доказывает **архитектуру effects**, но не объявляет готовыми реальные filesystem/HTTP/clock/native adapters.

См. [effect capability contract](docs/effect-capabilities.md).

## Текущее состояние

После #129 закрыт весь ранее заведённый dependency-ordered backlog AVM 1.0–1.5. Новые задачи должны появляться из реальных consumer requirements, новых frozen semantic evidence или доказанной необходимости расширить нижележащий contract.

Не следует создавать «следующую версию» только ради номера.

## Направления дальнейшего развития

При наличии реального use-case наиболее естественные направления:

1. дополнительные host-effect adapters поверх уже доказанного capability contract — отдельно для FS, HTTP, clock/time, native plugins или storage services;
2. дополнительные Relations Model constructs только через evidence-backed migration;
3. interactive debugger, если потребуется control contract поверх существующей read-only inspection boundary;
4. новые frontends поверх общего `ProjectionDescription -> find | realize` contract;
5. production persistence backends с отдельными crash-consistency/WAL/concurrency guarantees;
6. visualization/GUI как consumer существующих inspection APIs;
7. scheduler/parallel execution только после явного purity/effect ordering proof;
8. distributed execution/storage;
9. JIT/native compilation;
10. расширение standard library преимущественно через link-native composition.

## Как выбирать следующую задачу

Приоритет задаётся не размером feature, а силой evidence:

```text
real consumer problem
 -> exact observable requirement
 -> существующий lower-level contract?
 -> минимальный vertical slice
 -> conformance + failure cases
 -> full CI
 -> только затем расширение surface
```

Если behavior можно выразить существующими links/functions, новый native handler не добавляется без причины. Если операция читает host state, она не маскируется под pure resolve. Если исторический jsonRVM behavior не подтверждён frozen evidence, он не считается обязательной semantics AVM.

## Правило зависимостей

Dependent code не merge-ится раньше dependency gate. После доказанной миграции legacy implementation удаляется, а не сохраняется вторым production path. Историю хранит Git.
