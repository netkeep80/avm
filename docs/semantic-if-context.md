# Контекстно-корректный ленивый If

Этот документ фиксирует semantic contract `BootstrapVocabulary::if_relation` после обнаружения context-loss дефекта при переносе `CASE-BOOLEAN-BRANCH` из pinned jsonRVM oracle.

## Проблема старого canonical handler

До этого изменения canonical If уже был ленивым: сначала вычислял condition, затем исполнял только выбранную ветку. Но nested expressions запускались через context-free:

```text
Executor::execute(...)
```

Поэтому при вызове If внутри `SemanticContextView` condition и выбранная branch теряли semantic context.

Особенно это ломало canonical expression:

```text
if(resolve_reference(Current.RelationState), then, else)
```

потому что execution bridge reference resolver требует semantic context.

Это defect самого canonical control relation, а не особенность jsonRVM frontend. Поэтому исправление сделано в `BootstrapRuntime`, а semantic migrator только использует исправленный If.

## Контракт без semantic context

Для обычного context-free execution поведение сохраняется:

```text
evaluate condition
-> lookup canonical Boolean selector
-> execute exactly one selected branch
-> return selected branch result
```

Существующие AVM 1.0 Boolean/laziness scenarios не меняются.

## Контракт с semantic context

Если `ExecutionContext` содержит `SemanticContextView`, If действует композиционно:

```text
condition_outcome = execute(condition, input_semantic)
selector = Boolean(condition_outcome.result)
branch_outcome = execute(selected_branch, condition_outcome.semantic)
return branch_outcome
```

Нормативные свойства:

1. condition получает текущий semantic context;
2. condition может вернуть explicit изменённый `ExecutionOutcome.semantic`;
3. именно этот explicit state передаётся выбранной branch;
4. невыбранная branch не исполняется;
5. результат и semantic state выбранной branch возвращаются целиком;
6. входной `SemanticContextView` остаётся immutable;
7. If сам не вводит скрытый `$rel := result`.

Иными словами:

```text
control composition
    !=
hidden semantic mutation
```

## Почему stateful condition разрешён

If не требует state-neutral condition. Это важно для композиционности AVM.

Если condition сама является explicit stateful expression, её подтверждённый `ExecutionOutcome.semantic` должен быть виден выбранной branch так же, как `sequence_relation` thread-ит explicit outcomes между шагами.

Запрещать это только ради одного migration fixture означало бы создать special-case control semantics.

## Ленивость остаётся обязательной

Unselected branch не должна:

- получать `Enter` observer event;
- materialize runtime effects;
- падать из-за неизвестной relation;
- менять semantic state.

Focused core test использует intentionally failing unselected branch и bounded trace, чтобы это было executable invariant, а не только документация.

## Перенос frozen Boolean-сценария

Pinned source:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
```

Frozen fixture:

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

Legacy sequence сначала устанавливает current `$rel = true`, затем historical relation выбирает:

```text
true  -> object branch = 42
false -> subject branch = 13
```

Semantic compiler строит canonical program концептуально так:

```text
sequence([
  commit_relation_state(quote(Boolean(true))),
  commit_relation_state(
    if(
      resolve_reference(Current.RelationState),
      quote(Integer(42)),
      quote(Integer(13))))
])
```

После projection/realize runtime видит только canonical LinkIds и обычный `if_relation`.

## Граница frontend symbols

Generated `duplet-json/1` использует caller-owned transport anchors:

```text
bootstrap_true
bootstrap_quote
bootstrap_sequence
bootstrap_if
semantic_commit_relation_state
semantic_resolve_reference
current_relation_state_reference
```

Эти строки существуют только на frontend resolver boundary. Executor dispatch остаётся LinkId-based.

Native Duplet parser не знает legacy names `if_rel_then_obj_else_sub`, `$rel` или jsonRVM control syntax.

## Узкий compatibility scope

Первый Boolean slice поддерживает только evidence-backed форму:

- top-level executable sequence;
- первый step — literal `true`;
- второй step — exact `if_rel_then_obj_else_sub`;
- `$sub` и `$obj` — Integer branches;
- true выбирает object, false выбирает subject.

Не объявляются jsonRVM-compatible без отдельного frozen evidence:

- raw standalone Boolean program;
- `false` sequence fixture;
- `if_rel_then_sub_else_obj`;
- другие historical `if_*` relations;
- nested branch expressions.

Canonical AVM core может поддерживать более общий If, но frontend compatibility claims остаются evidence-driven.

## Запреты архитектуры

Этот gate не добавляет:

- `IfExecutor`;
- migrator-side branch execution;
- eager evaluation обеих branches;
- string dispatch в Executor;
- host mutable variable для `$rel`;
- raw JSON Boolean как internal AVM value;
- hidden state mutation внутри `if_relation`;
- legacy interpreter fallback.

## Проверяемые инварианты

Core conformance проверяет:

- старый context-free lazy If;
- Current.RelationState condition внутри semantic context;
- отсутствие Enter для unselected branch;
- threading explicit stateful condition outcome;
- propagation explicit selected-branch outcome;
- immutable input context;
- repeated converged execution without growth.

Differential migrator conformance дополнительно проверяет:

- уничтожение source DOM до projection;
- non-mutating projection/find;
- explicit realization;
- structural shape двух commits и canonical If;
- condition = Current.RelationState reference;
- branch order `42/13`;
- final canonical Integer result `42`;
- final semantic relation-state = result;
- unselected `13` branch не исполняется;
- unsupported nearby legacy forms reject на migration boundary.