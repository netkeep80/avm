# Нативный дуплетный JSON-формат AVM

Родительская задача: #169. ADR-задача: #170. Связанные задачи: #130, #171–#175.

## Статус

Этот документ определяет нативный JSON frontend AVM версии `duplet-json/1`.

Формат принадлежит AVM. Он **не заменяет** и не изменяет JSON-язык `jsonRVM`: исторический `jsonRVM` сохраняет `$rel/$sub/$obj` и остаётся compatibility/oracle source.

Главный принцип:

```text
JSON-пара описывает ровно Link(begin, end)
```

Триединая сущность Модели Отношений является только частным структурным паттерном пары.

## Канонический порядок ролей

Модель Отношений использует:

```text
(rel, sub, obj)
```

Каноническое представление через вложенные дуплеты:

```text
(rel, sub, obj) = (rel, (sub, obj))
```

Следовательно:

```text
RelationEntity = Link(relation, Link(subject, object))
```

В старом `jsonRVM#10` исторически была записана перестановка `(rel,(obj,sub))`. Она не является compatibility-вариантом AVM и в native frontend не поддерживается.

## Базовая форма дуплета

Единственная структурная primitive-форма:

```json
{
  "<<": <begin>,
  ">>": <end>
}
```

где:

```text
<< = begin
>> = end
```

Объект пары должен содержать ровно два поля `<<` и `>>`.

Недопустимы:

```json
{"<<": 1}
```

```json
{">>": 2}
```

```json
{"<<": 1, ">>": 2, "extra": 3}
```

Parser обязан отвергнуть такие формы до любой materialization.

## Триединая сущность

RelationEntity записывается обычной вложенностью пар:

```json
{
  "<<": <relation>,
  ">>": {
    "<<": <subject>,
    ">>": <object>
  }
}
```

Это не отдельная JSON-конструкция и не opcode parser-а. Только Executor позднее интерпретирует полученный canonical `LinkId` как исполняемую RelationEntity.

## Почему используются `<<` и `>>`

Запись `[begin,end]` короче, но не выбрана нормативной, потому что:

1. `jsonRVM` исторически использует JSON arrays как исполняемые последовательности;
2. array визуально не показывает направление связи;
3. `<<` и `>>` позволяют глазами читать begin/end рекурсивной асети.

Эти ключи не имеют execution semantics.

## Документ и отдельный терм

### Версионированный файл

Файл Native JSON версии 1 имеет envelope:

```json
{
  "$avm": "duplet-json/1",
  "$root": <term>
}
```

`$avm` и `$root` относятся только к transport layer и не материализуются как связи.

Version marker обязателен для file/CLI format.

### API отдельного терма

Library adapter может принимать непосредственно `<term>`, если вызывающий код уже явно выбрал parser `duplet-json/1`.

Bare term не используется для эвристического определения формата файла.

## Терм и лист

Концептуальная grammar:

```text
Document := {
  "$avm": "duplet-json/1",
  "$root": Term
}

Term := Pair | Leaf

Pair := {
  "<<": Term,
  ">>": Term
}

Leaf := protocol-level token,
        разрешаемый внешним leaf resolver
```

Parser отвечает за структуру пары, но не имеет права сам решать, какой `LinkId` соответствует произвольному JSON scalar/object.

## Минимальный лист: существующий LinkId

Первый raw resolver поддерживает явную ссылку на уже существующую AVM identity:

```json
{"$link": 42}
```

Семантика:

```text
{"$link": N}
 -> ProjectionRef::Anchor(N)
```

Ограничения:

- `N` — положительный целый JSON number в диапазоне `LinkId`;
- `0` запрещён как `invalid_link_id`;
- parser/resolver не создаёт отсутствующий LinkId;
- `find_projection` при отсутствующем anchor возвращает miss;
- `realize_projection` при отсутствующем anchor завершается ошибкой до materialization prefix.

Пример:

```json
{
  "$avm": "duplet-json/1",
  "$root": {
    "<<": {"$link": 10},
    ">>": {
      "<<": {"$link": 20},
      ">>": {"$link": 30}
    }
  }
}
```

После realization:

```text
args   = Link(20, 30)
entity = Link(10, args)
```

то есть `(rel,(sub,obj))`.

## Расширяемые resolvers листьев

Будущие задачи могут добавить:

```text
symbolic anchor
canonical Integer
Boolean / nil
Text
context/reference expression
```

Но запрещено вводить правила:

```text
"unknown string" -> store.create_point()
```

или:

```text
JSON scalar -> hidden host-side value keyed by LinkId
```

Рабочие spelling-и `$symbol`, `$integer`, `$text` не становятся нормативными до #173.

## Проекция в `ProjectionDescription`

Нативный путь:

```text
Duplet JSON
 -> parse JSON
 -> validate Document/Pair
 -> resolve Leaf -> ProjectionRef
 -> post-order build ProjectionNode list
 -> ProjectionDescription
```

Для пары `{"<<":B,">>":E}` adapter сначала проецирует `B` и `E`, затем добавляет `ProjectionNode{begin_ref,end_ref}`.

Post-order нужен, потому что `ProjectionDescription` разрешает Node ссылаться только на более ранние nodes.

## Разделение поиска и материализации

Parser/projector не принимает решение materialize. Caller явно выбирает:

```text
find_projection(store, description)
```

или:

```text
realize_projection(store, description)
```

### Поиск

- не вызывает `intern`;
- не вызывает `create_point`;
- miss не меняет `LinkStore::size()`.

### Материализация

- сначала валидирует description и anchors;
- затем явно вызывает canonical `intern(begin,end)`;
- повторный вызов идемпотентен.

Одинаковые пары получают одну structural identity только благодаря canonical `intern`, а не потому что parser увидел одинаковые JSON subtrees.

## JSON-дерево не является графом identity

JSON AST содержит occurrences. LinkStore содержит canonical graph identities.

Два одинаковых JSON-фрагмента являются двумя syntax occurrences, но после explicit realization могут схлопнуться в один `LinkId`.

Нельзя использовать адрес DOM node, pointer parser-а или occurrence index как semantic identity AVM.

## Именованные определения и общие ссылки

Raw `duplet-json/1` не вводит `$defs`, forward references или graph labels автоматически.

Такая возможность требует отдельного identity contract, потому что имя может означать:

1. существующий anchor;
2. создание независимой point identity;
3. alias на уже описанный duplet;
4. external symbol resolver entry.

До решения #173 эти случаи не смешиваются.

## Обычные JSON-значения

Object без `<<`/`>>` является Leaf token для resolver-а.

Если object содержит хотя бы один pair-marker, применяется strict pair validation: должны присутствовать оба marker-а и не должно быть чужих members.

Arrays и scalars также являются leaf tokens до определения конкретного resolver-а.

## Дубликаты ключей JSON

DOM parser может потерять duplicate members до semantic validation.

CLI/file frontend #172 должен обнаруживать duplicate `<<`, `>>`, `$avm`, `$root` и отвергать неоднозначный документ.

## Structural converter и native parser разделены

Strict converter #171 преобразует syntax и не обязан разрешать листья в LinkId.

Например:

```json
{"$rel":"+","$sub":1,"$obj":2}
```

преобразуется в:

```json
{
  "<<": "+",
  ">>": {
    "<<": 1,
    ">>": 2
  }
}
```

Это корректная structural migration. Исполняемой AVM-программой результат станет только после resolver-а, который определит canonical meaning `"+"`, `1`, `2`.

## Структурный конвертер версии 1

Поддерживаемый source relation-form:

```json
{
  "$rel": R,
  "$sub": S,
  "$obj": O
}
```

Преобразование:

```text
convert(R,S,O)
 -> Pair(convert(R), Pair(convert(S), convert(O)))
```

То есть:

```json
{
  "<<": R,
  ">>": {
    "<<": S,
    ">>": O
  }
}
```

Порядок `S/O` является veto-gate.

## Неполные legacy-формы

Старый jsonRVM допускает context-dependent relation forms, в которых `$sub` или `$obj` отсутствует.

Strict converter не исполняет context semantics и поэтому обязан отвергать их, а не угадывать недостающий operand.

Полная миграция относится к #174.

## Смешанные legacy-формы

Объект с `$rel/$sub/$obj` и дополнительными members strict converter отвергает как неоднозначный, потому что механический converter не имеет права терять или переинтерпретировать поля.

## Обратный конвертер

Для round-trip conformance поддерживается:

```text
Pair(R, Pair(S,O))
 -> {"$rel":R,"$sub":S,"$obj":O}
```

Он предназначен для relation-shaped subset и не является общим представлением произвольного `Link(A,B)` в jsonRVM.

Standalone duplet, который нельзя однозначно выразить explicit triplet-ом, отвергается.

## Формат вывода конвертера

Converter:

- использует UTF-8 JSON;
- печатает отступ 2 пробела;
- создаёт keys `<<`, затем `>>`;
- inverse создаёт `$rel`, `$sub`, `$obj`;
- завершает output переводом строки;
- сохраняет ordinary leaves;
- не добавляет `$avm` envelope автоматически.

## Граница с `JsonCompatibilitySession`

Существующий `JsonCompatibilitySession` остаётся старым compatibility frontend-ом.

Разделение:

```text
legacy JSON
 -> JsonCompatibilitySession / semantic migration tooling

native AVM duplet JSON
 -> DupletJson frontend
 -> ProjectionDescription
```

Не допускается общий interpreter с ветками `$rel` и `<<`.

## Диагностика

Ошибки должны содержать JSON path, например:

```text
$.items[2].program: incomplete legacy relation form: expected $rel, $sub and $obj
```

Диагностика является tooling text и не входит в semantic Link structure.

## Проверки соответствия

Минимальный corpus:

1. explicit triplet;
2. sentinel `R/S/O`, доказывающий `(R,(S,O))`;
3. relation в relation/subject/object slots;
4. triplet внутри array;
5. triplet внутри ordinary object;
6. incomplete `$sub`;
7. incomplete `$obj`;
8. mixed legacy members;
9. malformed pair без одного marker-а;
10. mixed pair members;
11. forward/inverse round-trip;
12. raw anchors -> `ProjectionDescription` -> canonical RelationEntity;
13. find miss не пишет;
14. repeated realize возвращает тот же root.

## Влияние на jsonRVM

Никакого.

`jsonRVM#10` закрывается как перенесённый. Existing parser/runtime/tests jsonRVM остаются в старой нотации и продолжают служить стабильным semantic oracle.

## Нормативные запреты

Запрещено:

- поддерживать `(rel,(obj,sub))` в AVM native syntax;
- считать `<<`/`>>` execution opcodes;
- выполнять `realize` внутри parser-а;
- создавать point из неизвестного JSON leaf;
- хранить JSON DOM как canonical program representation;
- смешивать native duplet parser и legacy compatibility interpreter;
- считать occurrence identity JSON semantic identity;
- превращать structural converter в jsonRVM evaluator;
- silently convert incomplete context-dependent triplets.

## Следующие этапы

1. #171 — strict structural converter;
2. #172 — native duplet parser/projector;
3. #173 — canonical leaf/value/symbol resolver;
4. #174 — полный semantic migrator jsonRVM;
5. #175 — CLI/examples/Showcase.