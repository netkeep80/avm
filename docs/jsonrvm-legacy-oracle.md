# Live oracle старого jsonRVM

Этот документ описывает воспроизводимое доказательство observable semantics старого `jsonRVM`, используемое в AVM 1.5 / #123.

## Зачем нужен oracle

`jsonRVM` содержит поведение, которое нельзя надёжно восстановить только по чтению `base.rm.h` и документации. В частности, старый runtime смешивает:

- mutable `ent/rel/sub/obj` context;
- `$ref` resolution;
- sequence execution;
- child contexts;
- JSON value semantics;
- разные conventions записи результата в `rel` или `sub`.

Поэтому expected result для differential migration нельзя получать из интуиции. Он считается frozen только после запуска реального старого runtime на фиксированном commit.

## Зафиксированный источник

Oracle всегда использует:

```text
repository: netkeep80/jsonRVM
commit:     843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime:    3.0.0
```

CI отдельно проверяет фактический checkout SHA перед сборкой.

## Граница безопасности архитектуры

Legacy runtime существует только внутри audit job:

```text
checkout pinned jsonRVM
  -> собрать минимальный rmvm
  -> выполнить маленькие deterministic fixtures
  -> сравнить observable result/error с frozen evidence
  -> завершить job
```

Он **не**:

- линкуется с `avm::core`;
- импортируется в `Executor`;
- становится fallback interpreter;
- участвует в production build AVM;
- определяет внутреннее представление values/contexts AVM;
- разрешает вернуть `nlohmann::json` как semantic universe ядра.

Git history и pinned CI oracle нужны для проверки миграции, а не для сохранения второй VM.

## Подтверждённые cases

### Арифметика

Fixture `arithmetic-add.json`:

```text
1 + 1 -> result = 2
```

Покрывает legacy semantic target `VALUE-ARITH-001`.

### Порядок sequence

Fixture `sequence-order.json` последовательно исполняет значения `1`, `2`, `3`.

Наблюдаемый итог:

```text
result = 3
```

Это фиксирует left-to-right ordered sequence behavior для `EXEC-ARRAY-SEQ-001` без утверждения о будущей parallel projection semantics.

### Child context `foreachobj`

Fixture `foreach-object-context.json` выполняет child relation над каждым object элемента массива и через `$ref: "$obj"` наблюдает child object.

Результат:

```json
{"result":[1,2,3]}
```

Покрываются `EXEC-FOREACH-OBJ-001` и наблюдение текущего child context. Этот fixture **не** считается доказательством `foreachsub`; `EXEC-FOREACH-SUB-001` остаётся отдельной незамороженной задачей.

### Boolean branch

Fixture `boolean-branch.json` выбирает object-branch отношения `if_rel_then_obj_else_sub` при `rel=true`.

Результат:

```text
result = 42
```

Это legacy baseline для `CTRL-IF-001`. Он не подменяет уже существующий link-native lazy `If` test AVM; differential comparison выполняется позже в #131.

### Композиция чистых отношений

Fixture `pure-relation-composition.json` сначала вычисляет `1 + 1`, затем второй relation получает предыдущий relation-value через `$ref: "$rel"` и вычисляет `2 + 3`.

Результат:

```text
result = 5
```

Case подтверждает observable composition через текущий relation state и одновременно показывает, почему semantics старого `vm_ctx` нельзя свести к одному абстрактному «return value» без отдельного triune contract #124.

### Отсутствующая ссылка

Fixture `missing-reference.json` обращается к заведомо отсутствующей identity `__avm_missing_reference_oracle__`.

Подтверждённое поведение:

- процесс завершается с кодом `1`;
- stdout не содержит успешного результата;
- stderr является JSON diagnostic;
- diagnostic содержит identity отсутствующей ссылки.

AVM не обязана копировать byte-for-byte текст ошибки старого runtime. Differential contract сохраняет semantic failure, а не host/filesystem wording.

## Два вида frozen evidence

В репозитории намеренно существуют два источника:

```text
compat/jsonrvm-golden.json
  -> assertions, уже существовавшие в legacy doctest

compat/jsonrvm-oracle-golden.json
  -> outcomes специально выделенных deterministic fixtures,
     подтверждённые запуском pinned rmvm
```

Первый источник фиксирует исторические tests самого `jsonRVM`. Второй расширяет corpus там, где старые tests не содержали достаточно точных assertions.

`tools/validate_jsonrvm_compatibility.py` объединяет оба источника и требует, чтобы каждый semantic ID со статусом `fixture_status = frozen` имел реальное evidence.

## Что oracle не доказывает

Live oracle не означает, что AVM обязана воспроизвести:

- внутреннее JSON representation;
- C++ exception type;
- абсолютный путь в diagnostic;
- quirks `nlohmann::json` вне выбранного semantic contract;
- mutex/parallel implementation старого runtime;
- database/filesystem/network effects без capability harness.

Он фиксирует только минимальные observable properties, необходимые для последующей link-native migration.

## Следующий шаг

После merge этого gate #123 получает достаточно представительный deterministic baseline для перехода к #124: формальному разделению entity identity, subject/view, object/model, вычисленного result и manifestation/projection без возврата к mutable JSON references.
