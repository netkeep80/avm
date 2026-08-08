# Контракт запросов Модели Отношений

## Статус

Этот документ определяет публичный read-only query layer AVM 1.1 над каноническими сущностями Модели Отношений.

Физический и семантический фундамент неизменен:

```text
LinkId -> (begin, end)
(relation, subject, object) = (relation, (subject, object))
```

Запросы — производное read-only представление над существующим контрактом `LinkStore`. Они не вводят вторую database, отдельный universe индексов, entity registry или execution path.

## Публичный API

```cpp
#include <avm/relations_query.h>

avm::RelationQuery query{
    .relation = relation_or_nullopt,
    .subject = subject_or_nullopt,
    .object = object_or_nullopt,
};

std::vector<avm::RelationMatch> matches =
    avm::query_relation_entities(store, query);
```

`RelationMatch` содержит:

```text
entity_id  канонический LinkId внешней relation-link
entity     decoded RelationEntity {relation, subject, object}
```

Хотя бы одно поле должно быть ограничено. All-wildcard query запрещён, потому что `LinkStore` намеренно не предоставляет contract полного enumeration store.

## Стратегии на существующих индексах

Query layer использует только индексы, уже выраженные публичным `LinkStore` API.

### Ограничено relation

```text
outgoing(relation)
-> outer candidate links
-> decode + filter остальных полей
```

### Ограничены subject и object, relation wildcard

```text
find(subject, object)
-> canonical subject/object pair, если она существует
-> incoming(pair)
-> decode + filter
```

Запрос никогда не вызывает `intern(subject, object)`. Missing pair даёт пустой результат без mutation.

### Ограничен subject

```text
outgoing(subject)
-> candidate pair links
-> incoming(pair)
-> outer candidate links
-> decode + filter
```

### Ограничен object

```text
incoming(object)
-> candidate pair links
-> incoming(pair)
-> outer candidate links
-> decode + filter
```

Backend-specific containers этому слою не видны.

## Структурная тотальность

AVM не поддерживает hidden registry связей, ранее созданных через `encode_relation_entity`.

`decode_relation_entity(store, id)` структурен: для любой существующей outer link её `end` сам является существующей link и поэтому задаёт пару subject/object. Следовательно point:

```text
x = (x, x)
```

структурно также представляет:

```text
relation = x
subject  = x
object   = x
```

Промежуточные pair links также могут удовлетворять более широкому structural query. Это намеренно. External registry вида «этот LinkId является entity» создал бы второй semantic identity universe и противоречил бы универсальности представления.

Если application нужен более узкий domain, его следует выражать дополнительными relations/constraints в самой асети, а не hidden C++ classification state.

## Семантика результата

Результаты:

- observational: размер store и canonical identities не меняются;
- deterministic: сортируются по возрастанию `entity_id`;
- defensively deduplicated по `entity_id`;
- structurally verified: каждая `entity` совпадает с `decode_relation_entity(store, entity_id)`.

Unknown constrained `LinkId` дают пустой результат. Query не materialize-ит missing links.

## Эквивалентность backends

Один и тот же fixture обязан давать эквивалентный результат для:

```text
InMemoryLinkStore
PersistentLinkStore до close
PersistentLinkStore после reopen
```

Query semantics остаётся выше backend boundary.

## Явные non-goals

AVM 1.1 query v1 не добавляет:

```text
full-store enumeration
SQL/query-language parsing
planner statistics
secondary persistent indexes
hidden entity registries
mutation through queries
JSON или Anum query semantics
```

Будущий индекс сначала должен быть оправдан измеряемой потребностью относительно benchmark baseline, а затем явно расширить `LinkStore` contract, а не читать backend internals.

## Проверки качества

CI не позволяет production `relations_query.h` вводить:

- `intern()` или `create_point()`;
- guessed enumeration через `store.size()`;
- прямую зависимость от `InMemoryLinkStore` или `PersistentLinkStore`;
- JSON/Anum dependencies.

Benchmark baseline покрывает relation-, subject-, object- и exact subject+object-driven queries.
