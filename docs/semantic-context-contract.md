# Контракт семантического контекста AVM

Родительская задача: #125. ADR-задача: #144. Epic: #122.

## Статус

Этот документ определяет границу между техническим шагом `Executor` и контекстом Модели Отношений, относительно которого имеют смысл legacy-местоимения `$ent`, `$rel`, `$sub`, `$obj` и переход к вышестоящему контексту `$`.

Он является продолжением triune contract #124 и **не** возвращает mutable JSON references старого `jsonRVM`.

Главное различие:

```text
ExecutionStepContext != SemanticContextState
```

и отдельно:

```text
controller relation identity != semantic relation-state ($rel)
```

Это различие является обязательным compatibility contract, а не деталями реализации.

## Источники legacy semantics

Контракт извлечён из pinned runtime:

```text
netkeep80/jsonRVM
843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Ключевые источники:

```text
modules/common/include/vm.rm.h
modules/console/include/base.rm.h
doc/Dictionary.md
compat/jsonrvm-oracle-golden.json
```

Особенно важен frozen `CASE-PURE-RELATION-COMPOSITION`: первый `+` производит `2`, второй `+` читает предыдущий `$rel` и получает `5`. Следовательно `$rel` observable как текущее состояние отношения, а не как identity controller-а.

## Два слоя контекста

### Контекст шага исполнения

Существующий `ExecutionContext` описывает конкретный dispatch:

```text
entity
relation      — identity native/link-native controller-а
subject
object
parent        — identity parent execution entity для текущего tooling path
frame         — call-frame identity, если есть
```

Он отвечает на вопрос:

> какая структурная entity прямо сейчас передана конкретному handler-у?

Этот context нужен:

- `Executor`;
- observer events;
- execution trace;
- diagnostics dispatch/result-validation;
- существующему call runtime.

Создание нового `ExecutionContext` **не означает** автоматически создание нового семантического контекста Модели Отношений.

### Семантический контекст Модели Отношений

Program-visible context отвечает на другой вопрос:

> в каком субъективном состоянии Модели Отношений сейчас проявляется исполняемая структура?

Его логические поля:

```text
SemanticContextFrame
{
    entity
    relation_state
    subject
    object
}
```

и lineage к вышестоящему `SemanticContextFrame`.

Здесь:

- `entity` — сущность, проекцией/проявлением которой является context;
- `relation_state` — текущее состояние отношения, legacy `$rel`;
- `subject` — legacy `$sub`;
- `object` — legacy `$obj`;
- parent — вышестоящий semantic context, legacy `$`.

Controller relation identity из `ExecutionContext.relation` в этот набор **не входит** автоматически.

## Почему `$rel` не является `ExecutionContext.relation`

При обработке JSON relation-form старый `vm::exec_ent` выполняет концептуально:

```text
controller = entity["$rel"]

child.rel = parent.rel
child.obj = explicit $obj или parent.rel
child.sub = explicit $sub или parent.rel
child.ent = relation-form entity
child.parent = parent

execute(controller, child)
```

То есть controller выбирается **из** relation-form, а `child.rel` остаётся состоянием/выходным slot контекста.

`doc/Dictionary.md` прямо определяет «Контекстное отношение» как текущее состояние отношения контекста исполнения.

Поэтому mapping:

```text
$rel -> ExecutionContext.relation
```

запрещён.

Для controller identity при необходимости используется отдельное понятие execution-step relation/controller.

## Immutable state вместо mutable aliases

Legacy `vm_ctx` хранит C++ references и допускает:

```text
$.rel = value
$.sub = value
```

AVM не переносит эту aliasing model.

Канонический принцип:

```text
старое состояние + semantic operation
-> result + новое состояние
```

Существующий `SemanticContextFrame` не мутируется. Изменение relation-state создаёт новое логическое состояние той же semantic frame.

Концептуальный результат context-aware execution:

```text
SemanticOutcome
{
    result
    next_context_state
}
```

`result` и `next_context_state.relation_state` не обязаны совпадать для всех relations.

Это продолжает #124:

```text
controller identity
returned result
semantic relation-state
```

являются разными понятиями.

## Relation-state transition является явной семантикой

Нельзя ввести общее правило:

```text
relation_state := every Executor result
```

Legacy vocabulary обновляет slots по-разному:

- arithmetic/comparison записывают в `$.rel`;
- `jsonCopy`, `jsonSqrt`, `jsonSize` и ряд преобразований пишут в `$.sub`;
- `jsonUnion` меняет `$.sub`, затем копирует его в `$.rel`;
- `jsonNull` не меняет ничего;
- control relations могут выполнить continuation в другом context;
- `catch` превращает exception-value в relation-state перед handler-ом.

Поэтому каждый будущий semantic relation contract должен явно принадлежать одному или нескольким классам перехода, например:

```text
ReturnOnly
SetRelationState
ProduceSubjectValue
SetRelationStateAndReturn
EnterChildContext
ResumeParentContext
ProjectedContext
ExternalEffect
```

Названия C++ enum могут отличаться. Нормативно важно отсутствие неявного универсального присваивания.

## Корневой контекст

Legacy root `vm_ctx` содержит self-parent:

```text
root.$ = root
```

`Dictionary.md` также определяет корневой context как context, для которого вышестоящим является он сам.

AVM сохраняет observable rule:

```text
parent(root) = root
parent(parent(root)) = root
```

При этом internal representation не обязана содержать физический цикл.

Предпочтительное представление:

```text
root.parent = none
```

а semantic accessor реализует saturation:

```text
parent(root_view) -> root_view
```

Это сохраняет старую семантику и исключает:

- cyclic C++ ownership;
- recursive link materialization;
- специальные бесконечные structures в persistent backend.

## Ephemeral lineage

Semantic lineage существует только столько, сколько требуется execution.

Нормативные свойства:

1. lineage immutable с точки зрения уже созданного snapshot;
2. child context получает ссылочно/структурно разделяемый immutable parent snapshot;
3. получение parent не читает `LinkStore` и не пишет в него;
4. arbitrary-depth traversal не зависит от hard-coded числа `$`;
5. один и тот же program entity может иметь несколько разных dynamic semantic contexts;
6. C++ representation не является semantic identity.

Допустимая реализация может использовать persistent/shared chain или другой cheap immutable value mechanism.

Запрещено делать program-visible identity равной адресу node/`shared_ptr`/stack object.

## Почему `ExecutionContext.parent` недостаточен

Текущий `ExecutionContext.parent` хранит immediate parent **entity identity**, необходимую observer/trace path.

Из неё нельзя восстановить semantic lineage:

- parent entity не содержит dynamic parent своего execution;
- recursive invocation одной entity создаёт разные dynamic histories;
- sequence может иметь несколько dispatch steps внутри одного semantic context;
- `if`, `switch`, `foreach`, `view`, `catch` используют parent routing, не совпадающий с обычной рекурсией `execute()`;
- function call-frame chain отражает lexical/call bindings, а не все виды semantic nesting.

Поэтому call frame и semantic lineage существуют рядом, но не дублируют друг друга.

## Классификация legacy переходов

### Простое значение/native relation

Compiled native relation исполняется в переданном `vm_ctx`.

Класс:

```text
same semantic context
```

Operation может вернуть value и/или определить явный state transition согласно своему relation contract.

### String reference

Legacy:

```text
exec_ent(current_ctx, resolved_value)
```

Класс:

```text
same semantic context
```

Reference resolution само по себе не создаёт child context.

### Object `$ref`

После resolution relation/program выполняется в том же `vm_ctx`.

Класс:

```text
same semantic context
```

Frontend syntax `$ref` не является context boundary.

### Lambda-vector / ordered sequence

Legacy array выполняет каждый child:

```text
exec_ent(current_ctx, child)
```

Класс:

```text
same semantic context + sequential state threading
```

Следующий child видит relation-state, полученный после предыдущих children. Именно эта семантика нужна frozen pure-composition case.

Следствие для AVM: `sequence_relation` не должна трактовать каждый вызов `Executor::execute(child)` как новый semantic context только потому, что появился новый dispatch-step.

### Relation-form `$rel`

Legacy relation-form создаёт новый `vm_ctx`.

Класс:

```text
child semantic context
```

Child получает:

```text
entity = relation-form entity
parent = current semantic context
relation_state = current relation_state
object = explicit object или current relation_state
subject = explicit subject или current relation-state destination semantics
```

Controller выполняется **внутри child context**.

Точная lvalue/default-sub propagation переносится в #126/#128 как explicit reference/state-transition semantics, а не mutable alias.

### Boolean `if/then/else`

Controller relation уже выполняется внутри relation-form child context `C`.

Legacy selected branch запускается через:

```text
exec_ent(C.parent, selected_branch)
```

Класс:

```text
resume/execute in parent semantic context
```

Не создаётся ещё один semantic child только ради branch dispatch.

### `while`

Body повторно выполняется в parent semantic context controller-а:

```text
exec_ent(C.parent, body)
```

Класс:

```text
parent-context continuation + repeated state threading
```

Termination/effect policy относится к #127.

### Typed `switch`

Выбранная branch также выполняется через parent context controller-а.

Класс:

```text
parent-context continuation
```

JSON object key selection позже заменяется canonical value/key model #128.

### `view`

Legacy `jsonView` строит отдельный context из значений parent context и исполняет object/model внутри него.

Класс:

```text
projected context derived from parent
```

Это не обычный `child(current)` и не простой `parent(current)`.

#125 фиксирует routing class; точная manifestation/view semantics остаётся совместной задачей #126/#127.

### `foreachobj` / `foreachsub`

Каждый элемент получает отдельный projection context. В legacy его parent равен `current.parent`, а не текущему controller context.

Класс:

```text
projected sibling-like context derived from parent
```

`foreachobj` frozen oracle уже подтверждает child object propagation. `foreachsub` остаётся отдельным case.

### `where`

Predicate выполняется в per-item context с тем же характерным routing к `current.parent`.

Класс:

```text
projected sibling-like context derived from parent
```

Historical doctest `CASE-WHERE-FILTER` является frozen semantic evidence collection behavior.

### `catch`

Try-body выполняется в новом context, производном от parent controller-а. При exception legacy записывает error-value в relation-state и handler также запускается в новом context того же parent уровня.

Класс:

```text
projected parent-derived context
+ explicit error/relation-state transition
```

Host C++ exception representation не является частью semantic contract.

### Lambda-structure parallel projection

Legacy object без `$rel/$ref` создаёт отдельные call contexts и использует parallel execution policy. Parent routing также не совпадает с простым child(current).

Класс:

```text
projected contexts / deferred scheduler semantics
```

До #127 нельзя переносить automatic parallelism. ADR сохраняет только факт, что это отдельные projected contexts.

### Rendering `tag/xml/html`

Legacy rendering использует ещё более специфичное routing через `$.$.$` для вложенного content execution.

Класс:

```text
projection/frontend-specific context routing — defer
```

Не используется как основание общего context kernel. Возврат rendering semantics возможен только после text/projection model #128.

## Текущие AVM functions и call frames

Current AVM function runtime является более поздним link-native механизмом и не должен автоматически отождествляться с legacy `vm_ctx`.

Call frame отвечает за:

```text
function identity
formal -> actual bindings
parent call frame
```

Semantic context отвечает за:

```text
ent / relation-state / sub / obj / upper semantic context
```

Первый context-aware function implementation должен явно решить, создаёт ли call новый semantic context или выполняет body в переданном. До такого решения call-frame parent нельзя использовать для `$` traversal.

## Контекстная ссылка без materialization instance

Для чтения `$ent`, `$$obj`, `$$$rel` программе не нужен persistent `LinkId` динамического context instance.

Вместо этого используется structural selector algebra:

```text
ContextSelector := Current
                 | Parent(ContextSelector)

ContextRole := Entity
             | RelationState
             | Subject
             | Object

ContextRoleRef := Role(ContextSelector, ContextRole)
```

Примеры:

```text
$ent    -> Role(Current, Entity)
$rel    -> Role(Current, RelationState)
$$obj   -> Role(Parent(Current), Object)
$$$sub  -> Role(Parent(Parent(Current)), Subject)
```

Selector является link-native **описанием вычисления ссылки**. Dynamic context остаётся ephemeral.

Evaluator selector-а:

- читает только semantic lineage;
- возвращает существующий role `LinkId`;
- не вызывает `intern()`;
- на root parent traversal saturates к root;
- не содержит строк `$ent`/`$$` внутри execution kernel.

Текстовый compiler syntax остаётся #126.

## Почему автоматическая materialization context instance откладывается

Ни один текущий acceptance case `$...` не требует сохранять dynamic context после завершения execution.

Автоматическое создание context links на каждом boundary привело бы к:

- observable росту store от pure execution;
- необходимости canonicalize dynamic execution histories;
- большим persistent traces, которые не являются программными данными;
- смешению runtime stack и semantic model.

Поэтому #125 использует правило:

```text
observe context != realize context
```

Canonical context-instance denotation **не входит в первый implementation slice**.

Если позже появится реальный consumer, которому нужно сохранить context как data, добавляется отдельная explicit `realize_context` operation с собственным representation/conformance contract.

## Observer и единый источник истины

Observer не должен вычислять semantic context повторно из dispatch parent IDs.

После внедрения context-aware execution один execution-step должен содержать/ссылаться на тот же immutable `SemanticContextView`, который использует program-visible context selector evaluator.

Тогда:

```text
observer semantic view == pronoun evaluator semantic view
```

а существующие поля `ExecutionContext.entity/relation/subject/object` продолжают описывать dispatch entity/controller roles согласно #124.

Trace serialization может показывать оба слоя явно, но не смешивать их.

## Минимальный implementation boundary после ADR

Первый кодовый slice #125 должен реализовать только:

1. immutable `SemanticContextFrame/State/View`;
2. root saturation;
3. arbitrary parent traversal;
4. явное создание child semantic context;
5. same-context forwarding без создания нового frame;
6. separate controller identity и relation-state;
7. observational context-role lookup API;
8. интеграцию одного источника semantic view в `ExecutionContext`/observer;
9. conformance tests, доказывающие отсутствие `LinkStore` mutation.

Не входит в первый slice:

- parser `$ent`/`$$...`;
- named/path references;
- lvalue write semantics;
- foreach implementation;
- while/switch migration;
- context materialization;
- scheduler/parallelism;
- canonical numeric/text values.

Эти части используют готовый context foundation в #126–#128.

## Conformance для первого slice

Нужны как минимум tests:

1. root roles читаются точно;
2. `parent(root)` возвращает root observationally;
3. child имеет собственные roles и точный parent;
4. traversal на 1/2/3/произвольную глубину возвращает правильные snapshots;
5. same-context dispatch меняет execution-step entity, но semantic roles остаются теми же;
6. child-context dispatch показывает новый semantic frame;
7. controller identity отличается от relation-state и оба доступны без смешения;
8. observer получает тот же semantic view, что context-role lookup;
9. context reads не меняют `store.size()`;
10. никакого JSON/Anum/string pronoun parsing в core;
11. existing BootstrapRuntime behavior остаётся численно/структурно эквивалентным;
12. ASan/UBSan и portable matrix зелёные.

Отдельный follow-up должен доказать sequential relation-state threading до подключения textual `$rel` compiler.

## Архитектурные veto

- `$rel` не равен `ExecutionContext.relation`;
- recursive `execute()` не создаёт semantic child автоматически;
- semantic context не хранится в mutable global registry;
- dynamic context не materialize-ится автоматически;
- raw C++ pointer/reference не становится program-visible identity;
- call-frame chain не заменяет semantic lineage;
- root saturation не реализуется физическим ownership cycle;
- selector evaluation не вызывает `intern()`;
- context frontend syntax не попадает в `Executor`;
- relation-state не обновляется скрытым универсальным правилом;
- новый слой не создаёт второй executor.

## Решение ADR

AVM принимает следующую модель:

```text
Executor dispatch state
    отдельно от
immutable SemanticContextState
```

`SemanticContextState` содержит `{entity, relation_state, subject, object}` и immutable lineage. Root parent observationally saturates к root. Context transitions являются явными: same, child, parent continuation или projected/deferred. `$...` в будущем компилируется в structural context selector, который наблюдает lineage без materialization.

Это является нормативной основой реализации #125 и reference compiler #126.
