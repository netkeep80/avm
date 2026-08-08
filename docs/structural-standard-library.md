# Структурная стандартная библиотека AVM 1.2

AVM 1.2 разделяет structural surface на небольшое native kernel и расширяемую link-native библиотеку.

## Нативное структурное ядро

Bootstrap runtime содержит только primitive relations, которые нельзя выразить через уже существующие structural observation/effect semantics:

```text
link_begin(expr)       -> begin-полюс вычисленного LinkId
link_end(expr)         -> end-полюс вычисленного LinkId
identity_equal(a,b)    -> каноническое Boolean-значение
link_exists(a,b)       -> каноническое Boolean-значение, observation
pair_intern(a,b)       -> канонический LinkId(a,b), явный эффект
```

`link_begin`, `link_end`, `identity_equal` и `link_exists` не материализуют наблюдаемую структуру. `pair_intern` — явная граница canonical structural mutation и делегирует `LinkStore::intern`.

## Производные операции являются программами, а не opcode

Если standard-library operation можно выразить существующим kernel, она должна быть обычным definition функции AVM, а не ещё одним native handler.

Например:

```text
is_self_link(x) = identity_equal(link_begin(x), link_end(x))
```

и:

```text
pair_matches(x,b,e) = AND(identity_equal(link_begin(x), b),
                          identity_equal(link_end(x), e))
```

Оба body являются обычными Relations Model expression entities, построенными `ProgramBuilder`. Function handles — обычные `LinkId`, а `Executor::has_native(handle)` для них возвращает false.

Направление расширения:

```text
малое native kernel
    -> link-native function definitions
    -> новые функции могут вызывать эти функции
```

а не:

```text
постоянно растущий C++ registry opcode/native-handler
```

## Function calls и учёт эффектов

Composed function, body которой содержит только observational primitives, не обязана быть zero-write на физическом уровне `LinkStore`.

AVM намеренно представляет execution state функций как links. Существующий call runtime materialize-ит canonical bindings и call frames. Поэтому первый вызов конкретной structural function может увеличить store, даже если body не содержит structural mutation primitive.

Следует различать:

- **семантику библиотеки:** `is_self_link` и `pair_matches` не содержат `pair_intern` и не вводят native handler;
- **семантику execution state:** function machinery может intern-ить bindings, binding lists, frame payloads и frame entities;
- **canonical convergence:** повтор structurally identical call переиспользует структуры там, где их identities совпадают.

Замена link-native call frames на ephemeral C++ stack только ради слова «pure» создала бы второй execution-state model и потому запрещена.

## Persistence

Function definitions уже link-native и сохраняются вместе со store. После reopen caller использует явные function handles вместе с bootstrap vocabulary, необходимым для восстановления runtime.

Скрытого standard-library registry и string-name lookup table нет. Handle является identity функции.

## Правило расширения

Перед добавлением native relation нужно проверить, выражается ли её поведение функцией над существующими primitives.

Если да — реализация должна быть в links. Новый native relation оправдан только для действительно неразложимой observation/effect boundary либо когда измеряемые требования доказывают невозможность соблюсти контракт через композицию.
