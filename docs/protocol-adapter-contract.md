# Контракт адаптера внешнего протокола

AVM намеренно **не** определяет интерфейс Anum parser. Стабильная граница — композиция нейтральных контрактов AVM:

```text
опциональный raw source -> RawCarrier
external parse / validate / quote / project(context)
                         -> ProjectionDescription
                         -> find_projection | realize_projection
                         -> денотация в LinkStore
```

Реализация протокола остаётся вне VM. Это может быть Anum/МТС, JSON, бинарный формат, заранее построенный AST или сгенерированная структура. AVM получает только непрозрачные raw bytes, если caller хочет их сохранить, и завершённую structural projection, когда требуется найти или материализовать денотацию.

## Ответственность внешнего adapter

Adapter владеет всей protocol/model semantics до границы AVM:

- grammar и tokenization источника;
- validation;
- quotation/unquotation и description-level semantics;
- projection context `K`;
- resolution внешних symbols/identities в существующие `Anchor(LinkId)`;
- построение топологически корректного `ProjectionDescription`.

Контекст передаётся adapter явно. `LinkStore` его не угадывает, а `ProjectionDescription` не занимается name lookup.

## Ответственность AVM

AVM владеет только L4-facing механикой:

- опциональным хранением opaque raw через `RawCarrier`;
- typed structural description из `Anchor` и projection-local `Node`;
- read-only `find_projection(const LinkStore&, ...)`;
- явным `realize_projection(LinkStore&, ...)`;
- canonical dyad identity через `LinkStore::intern`.

Ни загрузка raw, ни поиск projection не создают денотацию.

## Почему нет обязательного base class adapter

В core намеренно отсутствует `AnumParser`, виртуальный `ProtocolAdapter` или обязательный template concept. Такая абстракция навязала бы внешним протоколам source/AST/context types, не улучшая memory contract.

Replaceability является структурной: любой компонент, способный построить валидный `ProjectionDescription` над разрешёнными anchors, может работать с AVM.

Conformance tests доказывают это двумя независимыми toy adapters: один получает opaque binary bytes через `RawCarrier`, другой — уже разобранный toy AST; оба строят одну structural denotation и после materialization находят те же canonical `LinkId`.

## Контекст явно влияет на денотацию

Одинаковое structural rule при разных resolved anchors/context может дать разную денотацию. Это ожидаемое поведение. Store не выбирает контекст и не создаёт missing anchors скрыто.

## Соответствие `anum_docs`

Интеграционная точка следует разделению, принятому в `netkeep80/anum_docs`:

```text
Anum L3
  raw syntax / parser / protocol / quote / context projection
       |
       v
нейтральная граница AVM
  RawCarrier (опционально) + ProjectionDescription
       |
       v
AVM L4
  find_projection / realize_projection / LinkStore
```

Ключевые инварианты:

- `raw(A)` может существовать без `den(A)`;
- загрузка raw не реализует denotation;
- `find(A)` наблюдающий и non-mutating;
- `realize(A)` — явная materialization operation;
- context и protocol semantics остаются вне memory layer.

Этот контракт не претендует на определение canonical quotation МТС, грамматики Anum или окончательной политики external-symbol identity. Это ответственность канонической L3 implementation.

## Следствие для старого issue #3

Старый запрос на сериализацию/десериализацию Anum непосредственно внутри AVM не должен возвращать parser в VM. Будущая production integration должна реализовать adapter к canonical API `anum_docs` L3 и передавать результат через эту границу. После появления такого adapter issue #3 следует закрыть как superseded либо сузить до конкретного integration package.
