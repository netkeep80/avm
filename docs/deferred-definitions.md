# Отложенные определения функций

AVM различает исполнимый node `Def` и immutable строку определения функции, которую этот node материализует.

## Зачем нужны две формы

Projection/import должен уметь построить весь program graph до исполнения и при этом сохранить порядок объявлений. Если importer сразу вставит финальную строку function definition, `Call`, расположенный перед `Def` в sequence, ошибочно сможет найти функцию.

Поэтому projection создаёт deferred definition expression:

```text
parameters        = List(formal1, formal2, ...)
definitionPayload = Link(parameters, bodyRoot)
payload           = Link(functionHandle, definitionPayload)
defExpression     = (function_relation, unit, payload)
```

Само присутствие этой expression в store ещё не означает существование callable definition.

## Исполнение

Когда `Executor` dispatch-ит `defExpression` в handler `function_relation`, runtime декодирует node и materialize-ит:

```text
(function_relation, functionHandle, definitionPayload)
```

Сохранённое definition остаётся immutable и canonical. Повторное исполнение того же deferred definition идемпотентно. Другое definition для того же handle является явным conflict.

Это даёт непосредственное свойство порядка:

```text
Sequence(Def(f), Call(f))  -> успешно
Sequence(Call(f), Def(f))  -> Call не видит definition и завершается ошибкой
```

## Рекурсия

Function handle создаётся до построения body graph. Поэтому body может содержать `Call(handle, ...)`, даже если финальной definition row ещё нет. Исполнение `Def` делает recursive handle вызываемым без переписывания body.

## Граница слоя

Deferred node — часть link-native program model AVM и не зависит от JSON. JSON compatibility importer отображает textual syntax `Def` в эту форму, а execution целиком остаётся в `BootstrapRuntime` и `Executor`.

Это важный пример общего правила AVM: frontend может управлять порядком проекции и resolution текстовых symbols, но runtime semantics выражается canonical links.
