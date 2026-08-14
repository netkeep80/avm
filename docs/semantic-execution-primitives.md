# Явные semantic execution primitives AVM

Родительский migration gate: #197. Epic: #122/#174. Semantic context foundation: #125/#153/#162. Reference algebra: #126. Value denotation: #128.

## Назначение

Этот слой выражает три операции, которых не хватало для переноса stateful composition старого `jsonRVM`, не возвращая mutable `vm_ctx` в AVM:

```text
commit_relation_state
resolve_reference_relation
apply_pure_relation
```

Они являются обычными relation identities, зарегистрированными в **том же** `Executor`, что и остальные AVM relations.

Главный инвариант:

```text
value computation
    !=
semantic state transition
```

Pure Integer arithmetic после #192 возвращает только canonical result. Если этот result должен стать новым semantic `$rel`, это делает отдельная явная relation.

## Отдельный vocabulary

`SemanticExecutionVocabulary` не расширяет `BootstrapVocabulary`.

Это важно для persistence compatibility: добавление migration semantics не создаёт новую bootstrap-generation и не заставляет старые stores скрыто materialize-ить новые bootstrap identities при open.

Vocabulary содержит три independent identities:

```text
commit_relation_state
resolve_reference_relation
apply_pure_relation
```

Caller создаёт или восстанавливает их явно так же, как `IntegerVocabulary`, `ReferenceVocabulary` и другие domain-specific vocabularies.

## `commit_relation_state`

Canonical executable shape:

```text
(commit_relation_state, unit, value_expression)
```

Handler требует `SemanticContextView` и выполняет `value_expression` в том же semantic context.

Source expression обязано быть state-neutral:

```text
child.semantic == input.semantic
```

После validated result происходит единственный transition:

```text
ExecutionOutcome {
    result = child.result,
    semantic = input.with_relation_state(child.result)
}
```

То есть меняется relation-state **текущей caller frame**. Child lineage не может случайно заменить parent context.

Это structural replacement старого присваивания в mutable slot:

```text
$rel := result
```

но без C++ alias/lvalue.

## `resolve_reference_relation`

Executable shape:

```text
(resolve_reference_relation, unit, canonical_reference)
```

Relation не вводит новую reference semantics. Она является только execution bridge к уже принятому #126 API:

```text
resolve_reference(
    LinkStore,
    ReferenceVocabulary,
    reference,
    SemanticContextView)
```

Properties:

- reference lookup observational;
- result — существующий `LinkId`;
- semantic state unchanged;
- unresolved reference -> deterministic handler failure;
- `intern()` / `create_point()` отсутствуют в lookup path.

Для legacy `$ref:"$rel"` semantic migrator использует canonical:

```text
Role(Current, RelationState)
```

а не controller identity `ExecutionContext.relation`.

## `apply_pure_relation`

Canonical shape:

```text
(apply_pure_relation,
 target_relation,
 Link(subject_expression, object_expression))
```

Это не второй evaluator. Relation только собирает dynamic operands и затем вызывает обычный canonical target entity через тот же `Executor`.

### Operand phase

Оба operand expressions выполняются в caller semantic context.

Каждый operand обязан быть state-neutral. Если operand пытается изменить semantic state, pure application отвергается. Это запрещает скрытый порядок state effects внутри «чистой» relation form.

### Canonical target entity

После получения values materialize-ится или переиспользуется:

```text
RelationEntity(target_relation, subject_value, object_value)
```

через существующий Relations Model codec.

Это **explicit execution-time materialization effect**: dynamic operand может быть известен только после reference resolution.

Никакого synthetic manual dispatch в обход RelationEntity нет.

### Child semantic context

Target получает child semantic frame:

```text
entity         = apply wrapper entity
relation_state = caller relation_state
subject        = resolved subject value
object         = resolved object value
parent         = caller semantic context
```

Таким образом relation form имеет meaningful `sub/obj`, а controller target relation остаётся `ExecutionContext.relation` вложенного dispatch.

### Purity boundary

Target handler должен вернуть тот же child semantic state:

```text
target_outcome.semantic == target_child_context
```

Если target relation меняет semantic state, `apply_pure_relation` отвергает такой вызов. Для stateful/effectful relation понадобится отдельный будущий contract; нельзя незаметно расширять pure primitive.

Успешный pure target result возвращается в caller context без semantic transition:

```text
ExecutionOutcome {
    result = target.result,
    semantic = caller.semantic
}
```

Если result должен стать `$rel`, внешний `commit_relation_state` делает это явно.

## Legacy sequence composition

Existing `BootstrapRuntime::sequence_relation` уже использует:

```text
execute_same_context_sequence
```

и thread-ит `ExecutionOutcome.semantic` между children.

Поэтому frozen:

```json
{"$rel/result":[1,2,3]}
```

мигрирует концептуально в:

```text
sequence([
  commit(quote(Integer(1))),
  commit(quote(Integer(2))),
  commit(quote(Integer(3)))
])
```

Final result = Integer(3), final relation-state = тот же LinkId.

## Frozen pure relation composition

Legacy:

```json
{
  "$rel/result": [
    {"$rel":"+", "$sub":1, "$obj":1},
    {"$rel":"+", "$sub":{"$ref":"$rel"}, "$obj":3}
  ]
}
```

компилируется как:

```text
sequence([
  commit(
    apply_pure_relation(
      integer_add,
      quote(Integer(1)),
      quote(Integer(1)))),

  commit(
    apply_pure_relation(
      integer_add,
      resolve(Current.RelationState),
      quote(Integer(3))))
])
```

Execution:

```text
initial $rel = 0

1 + 1 -> 2
commit -> $rel = 2

resolve($rel) -> 2
2 + 3 -> 5
commit -> $rel = 5

result = 5
```

Это воспроизводит observable frozen oracle, не меняя pure Integer contract.

## Materialization accounting

Нужно различать фазы:

```text
Native projection/find
    -> no writes

explicit realize static program
    -> writes canonical program structure

execute apply_pure_relation
    -> may first materialize dynamic target RelationEntity
    -> target value relation may materialize canonical result value

repeat after convergence
    -> no further growth for identical deterministic input
```

Reference resolution сама по себе остаётся read-only.

## Failure boundary

Deterministic failures включают:

- semantic context отсутствует;
- wrong `unit` subject для commit/resolve;
- unresolved reference;
- target relation отсутствует/не зарегистрирована;
- stateful operand inside pure application;
- stateful target inside pure application.

Эти ошибки не превращаются в `nil` и не вызывают legacy fallback.

Явные effects, завершившиеся до более поздней failure, не откатываются: этот слой не является транзакцией.

## Persistence

Vocabularies и static program identities caller сохраняет явно.

После convergence одного `PersistentLinkStore`:

1. store закрывается;
2. тот же store открывается;
3. exact Bootstrap/Integer/Reference/SemanticExecution vocabularies восстанавливаются;
4. handlers регистрируются заново как host execution capability;
5. existing root `LinkId` исполняется без reparsing/remigration;
6. exact result identity и final semantic state сохраняются;
7. repeated execution не увеличивает store.

## Что намеренно не входит

- generic mutable variable assignment;
- arbitrary JSON lvalue/path semantics;
- `$ent/$sub/$obj/$$...` migration beyond already canonical reference algebra;
- stateful/effectful dynamic relation application;
- host capability/effect vocabulary;
- parallel execution;
- second Executor.

Эти primitives являются минимальным structural state-composition layer для #197 и должны расширяться только по frozen evidence.