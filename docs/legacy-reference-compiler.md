# Компилятор legacy-ссылок jsonRVM в AVM

Родительская задача: #184. Родительский gate: #126. Каноническая алгебра: #183.

## Назначение

Синтаксис ссылок старого `jsonRVM` не становится runtime-языком AVM.

Он существует только на frontend boundary:

```text
legacy source string
 -> legacy_reference::compile
 -> ProjectionDescription
 -> find_projection | realize_projection
 -> canonical Reference LinkId
 -> resolve_reference
```

Сам compiler:

- не принимает `LinkStore`;
- не вызывает `find`, `intern` или `create_point`;
- не знает `Executor`;
- не выполняет database lookup;
- не интерпретирует JSON containers;
- не хранит global current context.

## Контекстные местоимения

Поддерживается точная форма:

```text
$ent
$rel
$sub
$obj
$$ent
$$rel
$$sub
$$obj
$$$...
```

Количество `$` определяет parent depth:

```text
N знаков '$'
 -> Parent^(N - 1)(Current)
```

После этого выбирается роль:

```text
ent -> Entity
rel -> RelationState
sub -> Subject
obj -> Object
```

Критически важно:

```text
$rel -> SemanticContextRole::RelationState
```

а не `ExecutionContext.relation` controller identity.

Traversal выше root не требует отдельного правила compiler-а. После materialization canonical reference разрешается через `SemanticContextView::ancestor(N)`, который saturate-ится на root.

## Соответствие frozen jsonRVM evidence

`compat/jsonrvm-golden.json` фиксирует `CASE-RELATIVE-ADDRESSING` из jsonRVM commit:

```text
843b3326141e090ccd1a106ba0a4a21ce72805b7
```

Там наблюдаются группы:

```text
$ent  $rel  $sub  $obj
$$ent $$rel $$sub $$obj
$$$ent ...
$$$$ent ...
```

AVM сохраняет **денотацию context depth + role**, а не старую JSON-container representation результата.

Например старый observable path:

```text
$$$$sub/id
```

разделяется на два понятия:

1. `$$$$sub` — semantic reference, переносится в canonical AVM reference;
2. `/id` — доступ к полю старого JSON container, не является частью reference semantics AVM.

Поэтому compiler принимает `$$$$sub`, но детерминированно отвергает `$$$$sub/id`.

## Абсолютные имена

Поддерживается pure named resolution через таблицу, явно принадлежащую caller-у:

```text
"ent1" -> LinkId X
"ent2" -> LinkId Y
```

Source:

```text
ent1
```

компилируется в projection для:

```text
Named(X)
```

Compiler не проверяет store existence `X`, потому что store ему недоступен.

Дальше policy разделена:

- `find_projection` даст miss, если anchor отсутствует;
- `realize_projection` отвергнет отсутствующий anchor до mutation;
- никакого implicit `create_point` или database fetch нет.

Неизвестное текстовое имя — `CompileError`.

Это переносит чистую часть `CASE-ABSOLUTE-ADDRESSING`, не перенося скрытый storage lookup jsonRVM.

## Структурные суффиксы AVM

После базовой ссылки разрешены только операции, имеющие точный смысл в модели связей:

```text
/begin
/end
/relation
/subject
/object
```

Примеры:

```text
pair/begin
entity/subject
entity/end/begin
$ent/end
```

Они компилируются композиционно в:

```text
Begin(reference)
End(reference)
RelationPart(reference)
SubjectPart(reference)
ObjectPart(reference)
```

и затем разрешаются canonical `resolve_reference`.

## Что намеренно отвергается

### JSON member paths

Не поддерживаются:

```text
$ent/id
ent1/id
ent1/val
ent1/val/id
```

`id` и `val` не имеют встроенного structural meaning в AVM.

### Индексы JSON-массивов

Не поддерживается:

```text
ent1/0
```

Ordered collection semantics относится к отдельному data-model gate, а не к reference compiler.

### Неявные имена

Не поддерживается обращение к неизвестному имени с попыткой загрузить его из базы.

### Lvalue creation

Read/reference compiler ничего не создаёт в target model. Старое write-time создание JSON member не смешивается с pure reference semantics.

## Грамматика

```text
LegacyReference := Base StructuralSuffix*

Base := ContextRole | AbsoluteName

ContextRole := '$'+ ('ent' | 'rel' | 'sub' | 'obj')
AbsoluteName := caller-known exact name

StructuralSuffix :=
    '/begin'
  | '/end'
  | '/relation'
  | '/subject'
  | '/object'
```

Пустые path segments запрещены.

## Почему результат — ProjectionDescription

Compiler не возвращает сразу `LinkId`.

Это принципиально сохраняет общий AVM pipeline:

```text
syntax
 -> neutral structural projection
 -> explicit find или realize
```

Следовательно даже frontend compiler конструктивно не способен скрыто materialize canonical reference expression.

Одинаковый source при одинаковых vocabulary/names после `realize_projection` сходится к одному canonical `LinkId` через обычный `LinkStore::intern`.

## Инварианты

1. compiler не зависит от `LinkStore` и `Executor`;
2. `$rel` означает semantic `RelationState`;
3. arbitrary `$` depth композиционен;
4. неизвестное имя не вызывает hidden lookup;
5. JSON member traversal не маскируется под AVM reference;
6. structural suffix состоит только из операций канонической алгебры #183;
7. compile не materialize;
8. `find_projection` остаётся наблюдающим;
9. `realize_projection` — единственная materialization boundary;
10. textual legacy syntax не попадает в AVM core.
