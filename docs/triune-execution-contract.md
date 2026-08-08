# Контракт триединого исполнения AVM

Родительская задача: #124. Epic: #122.

## Статус

Этот документ определяет семантический контракт исполнения сущности Модели Отношений поверх уже существующего `Executor` AVM.

Ключевой результат аудита: generic `Executor` уже декодирует полный `RelationEntity` и передаёт handler-у точные роли `relation`, `subject`, `object`. Ограничение `subject == unit` относится только к bootstrap program-expression encoding и **не** является ограничением execution kernel.

Поэтому AVM 1.5 не вводит второй executor и не меняет каноническое представление сущности.

Контракт доказан runtime-сценарием #140, слитым через PR #141.

## Каноническая сущность

Исполняемая сущность имеет форму:

```text
entity = (relation, subject, object)
       = Link(relation, Link(subject, object))
```

После decoding четыре identity различаются принципиально:

```text
entity    identity конкретного проявления отношения
relation  identity семантики/контроллера
subject   входная роль view / receiver
object    входная роль model / input
```

Все четыре значения являются `LinkId` и не являются mutable C++ references.

## Результат исполнения — отдельное понятие

`Executor::execute(entity)` возвращает отдельный `LinkId` результата:

```text
execute(entity, context) -> result
```

`result` **не означает** автоматического присваивания:

```text
subject := result
relation := result
object := result
```

и не изменяет structural identity исходной entity.

Это решение необходимо, потому что legacy `jsonRVM` не имел единого result-slot:

- часть relations записывала значение в `$.rel`;
- `jsonCopy` и некоторые operations меняли `$.sub`;
- `jsonView` создавал дочерний context и использовал роли иначе.

Копировать эту mutable JSON aliasing model в AVM нельзя. Наблюдаемое legacy behavior переносится через явные link-native operations, а не через скрытую мутацию `ExecutionContext`.

## Три класса семантики relation

### 1. Чистая relation значения

Pure value relation использует `relation/subject/object/context`, но не изменяет canonical store и возвращает существующий canonical `LinkId`:

```text
(relation, subject, object)
-> result
```

Пример свойства:

```text
одинаковые relation/object,
разные subject
-> relation имеет право вернуть разные результаты,
если её semantics зависит от subject
```

Это основной conformance case meaningful non-unit subject.

### 2. Чистая relation проекции

Projection relation вычисляет view/representation из входных roles, но остаётся observational.

Она может:

- выбрать уже существующую identity;
- найти уже существующую canonical structure;
- выполнить read-only composition других pure relations.

Она не может скрыто materialize-ить отсутствующую связь только потому, что projection была запрошена.

Если необходимой denotation нет:

```text
projection/find -> deterministic absence/error
```

а не implicit `intern`.

Это согласуется с общей границей AVM:

```text
find != realize
```

### 3. Relation явного эффекта/materialization

Relation с эффектом может изменять store или внешний мир **только как часть собственного явного контракта**.

Примеры:

```text
pair_intern
explicit realization
filesystem/network capability в будущем #129
```

Executor не добавляет эффект автоматически. Relation сама обращается к разрешённой write/capability boundary и возвращает canonical outcome/result `LinkId`.

Если effect создаёт manifestation, её identity должна быть представима как обычная link-native value/entity, а не храниться только в скрытом C++ side table.

## Subject не является lvalue

Критический инвариант:

```text
ExecutionContext.subject : LinkId
```

не является ссылкой на mutable slot.

Следовательно legacy операция вида:

```text
$.sub = value
```

не переносится как mutation C++ variable внутри context.

Нужно определить, что именно наблюдал пользователь старой модели:

- новое значение view;
- новую manifestation entity;
- результат relation;
- явное изменение модели.

После этого поведение выражается отдельной canonical relation/structure/effect.

## Manifestation и identity исполняемой entity

Нельзя смешивать:

```text
entity_id      — identity входной исполняемой сущности
result         — вычисленный результат
manifestation  — при необходимости явно представленное проявление
```

В простом pure case manifestation может не существовать отдельно вообще.

Если semantics требует материализованного проявления, оно создаётся явно и получает собственный `LinkId`. Исходная immutable entity при этом не «переписывается».

## Bootstrap expression является частным случаем

Существующая program model использует:

```text
(relation, unit, payload)
```

Это удобное encoding исполнимого expression, где `payload` содержит аргументы/структуру программы.

Он остаётся полностью поддерживаемым, но трактуется как:

```text
частный frontend/program encoding
⊂
общая triune execution semantics
```

Нельзя реализовывать direct triune semantics условием:

```text
if subject == unit:
    expression mode
else:
    triune mode
```

для одной и той же relation identity, если это делает `unit` невозможным легитимным subject-value.

Если expression-wrapper semantics и direct triune semantics различаются, они должны иметь разные relation identities или явное structural encoding.

## Контекст и parent chain

Текущий `ExecutionContext` уже несёт identity текущей entity и её roles, а также runtime frame/parent state, необходимое существующему execution path.

#124 не вводит textual `$ent/$sub/$obj/$rel` resolution. Это следующий gate #125/#126.

Но #124 фиксирует основу: context roles являются typed identities и должны оставаться наблюдаемыми без JSON references.

Дополнительный аудит #125 показал, что execution step и semantic context boundary нельзя автоматически считать одним и тем же понятием. Этот вопрос вынесен в ADR #144 и не меняет triune contract #124.

## Наблюдаемость

Существующий observer contract уже различает:

```text
Enter(context)
Return(context, result)
Fail(context, phase)
```

Это соответствует новому контракту:

- `context.subject` остаётся входной identity;
- `result` передаётся отдельно в `Return`;
- observer не видит фиктивного mutation `subject := result`.

Trace/inspection tooling не требует отдельной triune VM.

## Ошибки

Ошибки должны быть детерминированы на semantic boundary:

- malformed/non-existing entity;
- unknown relation;
- invalid role/value для конкретной relation;
- missing projection target;
- failure явного effect/capability.

Host exception type/text не является semantic identity ошибки.

Pure failure не должен materialize-ить repair data или partial manifestation.

## Conformance-инварианты #124

Suite #140/#141 доказывает:

1. handler получает точные `entity/relation/subject/object` identities;
2. non-unit `subject` реально влияет на результат relation;
3. две entity с одинаковыми `relation/object`, но разными `subject` различимы семантически;
4. pure relation не меняет `LinkStore`;
5. read-only projection miss не materialize-ит отсутствующую structure;
6. explicit materialization relation изменяет store только через объявленную write operation;
7. `result` отделён от identity входной entity и от `subject`;
8. старый `(relation, unit, payload)` bootstrap path исполняется через тот же `Executor`;
9. `unit` остаётся допустимым обычным `subject` direct relation и не превращён в скрытый mode switch;
10. observer events сохраняют исходный context и отдельный return result.

Full CI для #141 прошёл warnings-as-errors, ASan/UBSan, installed-package consumer и portable matrix на Linux/Windows/macOS.

## Фактическая реализация direct-triune слоя

Public API находится в:

```text
include/avm/triune_primitives.h
```

и экспортируется через `avm/avm.h` и установленный `avm::core` package.

`DirectTriuneVocabulary` вводит отдельные identities:

```text
subject_value_relation
pair_find_relation
pair_realize_relation
pair_target_begin_relation
pair_target_end_relation
```

Ни одна из них не перегружает существующие bootstrap expression relations.

Регистрация выполняется в **существующий** executor:

```cpp
register_direct_triune_primitives(runtime.executor(), vocabulary);
```

Нового `TriuneExecutor` или второго runtime нет.

### Relation `subject_value`

Минимальное доказательство meaningful subject:

```text
(subject_value_relation, subject, object)
-> subject
```

Execution не изменяет store. Две entity с одинаковыми relation/object и различными subject возвращают разные identities.

### Почему target пары нельзя кодировать прямо в subject/object entity

Каноническое представление исполняемой entity само требует:

```text
Link(subject, object)
```

как inner dyad. Поэтому к моменту существования entity эта пара уже materialized.

Следовательно ошибочным был бы тест вида:

```text
(pair_find, subject, object)
-> проверить, отсутствует ли Link(subject, object)
```

Он никогда не может наблюдать настоящий miss: pair создана самим encoding entity.

### Link-native `PairTarget`

Для honest `find != realize` target вынесена в отдельную denotation.

`PairTarget` — свежая point identity `descriptor`, к которой привязаны два typed facts:

```text
(pair_target_begin_relation, descriptor, begin)
(pair_target_end_relation,   descriptor, end)
```

Создание descriptor и этих facts **не создаёт**:

```text
Link(begin, end)
```

поэтому target pair действительно может отсутствовать.

API:

```cpp
materialize_pair_target(store, vocabulary, begin, end);
decode_pair_target(store, vocabulary, descriptor);
```

Descriptor остаётся полностью link-native: JSON, string role names и side tables не используются.

### Relation `pair_find`

```text
(pair_find_relation, receiver, pair_target)
-> existing Link(begin, end)
```

Handler декодирует PairTarget и вызывает только:

```cpp
store.find(begin, end)
```

При miss возникает deterministic semantic failure. `store.size()` не изменяется ни на miss, ни на hit.

### Relation `pair_realize`

```text
(pair_realize_relation, receiver, pair_target)
-> intern(begin, end)
```

Это отдельная явная materialization boundary.

Первый вызов для missing pair создаёт ровно одну canonical связь. Повторный вызов возвращает тот же `LinkId` без роста store.

Таким образом граница:

```text
pair_find != pair_realize
```

доказана кодом и conformance tests, а не только документацией.

## Не входит в #124

- textual pronouns `$ent/$rel/$sub/$obj` — #125/#126;
- semantic context lineage и точная граница context creation — #144;
- mutable reference/lvalue algebra — #126;
- foreach/lambda/projection aggregation — #127;
- полный numeric/text/collection universe — #128;
- FS/HTTP/time/native capabilities — #129;
- автоматический parallel scheduler;
- второй executor;
- mutable JSON slot внутри `ExecutionContext`.

## Следующий gate

Triune execution contract завершён. Следующий semantic layer — #125/#144: определить program-visible current/parent context так, чтобы различать обычный dispatch step и настоящее создание дочернего Relations Model context, не materialize-я context автоматически в `LinkStore`.
