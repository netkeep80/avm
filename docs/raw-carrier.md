# RawCarrier: `raw(A)` не является `den(A)`

`RawCarrier` хранит непрозрачный исходный носитель независимо от links, представляющих денотацию AVM.

Граница закрепляет инвариант:

```text
raw(A) может существовать, когда den(A) ещё не существует
```

и наоборот: материализованная денотация может продолжать существовать после удаления исходной записи raw carrier.

## Контракт

```text
RawDocumentId put(RawBytes)
optional<RawBytes> get(RawDocumentId) const
bool contains(RawDocumentId) const
bool erase(RawDocumentId)
size_t size() const
```

Интерфейс не принимает `LinkStore`, `ProjectionDescription`, executor или parser API. Поэтому загрузка raw физически не может случайно materialize связь.

`RawBytes` — бинарные данные, а не текст. Нулевые байты и произвольные значения допустимы. Интерпретация и character encoding принадлежат внешнему protocol adapter.

## Идентичность

`RawDocumentId` идентифицирует только запись носителя. Это не `LinkId`, и он не задаёт identity денотации.

Эталонный `InMemoryRawCarrier` сейчас выдаёт новый document ID на каждый `put`, даже для одинаковых bytes. Другой backend может выбрать иную deduplication policy. Эта storage policy не должна менять денотацию, получаемую внешним projector.

## Независимый жизненный цикл

Поддерживаемая последовательность намеренно разделена:

```text
put(raw)                 -> меняется только RawCarrier
project(raw, context)    -> внешняя операция; создаёт ProjectionDescription
find_projection(...)     -> только наблюдает LinkStore
realize_projection(...)  -> явно изменяет LinkStore
erase(raw)               -> меняется только RawCarrier
```

Удаление raw source никогда каскадно не изменяет `LinkStore`. Будущее удаление materialized denotation также должно быть отдельной явной операцией и не выводиться из lifetime raw carrier.

## Связь с Anum

Для будущего Anum adapter:

```text
RawCarrier
   |
   v
внешний Anum L3 parser / validator / projector(context)
   |
   v
ProjectionDescription
   |
   +--> find_projection(const LinkStore&, ...)
   `--> realize_projection(LinkStore&, ...)
```

AVM не знает, являются ли bytes Anum, JSON, бинарным protocol или чем-либо ещё. Это намеренно: parser/protocol semantics находятся выше L4 memory boundary.

## Эталонная реализация

`InMemoryRawCarrier` — только reference/test backend. Persistent raw storage, filesystem, PMM integration, hashes и retention policies относятся к backend work, а не к semantic memory contract.
