# Ядро исполнения AVM

Ядро исполнения AVM запускает сущность Модели Отношений по её `LinkId`. Оно не разбирает JSON и не выполняет dispatch операторов по текстовым именам.

## Путь исполнения

```text
LinkId сущности
    |
    v
decode_relation_entity
    |
    v
ExecutionContext
    |
    v
relation LinkId -> bootstrap/native handler или link-native program structure
    |
    v
LinkId результата
```

## Контекст исполнения

Контекст явный и несёт инстанцированную сущность Модели Отношений и call-state, необходимые для исполнения. Скрытое глобальное состояние C++ не является частью execution contract.

Минимально relation dispatch выводится из канонически декодированной сущности:

```text
entity
relation
subject
object
parent/context, когда применимо
```

Program bindings и call frames принадлежат ассоциативной модели, а не внешнему environment.

## Bootstrap boundary

Native handler может быть зарегистрирован по relation `LinkId`:

```text
identity отношения -> C++ handler
```

Это bootstrap-механизм, а не вторая база программ. Native handlers дают минимальный мост, необходимый для выполнения ассоциативного vocabulary. Новая семантика обязана сохранять dispatch по identity отношения и не создавать параллельный JSON/string-dispatch runtime.

## Результат и материализация

Handler возвращает `LinkId` согласно контракту конкретной relation. Возврат результата сам по себе не означает разрешение произвольной скрытой записи в `LinkStore`.

Наблюдающие relations должны оставаться read-only по смыслу. Relations с эффектом обязаны иметь явный effect/materialization contract.

Это особенно важно для AVM 1.5, где необходимо различить:

```text
identity исполняемой entity
subject/view
object/model
result
manifestation/projection
effect/materialization
```

## Инварианты

1. `Executor::execute` принимает entity `LinkId`, а не JSON expression.
2. Relation dispatch использует identity отношения, а не строки вроде `"Not"` или `"If"`.
3. Сущности декодируются через канонический Relations Model codec.
4. Unknown relation приводит к явной ошибке; наблюдающие операции не изменяют store.
5. Native handler возвращает существующий/канонический `LinkId` согласно контракту операции.
6. Execution context и call state являются явными или представлены links; они не скрыты в singleton-state.
7. Executor зависит от семантики `LinkStore`, а не от конкретного physical backend.
8. JSON/program-session adapters находятся вне execution kernel.
9. Observer/trace/inspection tooling не определяют альтернативную execution semantics.
10. `subject = unit` является bootstrap-частным случаем, а не окончательным определением триединой сущности.

## Удалённый исторический путь

Исторические `rel_t` и JSON-centric `eval()`/`interpret()` runtime были удалены после миграции consumers. Они не являются поддерживаемыми API и не должны возвращаться как fallback path; для исследования старой реализации существует Git history.
