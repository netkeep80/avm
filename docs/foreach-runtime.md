# Детерминированный foreach в AVM

Родительская задача: #187. Родительский gate: #127. Контекст исполнения: #125.

## Назначение

Старый `jsonRVM` имел операции `foreachobj` и `foreachsub`, связанные с JSON containers и mutable `vm_ctx`.

AVM переносит только наблюдаемую вычислительную семантику:

- ordered collection;
- отдельный child context для каждого элемента;
- ориентация item через `subject` или `object`;
- ordered body results;
- fail-fast;
- обычный `Executor` и обычные observer events.

JSON array/object как runtime-тип для этого не нужен.

## Нормативная форма

Используются две отдельные relation identity:

```text
(foreach_object, body, collection)
(foreach_subject, body, collection)
```

Канонический триплет кодируется как обычно:

```text
Link(relation, Link(body, collection))
```

`body` — обычный executable `LinkId`.

`collection` — ordered link-list из `program_model.h`, завершающийся явно переданной identity `list_nil`.

## Почему body находится в subject выражения

Direct triune entity имеет роли:

```text
relation = foreach orientation
subject  = body
object   = collection
```

Это не означает, что semantic child `$sub` обязан быть body.

Execution entity и semantic context — разные уровни. Для каждой итерации body исполняется как обычная executable identity, а semantic child frame задаётся отдельно.

## `foreach_object`

Для каждого `item` из коллекции создаётся новый child:

```text
child.entity         = parent.entity
child.relation_state = parent.relation_state
child.subject        = parent.subject
child.object         = item
```

Затем:

```text
Executor::execute_child_semantic_context_outcome(body, ...)
```

использует тот же canonical Executor.

Так body может читать item через `SemanticContextRole::Object`, то есть AVM-аналог старого `$obj`.

## `foreach_subject`

Симметричная orientation:

```text
child.entity         = parent.entity
child.relation_state = parent.relation_state
child.subject        = item
child.object         = parent.object
```

Body читает item через `SemanticContextRole::Subject`.

Старый live oracle отдельно доказал только `foreachobj`; subject-orientation фиксируется собственным AVM conformance, а не выдаётся за исторически доказанное поведение jsonRVM.

## Свежий child для каждой итерации

Каждая итерация строит child **из одного и того же исходного parent `SemanticContextView`**.

Если body возвращает:

```text
ExecutionOutcome{
  result = X,
  semantic = child.with_relation_state(Y)
}
```

то `Y` не становится скрытым input state следующего item.

Это принципиально отличает foreach от ordered same-context sequence #162:

- sequence явно thread-ит output semantic state шага в следующий шаг;
- foreach создаёт независимые sibling child contexts.

Если нужно состояние между итерациями, оно должно быть представлено явно как данные/effect, а не как неявная утечка mutable context.

## Результат

`outcome.result` — ordered link-list body results:

```text
[item1, item2, item3]
 -> [body(item1), body(item2), body(item3)]
```

Порядок полностью сохраняется.

Пустая коллекция возвращает `list_nil`.

Parent semantic state возвращается неизменным:

```text
outcome.semantic = parent semantic view
```

То есть result и state остаются раздельными, как требует `ExecutionOutcome`.

## Materialization результата

Входная коллекция читается через `decode_link_list` без записи.

Body results сначала собираются в host-side временный vector только как промежуточный control-flow buffer. Canonical output list materialize-ится через `encode_link_list` **только после успешного завершения всех итераций**.

Это даёт важный failure contract:

- body первого item может совершить свои явные effects;
- body второго item может упасть;
- третий item не исполняется;
- partial output list не создаётся.

AVM не обещает rollback явных effects body: foreach не является транзакцией.

## Порядок effects

Iteration order равен order canonical link-list.

Никакой implicit parallelism не разрешён:

```text
item1 -> item2 -> item3
```

Observable effects body происходят в том же порядке.

Даже если в будущем pure projection сможет быть оптимизирована параллельно, effectful foreach не получает `std::execution::par` как semantic contract.

## Отсутствующий semantic context

Foreach зависит от child semantic roles, поэтому direct execution без `SemanticContextView` отвергается:

```text
Executor::execute(foreach_entity) -> error
```

Caller должен использовать explicit semantic execution path.

Это лучше скрытого создания «текущего контекста» внутри handler-а.

## Соответствие live oracle jsonRVM

Frozen case `CASE-FOREACH-CONTEXT` возвращает:

```json
{"/result":[1,2,3]}
```

AVM conformance воспроизводит semantic denotation:

1. ordered collection содержит identity `1,2,3`;
2. body возвращает child object identity;
3. `foreach_object` возвращает ordered list тех же identities.

Старый JSON wrapper `/result` не переносится как часть foreach semantics.

## Контракт наблюдения

Каждый body запускается обычным `Executor`.

Следовательно canonical observer получает обычные:

```text
Enter / Return / Fail
```

для foreach entity и для каждого body entity. Отдельного `ForeachExecutor` или специального observer channel нет.

## Инварианты

1. collection — link-native ordered list, не JSON array;
2. body — обычный executable LinkId;
3. для каждого item создаётся sibling child context;
4. object/subject orientation разделены разными relation identities;
5. result order равен input order;
6. parent semantic state не меняется скрыто;
7. child semantic mutation не протекает в следующий item;
8. failure останавливает последующие items;
9. partial result list не materialize-ится при failure;
10. implicit parallelism запрещён;
11. используется единственный canonical Executor.
