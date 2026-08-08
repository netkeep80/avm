# Нативный дуплетный JSON-формат AVM

Родительская задача: #169. ADR-задача: #170. Связанные задачи: #130, #171–#175.

## Статус

Этот документ определяет нативный JSON frontend AVM версии `duplet-json/1`.

Формат принадлежит AVM. Он **не заменяет** и не изменяет JSON-язык `jsonRVM`. Исторический `jsonRVM` сохраняет формы `$rel/$sub/$obj` и используется как compatibility/oracle source.

Главный принцип новой нотации:

```text
JSON-пара описывает ровно Link(begin, end)
```

а триединая сущность Модели Отношений является только частным структурным паттерном пары.

## Канонический порядок ролей

Модель Отношений использует:

```text
(rel, sub, obj)
```

Каноническое представление через вложенные дуплеты:

```text
(rel, sub, obj) = (rel, (sub, obj))
```

Следовательно AVM RelationEntity структурно равна:

```text
Link(relation, Link(subject, object))
```

В старом `jsonRVM#10` исторически была записана перестановка `(rel,(obj,sub))`. Она **не является** compatibility-вариантом AVM и не поддерживается native frontend-ом.

## Базовая JSON-форма дуплета

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

Объект пары должен содержать **ровно** два поля `<<` и `>>`.

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

Явный relation pattern:

```json
{
  "<<": <relation>,
  ">>": {
    "<<": <subject>,
    ">>": <object>
  }
}
```

Это **не отдельная JSON-конструкция** и не специальный opcode parser-а. Это обычная пара, у которой `end` также является парой.

Именно Executor позднее интерпретирует полученный canonical `LinkId` как RelationEntity по существующему контракту:

```text
decode_relation_entity(entity)
 -> relation
 -> subject
 -> object
```

Native JSON adapter не исполняет этот паттерн и не знает controller semantics.

## Почему используются `<<` и `>>`

Короткий вариант через JSON array:

```json
[begin, end]
```

не выбран как нормативный по трём причинам:

1. `jsonRVM` исторически использует arrays как исполняемые последовательности;
2. визуально array не показывает направление связи;
3. `<<` и `>>` позволяют глазами читать рекурсивную асеть как begin/end links.

При этом символы не имеют execution meaning. Они являются только transport spelling полей `begin` и `end`.

## Документ и bare term

### Versioned file/document

Файл AVM Native JSON версии 1 имеет envelope:

```json
{
  "$avm": "duplet-json/1",
  "$root": <term>
}
```

Envelope относится только к transport layer.

`$avm` и `$root` **не материализуются** как связи и не входят в semantic identity программы.

Version marker обязателен для file/CLI format, чтобы будущая версия могла менять surface protocol без эвристического определения формата.

### Bare term API

Library adapter может принимать непосредственно `<term>`, если вызывающий код уже явно выбрал parser версии `duplet-json/1`.

Bare term не должен использоваться для автоматического определения формата файла.

## Term и leaf

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
        который разрешается внешним leaf resolver
```

JSON parser отвечает только за синтаксис Pair/Document.

Он не имеет права самостоятельно решать, какой `LinkId` соответствует произвольному JSON scalar/object.

## Минимальный стандартный leaf: существующий LinkId

Первый raw resolver должен поддержать явную ссылку на существующую AVM identity:

```json
{"$link": 42}
```

Нормативная семантика:

```text
{"$link": N}
 -> ProjectionRef::Anchor(N)
```

Ограничения:

- `N` — положительный целый JSON number в диапазоне `LinkId`;
- `0` запрещён как `invalid_link_id`;
- parser/resolver не создаёт отсутствующий LinkId;
- `find_projection` при отсутствующем anchor возвращает miss;
- `realize_projection` при отсутствующем anchor завершает операцию ошибкой **до** materialization prefix.

Пример RelationEntity над существующими anchors:

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

После projection/realization структура обязана быть:

```text
args   = Link(20, 30)
entity = Link(10, args)
```

то есть `(rel,(sub,obj))`.

## Расширяемые leaf resolvers

Будущие задачи могут добавить protocol-level leaves:

```text
symbolic anchor
canonical Integer
Boolean / nil
Text
context/reference expression
```

Но эти формы должны реализовываться отдельным resolver/projection contract.

Запрещено вводить правило:

```text
"unknown string" -> store.create_point()
```

или:

```text
JSON scalar -> hidden host-side value keyed by LinkId
```

Такой подход создаёт вторую невидимую semantic universe и нарушает persistence/find semantics.

## Рекомендуемые будущие spelling-и

Следующие spelling-и зарезервированы как рабочее пространство resolver-а, но **не становятся нормативными до #173**:

```json
{"$symbol": "integer_add"}
{"$integer": 7}
{"$text": "hello"}
```

Raw v1 parser должен передавать неизвестный непарный object leaf resolver-у, а стандартный resolver обязан детерминированно отвергать неизвестный leaf kind.

Это позволяет расширять protocol layer без изменения `ProjectionDescription` и LinkStore.

## Parser и `ProjectionDescription`

Нативный путь:

```text
Duplet JSON
 -> parse JSON
 -> validate Document/Pair syntax
 -> resolve Leaf -> ProjectionRef
 -> post-order build ProjectionNode list
 -> ProjectionDescription
```

Для пары:

```json
{"<<": B, ">>": E}
```

adapter сначала проецирует `B` и `E`, затем добавляет:

```text
ProjectionNode{begin_ref, end_ref}
```

и возвращает `ProjectionRef::Node(new_index)`.

Post-order нужен, потому что `ProjectionDescription` разрешает Node ссылаться только на более ранние nodes.

## `find` и `realize` остаются разными операциями

Parser/projector не принимает решение materialize.

После него caller явно выбирает:

```text
find_projection(store, description)
```

или:

```text
realize_projection(store, description)
```

### Find

- не вызывает `intern`;
- не вызывает `create_point`;
- miss не меняет `LinkStore::size()`;
- одинаково работает для in-memory и persistent stores.

### Realize

- сначала валидирует description и anchors;
- затем явно вызывает canonical `intern(begin,end)`;
- повторный вызов идемпотентен;
- одинаковые пары могут получить одну structural identity только здесь, а не потому что parser видел одинаковые JSON subtrees.

## JSON tree не является графом identity

JSON AST — дерево occurrence-ов. LinkStore — граф canonical links.

Два одинаковых фрагмента:

```json
{"<<": {"$link": 1}, ">>": {"$link": 2}}
```

в двух местах JSON являются двумя syntax occurrences.

При `realize_projection` они могут схлопнуться в один canonical `LinkId`, потому что `intern(1,2)` каноничен.

Запрещено использовать:

- адрес JSON DOM node;
- pointer на parser object;
- индекс occurrence без explicit document semantics

как semantic identity AVM.

## Named definitions и shared references

`duplet-json/1` raw tree не вводит `$defs`, forward references или graph labels автоматически.

Они могут появиться отдельным document-level расширением после определения identity ownership.

Причина: named definition может означать разные вещи:

1. имя существующего anchor;
2. создание новой независимой point identity;
3. alias на уже описанный duplet;
4. external symbol resolver entry.

Смешивать эти варианты одной строкой опасно для persistence и `find != realize`.

## Обычные JSON values не являются structural Pair

Любой JSON object, который не содержит pair-marker `<<`/`>>`, является Leaf token для resolver-а.

Если object содержит хотя бы один pair-marker, parser применяет strict pair validation:

- есть оба `<<` и `>>`;
- нет других members.

Это не даёт malformed pair случайно уйти в generic leaf resolver.

JSON arrays и scalars также являются leaf tokens на raw adapter boundary, пока конкретный resolver не определит их projection.

## Дубликаты JSON keys

Стандартный DOM parser может потерять информацию о duplicate object members до semantic validation.

CLI/file frontend должен использовать parse mode/callback, который обнаруживает duplicate members, либо отдельную pre-validation стратегию.

Документ с duplicate `<<`, `>>`, `$avm` или `$root` должен быть отвергнут как неоднозначный.

До реализации такого контроля #172 не считается завершённым.

## Structural converter и native parser — разные компоненты

Strict converter #171 переводит surface syntax:

```text
explicit triplet JSON
 -> duplet JSON
```

и **не обязан** уметь разрешить листья в LinkId.

Например:

```json
{"$rel":"+","$sub":1,"$obj":2}
```

структурно преобразуется в:

```json
{
  "<<": "+",
  ">>": {
    "<<": 1,
    ">>": 2
  }
}
```

Это корректный результат structural migration, но он станет executable native AVM program только когда resolver определит canonical meaning листьев `"+"`, `1`, `2`.

Converter не должен ради удобства создавать identities или встраивать hidden symbol table.

## Structural converter v1

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

Порядок `S`/`O` является conformance-veto.

## Неполные legacy relation forms

Старый jsonRVM допускает context-dependent формы, в которых `$sub` или `$obj` отсутствует.

Например runtime может подставлять текущее semantic relation-state.

Strict structural converter **не исполняет context semantics**, поэтому v1 обязан отвергать:

```json
{"$rel":"+","$obj":1}
```

а не угадывать недостающий subject.

Полная миграция таких программ относится к #174 после context/reference contracts.

## Смешанные legacy relation forms

Объект вида:

```json
{
  "$rel": R,
  "$sub": S,
  "$obj": O,
  "extra": X
}
```

strict converter отвергает как неоднозначный.

Причина: неизвестно, является `extra` частью data projection, metadata или ошибкой source format. Механический converter не имеет права терять member или придумывать representation.

## Inverse converter

Для conformance нужен обратный путь поддерживаемого relation-shaped subset:

```text
Pair(R, Pair(S,O))
 -> {"$rel":R,"$sub":S,"$obj":O}
```

Он предназначен прежде всего для проверки:

```text
triplet -> duplet -> triplet
```

и **не является** общим способом представить произвольную пару в старом jsonRVM.

Произвольный `Link(A,B)`, где `B` не relation-arguments pair, не имеет однозначного explicit-triplet representation и должен быть отвергнут inverse converter-ом.

## Формат converter output

Converter должен:

- использовать UTF-8 JSON;
- выдавать deterministic pretty-print с отступом 2 пробела;
- создавать pair keys в порядке `<<`, затем `>>`;
- создавать inverse triplet keys в порядке `$rel`, `$sub`, `$obj`;
- завершать файл переводом строки;
- не нормализовывать значения чисел/строк сверх поведения JSON parser/serializer;
- не менять ordinary object members кроме рекурсивного преобразования их values.

Converter output не получает `$avm` envelope автоматически: mechanical conversion может применяться к fragment-у внутри произвольного legacy document.

Отдельный `--envelope` может быть добавлен позднее после появления полноценного native leaf resolver.

## Граница с `JsonCompatibilitySession`

Существующий `JsonCompatibilitySession` остаётся compatibility frontend-ом и не переписывается под `<<`/`>>`.

Разделение:

```text
legacy JSON
 -> JsonCompatibilitySession / semantic migration tooling

native AVM duplet JSON
 -> DupletJson frontend
 -> ProjectionDescription
```

Нельзя добавлять branch вида:

```text
if object has "$rel" ... else if object has "<<" ...
```

в один общий interpreter. Это снова смешало бы два protocol языка и сделало JSON частью runtime semantics.

## Ошибки

Ошибки native parser/converter должны содержать JSON path к проблемной форме, например:

```text
$.items[2].program: incomplete legacy relation form: expected $rel/$sub/$obj
```

или:

```text
$.$root.>>: malformed duplet: fields << and >> must appear together
```

Диагностика является host tooling text, а не частью semantic Link structure.

## Conformance

Минимальный набор:

1. один explicit triplet;
2. sentinel `relation=R, subject=S, object=O`, доказывающий `(R,(S,O))`;
3. relation в relation slot;
4. relation в subject slot;
5. relation в object slot;
6. triplet внутри array;
7. triplet внутри ordinary object;
8. incomplete `$sub`;
9. incomplete `$obj`;
10. mixed legacy members;
11. malformed pair без `<<`;
12. malformed pair без `>>`;
13. mixed pair members;
14. forward/inverse round-trip supported subset;
15. raw native anchors -> `ProjectionDescription` -> canonical RelationEntity;
16. find miss не пишет;
17. repeated realize возвращает тот же canonical root.

## Влияние на jsonRVM

Никакого.

`jsonRVM#10` закрывается как перенесённый в AVM. Existing jsonRVM source files, parser, tests и runtime остаются в старой нотации.

Это позволяет использовать jsonRVM как стабильный semantic oracle во время разработки AVM, вместо одновременного изменения и источника, и целевой реализации.

## Нормативные запреты

Запрещено:

- поддерживать `(rel,(obj,sub))` в native AVM;
- считать `<<`/`>>` execution opcodes;
- выполнять `realize` внутри parser-а;
- создавать point из неизвестного JSON leaf;
- хранить JSON DOM как canonical program representation;
- смешивать native duplet parser и legacy compatibility interpreter;
- считать occurrence identity JSON semantic identity;
- превращать structural converter в jsonRVM evaluator;
- silently convert incomplete context-dependent triplets.

## Следующие gates

После этого ADR:

1. #171 — strict structural converter;
2. #172 — native duplet parser/projector;
3. #173 — canonical leaf/value/symbol resolver;
4. #174 — полный semantic migrator jsonRVM;
5. #175 — CLI/examples/Showcase.