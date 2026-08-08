# AVM 1.0 roadmap

## Принцип планирования

Текущий план — **Architecture Foundation 2.0 / AVM 1.0**. Работа идёт dependency-ordered gates: новый слой начинается только после того, как его зависимости имеют стабильный контракт и зелёные CI-gates.

Главный инвариант roadmap:

```text
external projection
  -> canonical LinkStore
  -> Relations Model codec
  -> LinkId program
  -> BootstrapRuntime / Executor
  -> result LinkId
```

Не допускаются второй semantic path, отдельное legacy-хранилище или JSON AST как альтернативное execution core.

## Gate 1 — архитектурный контракт ✅

- один physical primitive: directed dyad;
- opaque `LinkId` identity;
- canonical pair identity;
- read/query operations не материализуют данные;
- backend semantics отделены от VM semantics.

Основной документ: `docs/architecture.md`.

## Gate 2 — canonical LinkStore ✅

- reference `InMemoryLinkStore`;
- явное разделение `find` и `intern`;
- conformance coverage для canonical identity и query behavior;
- semantic code не зависит от pointer identity или layout backend-а.

## Gate 3 — Relations Model codec ✅

Единственное представление triplet:

```text
(relation, subject, object) = (relation, (subject, object))
```

Codec отделён от storage contract.

## Gate 4 — execution kernel ✅

- `Executor` принимает entity `LinkId`;
- relation dispatch выполняется по `LinkId`;
- execution context явный;
- bootstrap native handlers не являются второй program database.

Документ: `docs/execution-kernel.md`.

## Gate 5 — program-as-links ✅

- program structures представлены ссылками;
- bindings/call frames находятся в ассоциативной модели;
- link-native vertical slice покрывает наблюдаемую семантику;
- старый pointer-based `rel_t` semantic/storage universe удалён после миграции consumers.

## Gate 6 — external protocol boundary ✅

- parser/grammar/context принадлежат внешнему adapter-у;
- AVM core получает structural projection над `LinkId` anchors;
- `raw(A)` отделён от `den(A)`;
- `find` остаётся non-mutating, `realize` — явной материализацией.

Документ: `docs/protocol-adapter-contract.md`.

## Gate 7 — integration hardening (текущий)

### 7.1 Persistent LinkStore contract

Persistent implementation должна быть взаимозаменяемой с in-memory backend по observable semantics. Reopen/crash guarantees и backend-specific behavior проверяются отдельно от VM semantics.

Документ: `docs/persistent-link-store.md`.

### 7.2 End-to-end vertical slice

Поддерживать короткий воспроизводимый путь от projection до result `LinkId`, не добавляя обходных execution routes.

### 7.3 Performance baseline

Benchmark gates защищают от регрессий в canonical link operations и link-native execution. Производительность не может оправдывать нарушение identity/query invariants.

### 7.4 Documentation alignment

README, analysis и roadmap должны описывать только фактическую link-native архитектуру. Hard-coded test counts и исторические target claims не используются.

### 7.5 CI/quality hardening

Продолжать усиливать:

- Linux/Windows/macOS builds;
- warnings-as-errors;
- sanitizers;
- architecture quality guards;
- benchmark regression gates;
- persistent backend conformance.

## Gate 8 — AVM 1.0 release readiness

После завершения integration hardening:

- один документированный public execution path;
- стабильные core contracts;
- persistent backend с conformance/reopen coverage;
- воспроизводимый vertical slice;
- отсутствие legacy semantic paths;
- release/versioning policy и минимальный public API.

## После AVM 1.0

Только после foundation gates приоритет получают feature-направления:

1. расширение link-native стандартной библиотеки;
2. дополнительные protocol/front-end adapters;
3. query/indexing facilities поверх canonical store contracts;
4. debugger/REPL и observability;
5. packaging и integration tooling;
6. visualization/GUI;
7. distributed/persistent backend experiments.

Каждое расширение обязано использовать существующий `LinkStore -> Relations Model -> Executor` path и не создавать альтернативное runtime ядро.

## Dependency rule

Если gate зависит от незавершённого PR, можно готовить следующий **независимый** шаг, но нельзя мержить зависимую работу раньше зелёного dependency-gate. Legacy-код после миграции consumers удаляется; Git является хранилищем истории.
