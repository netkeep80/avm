# Semantic migrator jsonRVM → AVM

Родительская задача: #174. Первый executable gate: #195. Native Duplet JSON umbrella: #169.

## Назначение

Semantic migrator переносит **наблюдаемую семантику** legacy jsonRVM в уже принятые link-native contracts AVM.

Он принципиально отличается от structural converter `json_duplet_converter.h`.

Structural converter отвечает только за механическую форму:

```text
(rel, sub, obj) -> Link(rel, Link(sub, obj))
```

и не знает специальных runtime значений `$rel`, `$sub`, `$obj`, `$ref`, sequence, foreach или legacy mutable context.

Semantic migrator, напротив, знает выбранный **source-language subset**, но не исполняет его.

```text
legacy JSON source
    ↓
semantic compilation
    ↓
Native Duplet JSON
    ↓
существующий Native JSON projection
    ↓
canonical LinkId program
    ↓
существующий Executor
```

В проекте по-прежнему существует только один semantic execution path.

## Граница первого gate

#195 поддерживает frozen arithmetic fixture:

```text
compat/jsonrvm-legacy-fixtures/arithmetic-add.json
```

Source form:

```json
{
  "$rel/result": {
    "$rel": "+",
    "$sub": 1,
    "$obj": 1
  }
}
```

Pinned jsonRVM oracle возвращает observable `result = 2`.

На первом gate внешний ключ `$rel/result` рассматривается только как legacy output envelope. Он **не** становится generic Object/Map value AVM.

Внутреннее arithmetic expression компилируется в direct triune relation:

```text
(integer_add, Integer(1), Integer(1))
```

и затем в Native Duplet JSON:

```json
{
  "$avm": "duplet-json/1",
  "$root": {
    "<<": {"$symbol": "integer_add"},
    ">>": {
      "<<": {"$integer": 1},
      ">>": {"$integer": 1}
    }
  }
}
```

`integer_add` здесь является только protocol-level symbolic anchor. Caller явно связывает его с `IntegerVocabulary::add_relation`.

Migrator не создаёт LinkId и не владеет таблицей canonical identities.

## Поддержанные arithmetic relations

Первый slice использует уже принятый Integer vocabulary:

```text
+ -> integer_add
- -> integer_subtract
* -> integer_multiply
/ -> integer_divide
```

Operands должны принадлежать signed `int64` migration domain первого Integer implementation.

Arithmetic semantics не переопределяется migrator-ом. В частности division следует принятому #165 contract: signed C++20 truncation toward zero с deterministic failure на division by zero и `INT64_MIN / -1` уже внутри Integer relation.

Migrator только выбирает relation identity и canonical operand denotations.

## Result и semantic state

После #191 pure Integer arithmetic возвращает canonical result и **не** делает скрытый:

```text
$rel := result
```

Это обязательная граница.

Legacy jsonRVM использовал mutable context slots, поэтому в более сложных programs arithmetic result мог одновременно становиться новым relation-state.

Такое поведение не возвращается в Integer primitive ради совместимости.

Следующий migration gate должен выразить legacy state transition явно поверх:

- `ExecutionOutcome`;
- immutable `SemanticContextView`;
- ordered sequence state threading #153/#162;
- canonical reference/state operations.

Тем самым:

```text
value computation != state transition
```

остаётся истинным и после полной миграции.

## Observable output metadata

`MigrationResult` содержит:

```text
document
observable_json_pointer
```

`document` является единственным AVM program artifact.

`observable_json_pointer` — metadata differential harness-а, позволяющая сопоставить canonical AVM result с legacy oracle field `/result`.

Она не materialize-ится в LinkStore и не является runtime value.

Такой boundary позволяет проверить старый внешний JSON result без создания generic mutable JSON object внутри AVM.

## Ошибки migration boundary

Первый gate детерминированно отвергает:

- non-object root;
- root без exact `$rel/result` envelope;
- дополнительные root members;
- arithmetic object без exact `$rel/$sub/$obj`;
- non-string `$rel`;
- неизвестный arithmetic operator;
- non-integer subject/object;
- integer вне текущего signed `int64` migration domain.

Эти ошибки возникают **до** Native Duplet projection и до любой materialization.

Runtime arithmetic failures, например division by zero, остаются ответственностью canonical Integer relation и не эмулируются migrator-ом.

## Lifetime boundary

После `migrate_program` исходный legacy JSON DOM больше не нужен.

Native document является самостоятельным внешним artifact. После его projection/realization canonical program graph также не зависит от lifetime JSON DOM.

Это исключает возврат старой архитектуры, где JSON одновременно был AST, data model и mutable runtime state.

## Differential evidence

Для каждого поддержанного construct нужны два независимых доказательства:

1. pinned jsonRVM oracle фиксирует observable behavior source fixture;
2. AVM test проходит полный новый pipeline и сравнивает semantic result.

Нельзя объявлять совместимость только потому, что generated JSON визуально похож на source.

Для `CASE-ARITHMETIC`:

```text
jsonRVM oracle -> 2
AVM migrated program -> canonical Integer(2)
decode Integer -> 2
```

Это и есть observable semantic equivalence первого gate.

## Следующие gates

После arithmetic vertical slice расширение идёт construct-by-construct:

1. ordered sequence;
2. explicit legacy relation-state transition;
3. `CASE-PURE-RELATION-COMPOSITION`;
4. Boolean branch;
5. foreach-object через уже принятый #187;
6. missing/reference behavior через #126;
7. оставшиеся corpus constructs только после отдельного semantic decision.

Не поддержанные while/switch/catch/effects не получают заглушек и не интерпретируются частично.

## Архитектурные запреты

1. migrator не принимает `LinkStore`;
2. migrator не вызывает `Executor`;
3. migrator не регистрирует native relations;
4. migrator не хранит global current context;
5. legacy JSON не становится runtime value AVM;
6. Native Duplet parser не получает legacy-special cases;
7. frozen jsonRVM не изменяется;
8. unsupported construct отвергается вместо скрытого fallback к старому interpreter;
9. compatibility state transition не прячется в pure Integer/Text primitives;
10. каждый новый supported construct получает oracle-backed conformance.
