# Link-native модель программ AVM

Этот документ определяет bootstrap-представление программ AVM. Это инженерный протокол поверх канонического `LinkStore` и Relations Model codec; он не объявляется фундаментальным vocabulary МТС.

## Основной инвариант

После проекции/import исполнимая структура программы живёт в том же хранилище связей, что и данные:

```text
представление программы ∈ LinkStore
```

Executor не должен требовать текстовые имена операторов, JSON nodes, C++ maps определений функций или maps имён параметров для понимания структуры программы.

## Bootstrap-словарь

`BootstrapVocabulary` материализует непрозрачные `LinkId` identities для:

- `unit` — marker, используемый как subject bootstrap expression entities;
- `nil` — terminator цепочек link-list;
- Boolean values `true_value` и `false_value`;
- expression relations: quote, parameter, sequence, NOT, AND, OR, IF и CALL;
- runtime structure relations: function, binding и frame.

IDs намеренно непрозрачны. Текстовые имена существуют только на уровне проекции и не участвуют в core identity или dispatch.

## Форма expression

Bootstrap executable expression nodes являются сущностями Модели Отношений:

```text
Expression(relation, payload)
    = (relation, unit, payload)
    = Link(relation, Link(unit, payload))
```

Примеры:

```text
Literal(value)     = (quote, unit, value)
Parameter(formal)  = (parameter, unit, formal)
NOT(arg)           = (not, unit, List(arg))
AND(a,b)           = (and, unit, List(a,b))
OR(a,b)            = (or, unit, List(a,b))
IF(c,t,e)          = (if, unit, List(c,t,e))
Sequence(e...)     = (sequence, unit, List(e...))
```

Повторная материализация одной immutable structure возвращает тот же канонический `LinkId`, поскольку все составляющие дуплеты intern-ятся.

## Списки

Коллекции аргументов, параметров и последовательностей используют каноническую цепочку дуплетов:

```text
List()      = nil
List(a,b,c) = Link(a, Link(b, Link(c, nil)))
```

`decode_link_list` обнаруживает:

- cycles;
- missing `LinkId`;
- превышение configurable maximum item count.

Таким образом, decoding списка не зависит от JSON arrays и не требует рекурсивного внешнего AST.

## Функции

Function handle — независимо созданный point. Эта косвенность важна: body может ссылаться на handle до материализации immutable function definition, поэтому recursive program graph представим без mutable link identities.

Definition:

```text
params = List(formal1, formal2, ...)
payload = Link(params, bodyRoot)
definition = (function, handle, payload)
```

Один handle имеет не более одного определения. Повтор одинакового definition идемпотентен; попытка привязать другое definition к тому же handle является ошибкой.

Call expression:

```text
args = List(actualExpr1, actualExpr2, ...)
payload = Link(functionHandle, args)
call = (call, unit, payload)
```

Runtime materializes bindings и call frames отдельными link-native structures, описанными в `functions-and-frames.md`.

## Граница bootstrap-модели

Форма `(relation, unit, payload)` — инженерный bootstrap protocol для программ AVM 1.0–1.4. Она не отменяет общую триединую форму Relations Model:

```text
(relation, subject, object)
```

AVM 1.5 должен определить meaningful execution для произвольного `subject`, сохранив bootstrap expressions как частный случай общего контракта.

## Граница с JSON и Anum

Этот слой не разбирает JSON и не разбирает Anum/abits. Projection layer может сопоставлять внешние имена и syntax с vocabulary `LinkId` и строить link graph, но исполнение получает только `LinkId` и runtime vocabulary.

Эта граница намеренная: storage/execution semantics AVM не должны становиться ещё одной реализацией parser. Интеграция Anum подключается через separation description/load/find/realize и структурный L3→L4 boundary.
