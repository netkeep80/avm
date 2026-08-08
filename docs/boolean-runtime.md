# Ассоциативный Boolean runtime AVM

Bootstrap runtime исполняет первый link-native subset программ AVM без JSON semantics и строкового dispatch операторов.

## Таблицы истинности являются данными

Boolean semantics материализуются в `LinkStore` как сущности Модели Отношений. Native C++ handlers не кодируют таблицы истинности через `if`/`switch`: они вычисляют аргументы и выполняют ассоциативный поиск.

Строки унарного NOT:

```text
(not, true,  false)
(not, false, true)
```

Бинарные функции используют канонический `LinkId` пары аргументов как subject:

```text
key = Link(left, right)
(and, key, result)
(or,  key, result)
```

Lookup использует `LinkStore::find(left, right)` и поэтому не материализует отсутствующий key во время запроса.

## Исполнимые expressions

Bootstrap expressions используют форму:

```text
(relation, unit, payload)
```

Handlers покрывают:

- quote/literal;
- sequence;
- NOT;
- AND;
- OR;
- IF.

Expression handlers отклоняют Relations Model rows, у которых subject не равен execution marker `unit`. Благодаря этому truth-table data и executable application nodes могут использовать один relation namespace без смешения ролей.

Это ограничение относится именно к bootstrap expression protocol. AVM 1.5 расширяет общий execution contract для meaningful non-unit subject.

## Ленивый IF

`IF` использует две ассоциативные строки выбора:

```text
(if, true,  true)
(if, false, false)
```

Исполнение:

1. вычислить только condition expression;
2. определить Boolean selector через сохранённое отношение;
3. исполнить ровно одну выбранную branch.

Тесты доказывают lazy semantics: entity с незарегистрированной relation помещается в невыбранную branch. Expression успешно завершается, тогда как та же entity приводит к ошибке, если становится выбранной.

## Правило мутаций

Truth-table rows материализуются при создании `BootstrapRuntime`. Исполнение уже построенной Boolean program выполняет read-only lookup таблиц; тесты проверяют, что `LinkStore::size()` не меняется при nested Boolean evaluation и invalid Boolean lookup.

Материализация runtime bindings/call frames является отдельной существующей семантикой функции и не скрывается внутри Boolean relations.

## Инварианты

- truth tables представлены links, а не C++ branching logic;
- Boolean lookup не создаёт missing keys;
- lazy `IF` не исполняет unselected branch;
- Boolean results являются canonical vocabulary identities;
- runtime dispatch идёт по relation `LinkId`;
- JSON/string operators не участвуют в semantic path.
