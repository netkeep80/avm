# Семантический мигратор jsonRVM → AVM

Родительская задача: #174. Epic: #122. Native Duplet JSON umbrella: #169.

## Назначение

Semantic migrator переносит **наблюдаемую семантику** выбранного frozen subset legacy `jsonRVM` в уже принятые link-native contracts AVM.

Он принципиально отличается от structural converter `json_duplet_converter.h`.

Structural converter отвечает только за механическую форму:

```text
(rel, sub, obj) -> Link(rel, Link(sub, obj))
```

и не знает специальных runtime значений `$rel`, `$sub`, `$obj`, `$ref`, sequence, foreach или legacy mutable context.

Semantic migrator знает только явно доказанный source-language subset, но **не исполняет** его:

```text
legacy JSON source
    ↓
semantic compilation
    ↓
Native Duplet JSON / duplet-json/1
    ↓
существующий Native JSON projector
    ↓
ProjectionDescription
    ↓
explicit find | realize
    ↓
canonical LinkId program
    ↓
существующий Executor
```

В проекте существует один semantic execution path. Migrator не принимает `LinkStore`, не вызывает `Executor` и не содержит compatibility runtime.

## Источник совместимости

Pinned oracle:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Frozen manifest:

```text
compat/jsonrvm-oracle-golden.json
```

Поддержка нового construct добавляется только вместе с frozen fixture и differential AVM conformance.

## Текущий supported subset

| Frozen case | Legacy observation | Canonical AVM mapping | Статус |
|---|---|---|---|
| `CASE-ARITHMETIC` | `1 + 1 -> 2` | direct canonical Integer relation | ✅ #195/#196 |
| `CASE-SEQUENCE-ORDER` | `[1,2,3] -> 3` | sequence + explicit relation-state commits | ✅ #197/#198 |
| `CASE-PURE-RELATION-COMPOSITION` | `1+1; $rel+3 -> 5` | canonical reference + pure relation application + explicit commits | ✅ #197/#198 |
| `CASE-FOREACH-CONTEXT` | `foreachobj [1,2,3] -> [1,2,3]` | existing `ForeachVocabulary::object_relation` + `Current.Object` reference | 🚧 #199 |
| `CASE-BOOLEAN-BRANCH` | branch result `42` | canonical Boolean/lazy control | ⏭ следующий gate |
| `CASE-MISSING-REFERENCE` | deterministic failure | canonical observational reference miss | planned после Boolean |

Таблица означает **поддержанный semantic migration corpus**, а не полную совместимость со всем старым `base.rm.h`.

## Арифметический vertical slice — #196

Frozen source:

```json
{
  "$rel/result": {
    "$rel": "+",
    "$sub": 1,
    "$obj": 1
  }
}
```

Pinned oracle возвращает observable `result = 2`.

Внешний ключ `$rel/result` является только legacy output envelope. Он не становится generic Object/Map value AVM.

Expression компилируется в:

```text
(integer_add, Integer(1), Integer(1))
```

и затем в canonical Native Duplet form.

Поддержанный arithmetic vocabulary:

```text
+ -> integer_add
- -> integer_subtract
* -> integer_multiply
/ -> integer_divide
```

Operands принадлежат signed `int64` migration domain. Сам arithmetic contract принадлежит `IntegerVocabulary`, а не migrator-у.

## Result и semantic relation-state — #198

Pure Integer arithmetic возвращает canonical result и не выполняет скрытое:

```text
$rel := result
```

Legacy stateful composition выражается отдельными frontend-neutral executable relations:

```text
commit_relation_state
resolve_reference_relation
apply_pure_relation
```

Для frozen composition:

```text
1 + 1
$rel + 3
```

migrated structure концептуально выполняет:

```text
apply(integer_add, quote(1), quote(1)) -> 2
commit -> semantic $rel = 2
resolve(Current.RelationState) -> 2
apply(integer_add, 2, quote(3)) -> 5
commit -> semantic $rel = 5
```

Таким образом сохраняется инвариант:

```text
value computation != state transition
```

`$ref:"$rel"` существует только в legacy compiler boundary. Generated program содержит canonical `ReferenceRole::RelationState(Current)` identity; runtime не парсит `$rel` strings.

## Упорядоченный sequence — #198

Legacy source array в executable position:

```json
[1, 2, 3]
```

компилируется в existing canonical sequence с explicit commits каждого legacy step.

Existing sequence runtime thread-ит `ExecutionOutcome.semantic` между children. Никакого второго sequence executor нет.

Это отличается от ordinary data list: JSON array интерпретируется как sequence **только в доказанном legacy executable context**.

## Перенос foreach object context — #199

Frozen source:

```json
{
  "$rel/result": {
    "$obj": [1, 2, 3],
    "$rel": "foreachobj",
    "$sub": {
      "$obj": {"$ref": "$obj"},
      "$rel": "="
    }
  }
}
```

Pinned oracle:

```json
{"/result":[1,2,3]}
```

Core foreach semantics уже была принята в #187/#189, поэтому #199 **не добавляет новый runtime primitive**.

Legacy form компилируется в существующую canonical structure:

```text
(foreach_object,
    resolve_reference(Current.Object),
    [Integer(1), Integer(2), Integer(3)])
```

где:

- `foreach_object` — caller-owned anchor на `ForeachVocabulary::object_relation`;
- collection — canonical ordered link-list, завершающийся existing `bootstrap_nil`;
- каждый item получает fresh sibling child context;
- `child.object = item`;
- body читает item через canonical `ReferenceRole::Object(Current)`;
- body исполняется обычным `Executor`;
- ordered result list возвращается без изменения parent semantic state.

### Почему legacy `=` здесь не становится generic оператором

Frozen body:

```json
{
  "$obj": {"$ref": "$obj"},
  "$rel": "="
}
```

наблюдаемо ведёт себя в этом exact case как projection текущего child object.

Migrator распознаёт **ровно этот evidence-backed shape** и компилирует его в:

```text
resolve_reference(Current.Object)
```

Из одного fixture нельзя выводить generic assignment/equality/lvalue semantics старого `=`. Любые другие формы `=` остаются unsupported до отдельного frozen evidence и semantic contract.

### Foreach и state threading

Foreach принципиально отличается от sequence:

```text
sequence child output state -> next child input state
foreach iteration child state -X-> next sibling
```

Каждая итерация строится из исходного parent semantic context. Parent outcome после frozen identity foreach остаётся неизменным:

```text
ExecutionOutcome {
    result = ordered result list,
    semantic = input semantic context
}
```

### Collection — данные, не executable sequence

`[1,2,3]` внутри `$obj` `foreachobj` компилируется в canonical list **данных**:

```text
[Integer(1), Integer(2), Integer(3)]
```

Здесь не используются `quote` или `commit_relation_state`. Это важная source-context distinction: одинаковая JSON array surface form не определяет одну universal runtime semantics.

## Символьные anchors

Generated `duplet-json/1` может содержать protocol-level symbolic anchors, например:

```text
integer_add
bootstrap_unit
bootstrap_nil
bootstrap_quote
bootstrap_sequence
semantic_commit_relation_state
semantic_resolve_reference
semantic_apply_pure_relation
current_relation_state_reference
current_object_reference
foreach_object
```

Эти строки являются transport metadata frontend-а. Caller явно связывает их с уже существующими LinkIds конкретного prepared runtime.

Migrator не создаёт canonical identities и не владеет глобальным symbol registry.

## Метаданные наблюдаемого результата

`MigrationResult` содержит:

```text
document
observable_json_pointer
```

`document` — самостоятельный Native JSON artifact.

`observable_json_pointer` — differential-harness metadata для сопоставления canonical AVM result с legacy oracle field `/result`. Эта строка не materialize-ится в LinkStore и не является semantic value.

## Границы materialization

Нормативное разделение:

```text
migrate legacy JSON
    -> no LinkStore

project Native JSON
    -> no LinkStore mutation

find_projection
    -> observation only

realize_projection
    -> explicit static program/value materialization

execute
    -> только явно определённые runtime effects canonical relations
```

Для dynamic pure relation application #198 first execution может materialize canonical target RelationEntity. После convergence repeated identical execution не должно увеличивать store.

Для #199 identity foreach входной и выходной ordered list сходятся к одной canonical identity, поэтому successful execution после realization не требует дополнительных list nodes.

## Граница времени жизни

После `migrate_program` исходный legacy JSON DOM больше не нужен.

Native document является самостоятельным artifact. После projection/realization canonical program graph также не зависит от lifetime source DOM.

Differential tests намеренно уничтожают source DOM до projection.

## Ошибки migration boundary

Migrator детерминированно отвергает неподдержанные или неоднозначные формы до projection/materialization.

Среди уже закреплённых veto cases:

- non-object root;
- root без exact `$rel/result` envelope;
- дополнительные root members;
- malformed arithmetic relation;
- unknown arithmetic operator;
- non-integer arithmetic operand;
- unsupported sequence reference role;
- empty executable sequence;
- legacy `foreachsub` без frozen compatibility evidence;
- foreach body с `$ref` не на `$obj`;
- modified/generic `=` foreach body;
- non-Integer или empty collection первого foreach slice;
- дополнительные ambiguous foreach fields.

Runtime failures canonical relations не эмулируются migrator-ом.

## Дифференциальные доказательства

Для каждого supported construct нужны два независимых доказательства:

1. pinned jsonRVM oracle фиксирует observable behavior source fixture;
2. AVM test проходит полный migrated pipeline и сравнивает semantic result/context/order/failure meaning.

Совместимость не выводится из визуального сходства JSON.

Примеры:

```text
CASE-ARITHMETIC
jsonRVM -> 2
AVM -> canonical Integer(2)

CASE-PURE-RELATION-COMPOSITION
jsonRVM -> 5
AVM -> Integer(5), final semantic relation-state = Integer(5)

CASE-FOREACH-CONTEXT
jsonRVM -> [1,2,3]
AVM -> canonical ordered Integer list [1,2,3], parent semantic unchanged
```

## Следующие gates

После #199:

1. `CASE-BOOLEAN-BRANCH` через existing canonical lazy Boolean/control runtime, если его contract уже полностью соответствует frozen evidence;
2. `CASE-MISSING-REFERENCE` как первый frozen failure migration slice;
3. additional addressing/where/view/defaults только evidence-driven;
4. host effects только после #129 capability/effect boundary.

Не поддержанные constructs не получают заглушек и не исполняются частично.

## Архитектурные запреты

1. migrator не принимает `LinkStore`;
2. migrator не вызывает `Executor`;
3. migrator не регистрирует native relations;
4. migrator не хранит mutable/global current context;
5. legacy JSON не становится runtime value universe AVM;
6. Native Duplet parser не получает legacy-special cases;
7. frozen jsonRVM не изменяется;
8. unsupported construct отвергается вместо fallback к старому interpreter;
9. compatibility state transition не прячется в pure Integer/Text primitives;
10. `$ref/$rel/$obj` strings не попадают в canonical core;
11. generic legacy `=` не выводится из одного foreach fixture;
12. каждый новый supported construct получает oracle-backed conformance.
