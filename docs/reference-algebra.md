# Каноническая алгебра ссылок AVM

Родительская задача: #183. Родительский gate: #126. Основание контекста: #125.

## Назначение

Старый `jsonRVM` объединял в одной строковой ссылке сразу несколько механизмов:

- местоимения `$ent/$rel/$sub/$obj`;
- переход к родительскому контексту через дополнительные `$`;
- абсолютные имена;
- JSON member/path traversal;
- lazy database lookup;
- lvalue creation.

AVM разделяет эти уровни.

Core получает **каноническую структурную ссылку**, состоящую только из `LinkId` и `Link(begin,end)`. Строка `$...` является только frontend syntax и компилируется отдельно в #184.

Нормативная граница:

```text
frontend syntax
 -> canonical Reference LinkId
 -> resolve_reference(reference, SemanticContextView)
 -> optional<LinkId>
```

`resolve_reference` наблюдает существующий store и semantic context. Он ничего не materialize.

## Грамматика

```text
ContextSelector := Current | Parent(ContextSelector)

Reference :=
    Role(ContextSelector, Entity)
  | Role(ContextSelector, RelationState)
  | Role(ContextSelector, Subject)
  | Role(ContextSelector, Object)
  | Named(LinkId)
  | Begin(Reference)
  | End(Reference)
  | RelationPart(Reference)
  | SubjectPart(Reference)
  | ObjectPart(Reference)
```

## Словарь ReferenceVocabulary

`ReferenceVocabulary` содержит независимые identity:

```text
current_context
parent_context
entity_role
relation_state_role
subject_role
object_role
named_reference
begin_reference
end_reference
relation_part_reference
subject_part_reference
object_part_reference
```

Все anchors должны существовать и быть различны.

## Current и Parent

Текущий selector является identity:

```text
Current := current_context
```

Один переход к parent:

```text
Parent(selector) := Link(parent_context, selector)
```

Поэтому глубина не требует отдельных vocabulary identities:

```text
Parent(Current)
Parent(Parent(Current))
Parent^N(Current)
```

Материализуется только **выражение selector**, а не dynamic context.

При разрешении количество `Parent` применяется к immutable `SemanticContextView::ancestor(N)`.

Корневая семантика наследуется из #125:

```text
ancestor(N > depth) = root
```

Следовательно любая глубина над root детерминированно saturate-ится на root.

## Ролевая ссылка Role

Role reference кодируется:

```text
Link(role_marker, context_selector)
```

Соответствия:

```text
entity_role         -> SemanticContextRole::Entity
relation_state_role -> SemanticContextRole::RelationState
subject_role        -> SemanticContextRole::Subject
object_role         -> SemanticContextRole::Object
```

Критический инвариант:

```text
relation_state_role != ExecutionContext.relation controller identity
```

То есть будущий `$rel` разрешается в semantic `relation_state`, а не в controller текущего dispatch.

## Именованная ссылка Named

Известная абсолютная identity кодируется:

```text
Named(X) := Link(named_reference, X)
```

`X` уже обязана существовать в store.

`Named`:

- не содержит текстового имени;
- не вызывает database/API lookup;
- не создаёт point для неизвестного имени;
- является результатом frontend name-resolution policy.

Например frontend может иметь явную таблицу:

```text
"ent1" -> X
```

и скомпилировать её в `Named(X)`.

## Begin и End

Структурные projections:

```text
Begin(R) := Link(begin_reference, R)
End(R)   := Link(end_reference, R)
```

После recursive resolve внутренней ссылки они наблюдающе возвращают:

```text
store.get(target).begin
store.get(target).end
```

Никакой materialization при чтении нет.

## Части RelationEntity

```text
RelationPart(R)
SubjectPart(R)
ObjectPart(R)
```

после resolution `R` применяют canonical `decode_relation_entity`:

```text
entity = Link(rel, Link(sub,obj))
```

и возвращают соответственно `rel`, `sub`, `obj`.

Это structural AVM semantics. Оно не является JSON object member traversal.

## Find и realize reference-expression

Canonical expression строится через раздельные операции:

```text
find_context_selector
realize_context_selector

find_context_role_reference
realize_context_role_reference

find_named_reference
realize_named_reference

find_reference_projection
realize_reference_projection
```

`find_*` вызывает только `LinkStore::find` и не меняет store.

`realize_*` является явной materialization boundary и использует `intern`.

Повторная realization возвращает тот же canonical LinkId.

## Разрешение ссылки

```text
resolve_reference(store, vocabulary, reference, context)
```

возвращает:

```text
optional<LinkId>
```

Политика:

- отсутствующий root reference -> miss;
- role, содержащая отсутствующий LinkId в ephemeral context -> miss;
- malformed existing reference structure -> deterministic error;
- неизвестный vocabulary marker -> deterministic error;
- structural projection malformed target -> deterministic Relations Model error;
- read path не вызывает `intern/create_point`.

Есть явный `max_depth` для защиты от патологически глубокой external structure. По умолчанию искусственного предела нет: caller может задать собственный операционный лимит. Лимит не определяет semantic identity корректной ссылки.

## Контракт persistence

Reference LinkId является обычной canonical link structure.

После reopen:

- vocabulary anchors сохраняют identity;
- явно materialized reference сохраняет identity;
- `resolve_reference` с эквивалентным `SemanticContextView` возвращает то же значение;
- повторный `realize_*` не увеличивает store.

Dynamic semantic context при этом не обязан materialize-иться и сам по себе persistent identity не получает.

## Что намеренно не входит

### Текстовый `$...`

Компиляция:

```text
$ent
$$rel
$$$sub
```

остается adapter gate #184.

### Пути JSON pointer/member

`foo/bar/0` не является core reference syntax. JSON container traversal не переносится автоматически.

### Ленивый поиск имен

Внешняя database/capability загрузка не входит в pure resolver. Если она понадобится, это отдельный effect/capability contract #129.

### Запись/lvalue

`resolve_reference` только наблюдает.

Старое поведение `operator[]`, создающее отсутствующий member при write, не смешивается с reference read. Explicit write/effect semantics проектируется отдельно только при наличии compatibility evidence.

## Инварианты

1. reference expression состоит только из links;
2. dynamic semantic context не materialize-ится при resolve;
3. `$rel`-эквивалент всегда означает `RelationState`, не controller identity;
4. arbitrary parent depth композиционен;
5. root traversal saturates;
6. `find/resolve` не пишут;
7. `realize` явен и идемпотентен;
8. named reference не выполняет hidden lookup;
9. JSON/Anum parser не входит в core;
10. persistent и in-memory backends имеют одинаковую structural semantics.
