# Семантический мигратор jsonRVM → AVM

Родительская задача: #174. Epic: #122. Native Duplet JSON umbrella: #169.

## Назначение

Semantic migrator переносит **наблюдаемую семантику** выбранного frozen subset legacy `jsonRVM` в уже принятые link-native contracts AVM.

Он принципиально отличается от structural converter `json_duplet_converter.h`.

Structural converter отвечает только за механическую форму:

```text
(rel, sub, obj) -> Link(rel, Link(sub, obj))
```

и не знает специальных runtime значений `$rel`, `$sub`, `$obj`, `$ref`, sequence, foreach, Boolean control или legacy mutable context.

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

## Текущий frozen subset

| Frozen case | Legacy observation | Canonical AVM mapping | Статус |
|---|---|---|---|
| `CASE-ARITHMETIC` | `1 + 1 -> 2` | direct canonical Integer relation | ✅ #195/#196 |
| `CASE-SEQUENCE-ORDER` | `[1,2,3] -> 3` | sequence + explicit relation-state commits | ✅ #197/#198 |
| `CASE-PURE-RELATION-COMPOSITION` | `1+1; $rel+3 -> 5` | canonical reference + pure relation application + explicit commits | ✅ #197/#198 |
| `CASE-FOREACH-CONTEXT` | `foreachobj [1,2,3] -> [1,2,3]` | existing foreach + `Current.Object` | ✅ #199/#200 |
| `CASE-BOOLEAN-BRANCH` | `true; if -> 42` | context-preserving canonical lazy If + explicit commits | ✅ #202/#203 |
| `CASE-MISSING-REFERENCE` | deterministic failure | typed unresolved textual reference before materialization | 🚧 #205 |

Эта таблица означает **доказанный semantic migration corpus**, а не полную совместимость со всем историческим `base.rm.h`.

## Арифметика — #196

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

Expression компилируется в direct canonical relation:

```text
(integer_add, Integer(1), Integer(1))
```

Поддержанный frozen arithmetic vocabulary:

```text
+ -> integer_add
- -> integer_subtract
* -> integer_multiply
/ -> integer_divide
```

Pure Integer arithmetic возвращает value и не выполняет скрытое:

```text
$rel := result
```

Внешний `$rel/result` — differential-output metadata, а не generic mutable Object/Map runtime AVM.

## Sequence и явный relation-state — #198

Legacy executable array компилируется в existing canonical sequence.

Stateful compatibility выражается отдельными frontend-neutral relations:

```text
commit_relation_state
resolve_reference_relation
apply_pure_relation
```

Frozen composition:

```text
1 + 1
$rel + 3
```

становится концептуально:

```text
apply(integer_add, quote(1), quote(1)) -> 2
commit -> semantic $rel = 2
resolve(Current.RelationState) -> 2
apply(integer_add, 2, quote(3)) -> 5
commit -> semantic $rel = 5
```

Сохраняется главный инвариант:

```text
value computation != semantic state transition
```

`$ref:"$rel"` заканчивает существование на legacy compiler boundary. Generated program содержит canonical `ReferenceRole::RelationState(Current)` identity.

## Foreach object context — #200

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

Canonical migration:

```text
(foreach_object,
    resolve_reference(Current.Object),
    [Integer(1), Integer(2), Integer(3)])
```

Новый foreach runtime не создавался: используется `ForeachVocabulary::object_relation` из #187/#189.

Доказаны:

- ordered canonical collection;
- fresh sibling child contexts;
- `child.object = item`;
- no state bleed между sibling iterations;
- parent semantic state unchanged;
- canonical ordered result `[1,2,3]`;
- convergence/no-growth.

Exact historical body

```json
{"$rel":"=","$obj":{"$ref":"$obj"}}
```

трактуется только как evidence-backed identity projection `Current.Object`. Generic legacy `=` из одного fixture не выводится.

## Булевое управление — #203

Frozen source:

```json
{
  "$rel/result": [
    true,
    {
      "$obj": 42,
      "$rel": "if_rel_then_obj_else_sub",
      "$sub": 13
    }
  ]
}
```

Oracle:

```text
/result = 42
```

Во время migration был обнаружен реальный gap canonical core: existing `if_relation` был lazy, но nested condition/branch выполнялись context-free и теряли `SemanticContextView`.

#203 сохранил один `if_relation` и один Executor, но сделал semantic path compositional:

```text
condition_outcome = execute(condition, input_semantic)
selector = Boolean(condition_outcome.result)
branch_outcome = execute(only selected branch, condition_outcome.semantic)
return branch_outcome
```

Context-free behavior осталось прежним; If сам не вводит скрытый state transition.

Frozen relation компилируется в:

```text
sequence([
  commit_relation_state(quote(canonical true)),
  commit_relation_state(
    if(
      resolve(Current.RelationState),
      quote(Integer(42)),
      quote(Integer(13))))
])
```

Historical branch orientation сохраняется:

```text
true  -> object = 42
false -> subject = 13
```

Raw legacy `true` поддержан только в frozen evidence-backed executable context. Canonical AVM `false_value` существует и тестируется независимо, но raw legacy `false` не объявляется совместимым без отдельного oracle evidence.

## Missing reference failure — #205

Frozen source:

```json
{
  "$rel/result": {
    "$ref": "__avm_missing_reference_oracle__"
  }
}
```

Legacy oracle доказывает только semantic observation:

```text
requested textual reference cannot be resolved
```

и наличие diagnostic, содержащего source marker. Exact exception/JSON wording не является contract.

### Почему нельзя создавать fake LinkId

Canonical #126 `Named` reference содержит `LinkId`, а не строку. Для неизвестного source name semantic adapter не имеет корректной canonical identity.

Запрещён искусственный путь:

```text
unknown textual name
 -> create_point()
 -> Named(fake LinkId)
 -> runtime miss
```

Он нарушил бы фундаментальную границу:

```text
find / resolve / verify != realize / write
```

и создал бы semantic identity только ради представления отсутствия.

### Принятый failure boundary

Для frozen case используется typed adapter failure **до** Native document/projection/execution:

```text
legacy {$ref:"__avm_missing_reference_oracle__"}
 -> semantic adapter recognizes unresolved textual reference
 -> MigrationFailureKind::UnresolvedReference
 -> structured source path + source identity
 -> no ProjectionDescription realization
 -> no Executor
 -> LinkStore unchanged
```

`MigrationError` теперь несёт:

```text
kind
source_path
source_identity
human-readable what()
```

Обычные malformed/unsupported source forms сохраняют `MigrationFailureKind::InvalidSource`.

Differential test сравнивает typed category и structured marker, а не полный `what()`.

### Unknown source name и absent LinkId — разные случаи

Нужно различать:

```text
A. unknown textual legacy name
   -> frontend/migration name-resolution failure

B. caller already supplied canonical LinkId anchor,
   но anchor отсутствует в prepared store
   -> canonical find/realize miss/reject из #126
```

#205 не превращает A в B через synthetic identity.

### Почему пока нет generic name resolver API

Frozen corpus содержит только intentional missing-name failure и не содержит доказанного success case произвольного named reference.

Поэтому #205 не вводит преждевременный global/callback resolver surface. Когда появится реальный success fixture/consumer, caller-owned textual-name resolver можно добавить evidence-driven, всё так же без доступа migrator-а к LinkStore и без hidden DB lookup.

## Символьные anchors

Generated `duplet-json/1` success documents могут содержать frontend transport anchors, например:

```text
integer_add
bootstrap_unit
bootstrap_nil
bootstrap_true
bootstrap_quote
bootstrap_sequence
bootstrap_if
semantic_commit_relation_state
semantic_resolve_reference
semantic_apply_pure_relation
current_relation_state_reference
current_object_reference
foreach_object
```

Caller связывает их с уже существующими LinkIds prepared runtime. Migrator не создаёт global symbol registry.

Unknown textual **legacy references** не маскируются под `$symbol`.

## Метаданные наблюдаемого результата

Для success case `MigrationResult` содержит:

```text
document
observable_json_pointer
```

`observable_json_pointer` — differential-harness metadata и не materialize-ится в LinkStore.

Failure #205 не обязан создавать `MigrationResult`: unresolved source reference прекращает migration до Native document.

## Границы materialization

Нормативный pipeline:

```text
migrate legacy JSON
    -> no LinkStore

project Native JSON
    -> no LinkStore mutation

find_projection
    -> observation only

realize_projection
    -> explicit static materialization

execute
    -> только явные effects canonical relations
```

Для unresolved textual reference pipeline заканчивается на первом шаге:

```text
migration failure -> no projection -> no realization -> no execution
```

Differential test специально фиксирует `store.size()` до и после failure.

## Граница времени жизни

После успешного `migrate_program` исходный legacy DOM больше не нужен. Native document является самостоятельным artifact.

Для failure case structured diagnostic копирует source path/identity и также не зависит от lifetime input DOM после throw/catch boundary.

## Ошибки migration boundary

Migrator детерминированно отвергает неподдержанные или неоднозначные формы до projection/materialization.

Среди закреплённых veto cases:

- non-object root;
- root без exact `$rel/result` envelope;
- дополнительные root members;
- malformed/unknown arithmetic;
- non-integer arithmetic operand;
- unsupported sequence reference role;
- empty executable sequence;
- raw legacy `false` без frozen evidence;
- legacy `foreachsub` без evidence;
- modified/generic `=` foreach body;
- non-Integer/empty foreach collection первого slice;
- unknown/modified conditional relation;
- missing/extra Boolean relation fields;
- non-Integer Boolean branch values первого slice;
- malformed top-level `$ref`;
- frozen unresolved textual reference — отдельный typed `UnresolvedReference`.

## Дифференциальные доказательства

Для success constructs сравниваются decoded semantic values/context/order, а не source JSON formatting или cross-store numeric LinkIds.

Для frozen failure сравниваются:

```text
failure kind
source identity/path
absence of hidden materialization
```

Не сравниваются literal host exception strings.

Текущий minimal corpus:

```text
CASE-ARITHMETIC                -> 2
CASE-SEQUENCE-ORDER            -> 3
CASE-PURE-RELATION-COMPOSITION -> 5
CASE-FOREACH-CONTEXT           -> [1,2,3]
CASE-BOOLEAN-BRANCH            -> 42
CASE-MISSING-REFERENCE         -> typed unresolved-reference failure
```

## Дальнейший план

После #205 минимальный frozen corpus #123 считается закрытым.

Следующий приоритет:

1. #131 — собрать AVM 1.5 end-to-end release proof, включая persistent reopen без remigration;
2. #130 — common-denotation JSON/Anum equivalence на реально общем subset;
3. #129 — capability/effect boundary до переноса первого FS/HTTP/time/database/native effect;
4. дополнительные legacy constructs — только по реальному consumer/use-case и frozen evidence.

Не следует механически портировать весь `base.rm.h` operator-by-operator.

## Архитектурные запреты

1. migrator не принимает `LinkStore`;
2. migrator не вызывает `Executor`;
3. migrator не регистрирует native relations;
4. migrator не хранит mutable/global current context;
5. legacy JSON не становится runtime value universe AVM;
6. Native Duplet parser не получает legacy-special cases;
7. frozen jsonRVM не изменяется;
8. unsupported construct отвергается вместо fallback к old interpreter;
9. state transition не прячется в pure Integer/Text/If primitives;
10. `$ref/$rel/$obj` strings не попадают в canonical core;
11. generic legacy `=` не выводится из одного foreach fixture;
12. legacy Boolean relation name не становится runtime opcode;
13. unselected If branch не исполняется;
14. unknown textual reference не создаёт fake LinkId;
15. каждый новый supported construct получает oracle-backed conformance.
