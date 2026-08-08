# Совместимость jsonRVM и AVM

Родительская задача: #123. Epic: #122.

## Происхождение данных

Аудит привязан к фиксированной версии исходного runtime:

```text
repository: netkeep80/jsonRVM
commit:     843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime:    jsonRVM 3.0.0
```

Машиночитаемый companion: `compat/jsonrvm-semantics.json`. Frozen assertions: `compat/jsonrvm-golden.json`.

Цель — не source compatibility со старым C++/JSON interpreter. Нужно определить:

- какая observable semantics Модели Отношений должна сохраниться в AVM;
- какой старый syntax должен остаться только frontend projection;
- какие operations являются explicit host effects;
- какие детали являются implementation artifacts и не должны мигрировать.

## Триединая сущность Модели Отношений

`jsonRVM` использует три роли:

```text
(relation, subject, object)
```

JSON projection:

```json
{
  "$rel": "relation",
  "$sub": "subject",
  "$obj": "object"
}
```

Концептуальная модель трактует:

```text
relation = controller
subject  = view / receiver / manifestation
object   = model / input
```

Эта execution semantics богаче простого структурного encoding и является предметом #124.

## Каноническое представление дуплетами

AVM использует один порядок:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

Следовательно:

```text
(relation, subject, object)
= (relation, (subject, object))
```

Отдельная физическая triplet record ядру AVM не нужна.

Для materialized triplet `t`:

```text
encode(t) -> entity LinkId
decode(entity LinkId) -> t
```

сохраняет все три роли. Inner и outer pairs каноничны, поэтому repeated encoding возвращает те же identities внутри одного logical store.

`find_relation_entity(relation, subject, object)` принципиально отличается от encode/materialize: сначала ищет `(subject, object)`, затем outer pair и при miss ничего не создаёт.

Это тот же общий инвариант:

```text
description/query != realization/write
```

## Центральный результат аудита

Исторический blocker — **не** математическое представление triplet через dyads. Эта задача проста и уже решена.

Сложность в том, что старый `jsonRVM` заставляет `nlohmann::json` одновременно играть роли:

1. syntax программы;
2. runtime scalar/collection values;
3. mutable execution context;
4. in-memory view Модели Отношений/database.

Execution semantics поэтому переплетена с JSON references, mutable containers, host concurrency и external modules.

AVM 1.0–1.4 уже устранил главную архитектурную связанность: execution identity — `LinkId`, программы/functions/frames живут в links. AVM 1.5 занимается **semantic migration**, а не очередной заменой storage.

## Категории совместимости

Каждая запись `compat/jsonrvm-semantics.json` относится к одной категории.

### `canonical-semantic`

Поведение принадлежит смыслу Relations Model/VM и должно существовать независимо от surface syntax: triune roles, parent contexts, ordered sequence, pure arithmetic и т. п.

### `projection-syntax`

Полезный внешний syntax, который frontend обязан скомпилировать/спроецировать в canonical links до execution: `$ent`, `$$obj`, JSON `$ref`, named/path addressing.

### `effect-adapter`

Поведение взаимодействует с external process state и требует явной capability/effect boundary: filesystem, HTTP, clock/sleep, stdout, plugins, lazy external entity retrieval.

### `implementation-artifact`

Деталь, существующая из-за mutable JSON runtime и не являющаяся semantics AVM: JSON type tags, mutexes containers, literal `operator[]` create-on-access и т. п.

### `defer`

Идея может быть полезна, но небезопасна до появления lower-level contracts. Главный пример — automatic parallel projection: сначала требуется purity/effect ordering.

## Что реально исполняет jsonRVM

### Триединый execution context

`vm_ctx` несёт:

```text
ent
rel
sub
obj
$
```

где `$` — parent/outer context. Старый runtime меняет `$.sub`/`$.rel`, а nested relations могут выполняться в `$.$`, `$.$.$` и более внешних contexts.

Текущий AVM структурно декодирует все роли, но bootstrap expressions в основном используют `subject = unit`. Это хороший minimal kernel, но не полный replacement исходной triune semantics. Поэтому #124 предшествует массовой миграции vocabulary.

### Context pronouns и parent traversal

Старый resolver распознаёт `$ent/$rel/$sub/$obj` и hard-coded `$$`, `$$$`, `$$$$`. Fixture `relative_addressing.json` проверяет эти формы.

Сохранять нужно **смысл**, а не четырёхуровневое текстовое ограничение. AVM должен представить parent traversal композиционно, а frontend — компилировать `$...` в canonical reference structure (#125/#126).

### Named и absolute references

`absolute_addressing.json` показывает named entities, nested paths и `add_entity`. Старый resolver также мог вызвать `database_api::get_entity`, если named entity отсутствовала.

В одном path смешивались:

- pure lookup;
- syntax/path parsing;
- external lazy retrieval;
- lvalue creation/mutation.

В AVM эти concerns разделяются:

```text
parse/project reference
 -> non-mutating resolve/find
 -> optional explicit external lookup effect
 -> optional explicit realization/write
```

Read miss никогда не создаёт links.

### Sequence, projection и foreach

Executable JSON arrays в старой модели играют роль ordered lambda-vectors. `foreachobj`/`foreachsub` создают child contexts для collection items.

Смысл относится к canonical semantics, JSON array — нет. AVM уже имеет canonical link lists и `sequence`; #127 расширяет их до deterministic projection/foreach semantics.

### Parallel object execution

Старый runtime экспериментировал с parallel object-like projections через C++ execution policies и mutex-protected mutable JSON.

Scheduler semantics намеренно не переносится сейчас. При materialization/effects порядок может быть observable. Parallel execution допустим только после explicit purity/effect model.

### Pure value vocabulary

Старый `import_relations_model_to` регистрирует большой vocabulary:

- conversions: `int`, `integer`, `float`, `double`, `null`;
- data: `where`, `union`, `size`, `get`, `set`, `erase`, `sequence/integer`;
- arithmetic: `*`, `:`, `+`, `-`, `pow`, `sqrt`, `sum`;
- logic/comparison: `^`, `==`, `!=`, `<`, `>`, `&&`;
- strings: conversion, append, find, split, join;
- control: foreach, if variants, then/else, while, typed switch, throw/catch;
- rendering: print, tag, XML, HTML;
- time: sleep и steady clock.

Нельзя механически port-ить эти operators до определения canonical value denotation. Coercion/type behavior `nlohmann::json` не является автоматически нормативным. Это gate #128.

### External vocabulary

Filesystem, HTTP и dynamic dictionary loading являются capabilities. Они не являются основанием для implicit host calls внутри core. #129 вводит explicit capabilities и deterministic fake providers до масштабной effect-vocabulary migration.

## Сводная compatibility matrix

| ID | Поведение jsonRVM | Категория | AVM сейчас | Решение | Gate |
|---|---|---|---|---|---|
| RM-TRIUNE-001 | роли `ent/rel/sub/obj` | canonical | partial | сохранить полный triune contract | #124 |
| CTX-CURRENT-001 | current context roles | canonical | partial | canonical context access | #125 |
| CTX-PARENT-001 | `$` parent chain | canonical | partial | unbounded compositional traversal | #125 |
| REF-PRONOUN-001 | `$ent`, `$$obj`, ... | projection | missing | compile в reference links | #126 |
| REF-ABSOLUTE-001 | named/path addressing | projection | missing | canonical reference algebra | #126 |
| REF-LAZY-DB-001 | lazy DB retrieval | effect | missing | explicit lookup capability | #129 |
| REF-LVALUE-001 | create-on-write traversal | projection/write | missing | разделить resolve и write | #126 |
| EXEC-ARRAY-SEQ-001 | ordered executable arrays | canonical | partial | canonical link sequence | #127 |
| EXEC-OBJECT-PAR-001 | parallel object projection | defer | missing | ждать effect/purity model | #127 |
| EXEC-FOREACH-OBJ-001 | `foreachobj` | canonical | missing | deterministic link-list map | #127 |
| EXEC-FOREACH-SUB-001 | `foreachsub` | canonical | missing | после triune contract | #127 |
| CTRL-IF-001 | Boolean conditional | canonical | partial | differential parity baseline | #131 |
| CTRL-WHILE-001 | while | canonical | missing | после ordering contract | #127 |
| CTRL-SWITCH-001 | typed switch | canonical | missing | canonical keys/values | #128 |
| ERROR-THROW-CATCH-001 | program error handling | canonical | partial | deterministic semantic failures | #131 |
| VALUE-COPY-001 | copy/view | canonical | partial | identity + explicit projection | #124 |
| VALUE-ARITH-001 | arithmetic | canonical | missing | сначала numeric denotation | #128 |
| VALUE-COMPARE-001 | compare/Boolean | canonical | partial | structural/value equality | #128 |
| VALUE-COLLECTION-001 | size/where/union | canonical | partial | canonical list semantics | #128 |
| VALUE-GETSET-001 | JSON get/set/erase | artifact | none | буквально не переносить | #126 |
| VALUE-STRING-001 | string operations | canonical | missing | canonical byte/text denotation | #128 |
| VALUE-TYPE-PRED-001 | JSON type predicates | artifact | n/a | только обоснованные structural predicates | #128 |
| DISPLAY-PRINT-001 | stdout | effect | tooling only | explicit effect/frontend | #129 |
| DISPLAY-MARKUP-001 | tag/XML/HTML | projection | missing | после text/projection semantics | #128 |
| EFFECT-TIME-001 | clock/sleep | effect | missing | explicit clock capability | #129 |
| EFFECT-FS-001 | filesystem | effect | missing | explicit FS capability | #129 |
| EFFECT-HTTP-001 | HTTP | effect | missing | сначала fake provider | #129 |
| EFFECT-DLL-001 | native dictionary loading | effect | missing | plugin/capability boundary | #129 |
| IMPL-JSON-MUTEX-001 | JSON mutexes | artifact | n/a | удалить из модели | #127 |
| IMPL-JSON-AST-001 | JSON как code/data/context | artifact | removed | не возвращать | #130 |

Authoritative tooling representation — JSON manifest; таблица здесь предназначена для человека.

## Дифференциальный корпус

### `CASE-RELATIVE-ADDRESSING`

Источник: `modules/console/test/relative_addressing.json`.

Проверяет context roles и parent-depth forms. В AVM resolution context pronoun обязан быть observational и не менять `LinkStore::size()`.

### `CASE-ABSOLUTE-ADDRESSING`

Источник: `modules/console/test/absolute_addressing.json`.

Старый fixture смешивает pure named lookup и `add_entity`. AVM должен разделить эти semantics.

### `CASE-BOOLEAN-BRANCH`

AVM уже имеет canonical Boolean values и link-native `if`. Это дешёвый differential baseline после фиксации точного legacy oracle.

### `CASE-FOREACH-CONTEXT`

Минимальная old-style program должна доказать child-context propagation до более сложной projection migration.

### `CASE-ARITHMETIC`

Фиксируются только deterministic scalar cases после выбора integer denotation в #128. Raw JSON numeric representation не является equality criterion.

### `CASE-MISSING-REFERENCE`

Нужно зафиксировать semantic failure category/context, а не byte-for-byte JSON exception rendering.

## Что уже frozen в main

`compat/jsonrvm-golden.json` содержит assertions, извлечённые из реальных legacy doctest для:

- relative addressing;
- absolute addressing;
- `where`;
- runtime-version provenance.

Остальные cases остаются `derive-fixture`, пока их expected behavior не будет подтверждено исполняемым legacy fixture/assertion или воспроизводимым oracle. Очевидность результата вроде «1+1=2» недостаточна как доказательство differential migration VM.

## Явные non-goals совместимости

AVM не обещает:

- одинаковые numeric `LinkId` в independent stores;
- одинаковое внутреннее JSON tree;
- одинаковый C++ exception type/text;
- quirks coercion `nlohmann::json`;
- automatic JSON member creation на read;
- hard limit в четыре parent-context levels;
- implicit database/network/filesystem access;
- automatic parallel execution до effect model.

## Связь с Anum/МТС

Для обоих frontend действует одна boundary:

```text
raw(A) != den(A)
find(A) не создаёт den(A)
realize(A) выполняется явно
interpret(F) не означает realize(F)
```

JSON text и Anum raw structure различны, но после projection должны быть способны обозначать одну canonical link denotation. Это задача #130.

## Порядок дальнейшей реализации

1. завершить #123: representative deterministic corpus и полная metadata классификации;
2. #124: meaningful non-unit subject без mutable host references;
3. #125: canonical current/parent execution contexts;
4. #126: reference algebra и non-mutating lookup;
5. #127/#128: projection/sequence и canonical values;
6. #129: explicit effects;
7. #130: frontend convergence;
8. #131: end-to-end differential program.

Назначение inventory — не дать миграции превратиться в копирование `base.rm.h`: каждое сохранённое свойство должно ссылаться на observable legacy behavior, а каждое отброшенное — иметь явную причину.
