# Независимая проекция исполнения

Родительская задача: #188. Общий gate: #127. Epic: #122.

## Назначение

В старом `jsonRVM` обычный JSON object без `$ref` и `$rel` выполнял сразу несколько функций:

1. ключ object-а разрешался как изменяемый destination;
2. значение исполнялось как программа;
3. для каждого элемента создавался отдельный `vm_ctx`;
4. набор элементов запускался через `std::execution::par`;
5. ошибки собирались после выполнения workers.

Эти свойства нельзя переносить в AVM одной монолитной операцией. Иначе JSON object, lvalue mutation и host scheduler снова стали бы частью semantic core.

Поэтому AVM разделяет:

- независимое вычисление нескольких executable bodies;
- ordered same-context sequence;
- foreach по collection;
- structural `ProjectionDescription`;
- destination/write semantics;
- будущую оптимизацию scheduler-а.

## Не путать с `ProjectionDescription`

`ProjectionDescription` из `projection.h` — это структурный план построения дуплетов:

```text
external structure
 -> ProjectionDescription
 -> find_projection | realize_projection
 -> canonical LinkId graph
```

Он не исполняет программы, не содержит execution context и не задаёт порядок effects.

Независимая проекция исполнения — другой контракт:

```text
canonical executable bodies
 -> Executor
 -> ordered body results
 -> canonical result list
```

Эти два понятия намеренно не объединяются одним API.

## Каноническая форма

Используется отдельная relation identity:

```text
(independent_projection, body_list, list_nil)
```

В терминах `RelationEntity`:

```text
relation = independent_projection
subject  = body_list
object   = list_nil
```

`body_list` — canonical ordered link-list обычных executable `LinkId`.

`list_nil` — явная identity конца и входной, и выходной последовательности.

JSON object/array не являются частью этой формы.

## Контекст body

Каждый body получает **один и тот же входной `SemanticContextView`**.

Если исходный context равен `C`, то:

```text
body1(C)
body2(C)
body3(C)
```

а не:

```text
C1 = body1(C)
C2 = body2(C1)
C3 = body3(C2)
```

Второй вариант уже является state-threading sequence и реализован отдельно через sequence semantics #162.

Returned semantic state конкретного body не становится неявным input следующего body.

Это позволяет моделировать независимые проекции без mutable shared `vm_ctx`.

## Execution nesting

Body исполняется тем же canonical `Executor`:

```text
Executor::execute_outcome_in_context(
    body,
    projection_context.semantic,
    projection_entity,
    projection_context.frame)
```

Поэтому execution observer видит обычную вложенность:

```text
Enter(projection)
  Enter(body1)
  Return(body1)
  Enter(body2)
  Return(body2)
Return(projection)
```

Отдельного `ProjectionExecutor` или scheduler-specific execution path нет.

## Результат

Body results сохраняются в том же порядке:

```text
[b1, b2, b3]
 -> [result(b1), result(b2), result(b3)]
```

Результат кодируется тем же canonical ordered link-list и тем же `list_nil`.

Parent semantic state возвращается отдельно и без изменения:

```text
ExecutionOutcome{
  result = result_list,
  semantic = original_semantic_context
}
```

## Граница materialization

Входной `body_list` полностью декодируется до начала исполнения bodies. Read path не создаёт links.

Во время исполнения body results временно накапливаются только как host-side vector `LinkId`.

Canonical output list создаётся через `encode_link_list` **только после успешного завершения всех bodies**.

Следствие:

- если `body1` завершился успешно;
- `body2` завершился ошибкой;
- `body3` не исполняется;
- partial output list не materialize-ится.

При этом AVM не обещает rollback явных effects, которые уже выполнил `body1`. Независимая проекция не является транзакцией.

## Порядок и effects

Первый contract выполняет bodies строго в canonical list order.

Это даёт детерминированный observable order:

```text
body1 -> body2 -> body3
```

Если bodies имеют effects, они наблюдаются в том же порядке.

Если требуется иной порядок, он должен быть представлен явной structural program form, а не выбором host scheduler-а.

## Почему `std::execution::par` не является семантикой

Pinned `jsonRVM` использовал `std::execution::par` после подготовки `callctx` для членов JSON object.

Но эта деталь одновременно зависела от:

- mutable JSON destinations;
- host thread scheduler;
- mutex-ов JSON containers;
- post-hoc aggregation exceptions.

Ни один из этих implementation details не определяет canonical identity вычисления AVM.

Поэтому implicit parallel execution в AVM 1.5 запрещено.

В будущем proven-pure projection может быть оптимизирована параллельно только если доказано сохранение:

- canonical result;
- result order;
- failure behavior;
- observer contract;
- отсутствия observable effects и conflicts.

До такого доказательства последовательное исполнение является нормативным.

## Отличие от sequence

Sequence #162:

- исполняет ordered bodies;
- явно передаёт output semantic state шага в следующий шаг;
- предназначена для последовательной эволюции state.

Independent projection:

- исполняет ordered bodies;
- каждому body даёт один исходный semantic state;
- игнорирует body-local semantic transition при переходе к соседнему body;
- собирает отдельный ordered result list.

То есть различие является семантическим, а не scheduler optimization.

## Отличие от foreach

Foreach #187:

- принимает collection items;
- создаёт свежий child semantic frame для каждого item;
- помещает item в `subject` или `object` role child context.

Independent projection:

- принимает список executable bodies;
- не создаёт child semantic frame;
- все bodies видят тот же semantic context.

Обе операции являются ordered и fail-fast, но решают разные задачи.

## Destination и write semantics

Legacy JSON key выполнял роль lvalue destination. Этот аспект **не входит** в independent projection.

AVM не должен восстанавливать скрытую конструкцию:

```text
JSON key
 -> mutable destination pointer
 -> create missing member on write
```

Если migration corpus требует записи результата в модель, это должно быть выражено отдельной canonical write/materialization/effect operation поверх reference contract #126 и будущего data-model/effect contract.

Read resolution и execution projection не создают destination автоматически.

## Связь с semantic migrator

#174 может использовать independent projection как frontend-neutral target для той части legacy lambda-structure, где observable contract сводится к независимому вычислению результатов.

При этом #174 не должен заявлять полную parity старого plain-object lvalue behavior, пока destination/write semantics не представлены отдельным explicit contract.

JSON wrapper вроде `/result` в oracle fixture может оставаться harness/output projection, если differential assertion проверяет вычисленное значение, а не mutation модели.

## Persistent equivalence

Canonical identity программы и результата не зависит от process lifetime.

После первого исполнения output list сходится к canonical links. После close/reopen того же `PersistentLinkStore`:

- relation/body/result identities сохраняются;
- тот же projection root исполняется обычным `Executor` после регистрации native handlers;
- возвращается тот же exact output `LinkId`;
- повторное исполнение не увеличивает store.

## Инварианты

1. `ProjectionDescription` остаётся structural find/realize API;
2. execution projection имеет отдельную relation identity;
3. body list является canonical link-list;
4. каждый body получает один исходный immutable semantic context;
5. semantic state между bodies не thread-ится;
6. result order равен body order;
7. failure — fail-fast;
8. partial output list не materialize-ится;
9. явные effects не откатываются;
10. implicit parallelism запрещён;
11. используется единственный `Executor`;
12. JSON lvalue/member semantics не входят в core contract.
