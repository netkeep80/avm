# Анализ проекта AVM

## Текущее архитектурное состояние

AVM завершил Architecture Foundation / AVM 1.0 и перешёл к развитию совместимых 1.x facilities. Ядро строится вокруг одного канонического пространства ссылок и исполняет программы по `LinkId`:

```text
external representation
  -> projection / protocol adapter
  -> canonical LinkStore
  -> Relations Model codec
  -> LinkId program
  -> BootstrapRuntime / Executor
  -> result LinkId
```

Это единственный semantic path. Удалённые `rel_t`, JSON-interpreter и protocol-only Anum bridge не являются compatibility-ядрами и не должны возвращаться.

## Foundation, который уже закрыт

### Link model и LinkStore

Физический примитив — направленная диада `LinkId -> (begin,end)`. `LinkId` непрозрачен. `find/get/outgoing/incoming` наблюдают существующую структуру; `intern/create_point` являются явными mutation operations.

`InMemoryLinkStore` — reference backend. `PersistentLinkStore` доказал reopen/identity semantics и индексный rebuild; одинаковые observable contracts проверяются в CI.

### Relations Model

Исполнимая сущность кодируется единственным порядком вложения:

```text
(relation, subject, object) = (relation, (subject, object))
```

Никакого внешнего type registry для «entity links» нет: принадлежность Relations Model определяется структурой ссылок.

### Execution

`BootstrapRuntime` создаёт bootstrap vocabulary identity. `Executor` принимает entity `LinkId`, декодирует Relations Model entity и dispatch-ит по relation `LinkId`.

Program structures, bindings и call frames представлены ссылками, а не JSON AST или скрытым C++ environment.

### Projection boundary

JSON, Anum и другие форматы находятся вне execution kernel. Конкретный внешний протокол владеет grammar/parsing/context semantics и передаёт AVM только structural projection/denotation.

Ключевая граница:

```text
raw(A) != den(A)
load(A) does not imply den(A)
find(A) is observational
realize(A) explicitly materializes denotation
```

Canonical Anum semantics находятся в `netkeep80/anum_docs`; AVM structural bridge не содержит Anum parser или recursive grammar.

### Release/package validation

AVM 1.0 public package прошёл installed-consumer и portable validation на Linux, Windows и macOS, а также strict warnings, sanitizers и benchmark gates. Поэтому package portability больше не является незакрытым foundation-risk.

## AVM 1.1: read-only associative query layer

Первое post-1.0 направление — запросы поверх уже существующей Relations Model структуры.

Новая граница:

```text
RelationQuery(relation?, subject?, object?)
-> existing LinkStore indexes
-> decode/filter
-> deterministic RelationMatch[]
```

Запросы принципиально не создают отдельный индексный universe. Используются только `find`, `outgoing`, `incoming`, `get`, `contains`.

### Почему нет hidden entity registry

`decode_relation_entity` структурен. Если существует point:

```text
x = (x,x)
```

то он также структурно читается как:

```text
(x, x, x)
```

Промежуточный `(subject,object)` link тоже может одновременно удовлетворять более широкому Relations Model query. Это следствие универсальности Link primitive, а не ошибка типов.

Если поверх этого создать C++-набор «только те LinkId, которые когда-то вернул encode_relation_entity», возникнет второе понятие identity/classification, которого нет в Relations Model. Поэтому AVM 1.1 queries остаются структурными. Более узкие домены должны выражаться дополнительными relations/constraints в самой асети.

### Query strategies

Без нового storage API доступны четыре индексных пути:

```text
relation constrained
  -> outgoing(relation)

subject + object constrained
  -> find(subject,object)
  -> incoming(pair)

subject constrained
  -> outgoing(subject)
  -> incoming(pair)

object constrained
  -> incoming(object)
  -> incoming(pair)
```

Все candidates затем структурно декодируются и фильтруются. Missing pair не материализуется.

All-wildcard query пока запрещён: `LinkStore` не имеет public enumeration contract. Сканирование guessed диапазона `1..size()` было бы нарушением opaque LinkId semantics.

## Сильные стороны текущей архитектуры

1. **Один physical/semantic core.** Нет конкурирующих identity universes.
2. **Canonical link identity.** Семантика не зависит от адресов C++ объектов.
3. **Read/write separation.** Queries и projection-find не материализуют данные.
4. **External protocol boundary.** JSON/Anum не проникают в executor.
5. **Backend replaceability.** In-memory/persistent проверяются одним observable contract.
6. **Structural Relations Model.** Entity semantics не требуют side registry.
7. **CI as architecture gate.** Cross-platform builds, package consumer, sanitizers, static guards and benchmarks защищают фундамент.

## Текущие риски AVM 1.1

### 1. Query cost growth

Subject/object queries могут проходить через fan-out существующих `outgoing/incoming` indexes. До добавления новых physical indexes нужна измеряемая проблема, подтверждённая benchmark/workload.

### 2. Enumeration contract

Полностью unconstrained queries нельзя корректно реализовать через `size()` и предположение о contiguous IDs как semantic API. Если enumeration понадобится, её следует добавить отдельным explicit `LinkStore` contract и проверить на всех backends.

### 3. Bootstrap/native boundary

Native handlers остаются допустимым bootstrap mechanism, но registry не должен становиться вторым program store.

### 4. Protocol integrations

Новые adapters должны оставаться thin projection layers. String dispatch, external AST или protocol-specific query semantics не должны создавать второй runtime path.

## Что больше не является актуальной проблемой

- `src/main.cpp` как semantic core;
- singleton/global `rel_t` storage;
- pointer identity;
- JSON expression interpreter как VM execution model;
- LinksPlatform как обязательная semantic dependency;
- protocol-value-only Anum bridge;
- portable installed-package validation как незавершённый AVM 1.0 gate.

## Приоритет

Текущий приоритет — #83/#84: доказать полезный read-only Relations Model query layer на существующих индексах, получить backend/reopen conformance и performance baselines. Только после измерений решать, нужен ли новый явный LinkStore index/enumeration contract.

## Основная документация

- `docs/architecture.md` — базовый архитектурный контракт;
- `docs/execution-kernel.md` — execution kernel;
- `docs/protocol-adapter-contract.md` — внешний protocol boundary;
- `docs/anum-l3-l4-bridge.md` — canonical Anum structural bridge;
- `docs/relations-query.md` — AVM 1.1 query contract;
- `docs/persistent-link-store.md` — persistent backend contract;
- `plan.md` — dependency-ordered roadmap.

## Вывод

AVM 1.0 сформировал устойчивый link-native фундамент. AVM 1.1 расширяет его не новым runtime, а наблюдаемыми facilities поверх тех же canonical links. Критерий дальнейшего развития остаётся прежним: новые возможности должны сохранять единый путь `LinkStore -> Relations Model -> Executor` и не вводить скрытые identity/storage semantics.
