# Сквозной execution-сценарий AVM 1.0

Этот документ описывает исполнимый путь, проверяемый `vertical_slice_tests`.

## Один семантический путь

```text
JSON expression
    |
    v
JsonProgramImporter
    |
    v
LinkStore (InMemoryLinkStore или PersistentLinkStore)
    |
    v
root LinkId
    |
    v
BootstrapRuntime / Executor
    |
    v
result LinkId
```

JSON используется только importer-ом. `Executor` получает `LinkId`; он не получает и не хранит JSON AST.

Одни и те же materialized program entities исполняются через абстрактный `LinkStore` contract. Тест покрывает NOT/AND/OR, lazy `If`, `Def`/`Call` и representative recursive call.

## Повторное открытие persistent store

Persistent scenario имеет две отдельные фазы.

### Первый lifetime процесса/store

1. Открыть пустой `PersistentLinkStore`.
2. Создать `BootstrapRuntime`; он вводит bootstrap vocabulary с явными `LinkId` identities.
3. Импортировать JSON expressions в links.
4. Выполнить imported roots и проверить observable results.
5. Сохранить только structural identities, нужные после reopen: `BootstrapVocabulary` и root `LinkId`.
6. Закрыть store.

### Lifetime после reopen

1. Повторно открыть тот же `PersistentLinkStore`.
2. Создать `BootstrapRuntime(store, saved_vocabulary)`.
3. Проверить, что восстановление runtime не меняет `store.size()`; truth-table materialization обязана переиспользовать canonical links.
4. Выполнить сохранённые root `LinkId` напрямую.
5. Повторить ещё один reopen, чтобы исключить случайное one-reopen-only поведение.

На reopened phase JSON не parse-ится и не import-ится. Исполняется сохранённый link graph программы, а не внешний AST, перестраиваемый при каждом запуске.

## Bootstrap identity является persistent state

Bootstrap vocabulary не пересоздаётся после reopen. Relation identities вроде `and_relation`, `if_relation`, `function_relation` и `call_relation` входят в semantic address space materialized program.

Поэтому `BootstrapRuntime` имеет два режима construction:

```cpp
BootstrapRuntime(store);              // создать новый vocabulary
BootstrapRuntime(store, vocabulary);  // восстановить существующий vocabulary
```

Restore path проверяет существование всех vocabulary `LinkId` и различность bootstrap identities. Invalid restored metadata отклоняется до execution.

Будущий production persistence layer может хранить named/rooted reference на bootstrap metadata. Reference backend намеренно не навязывает такую policy: persistence contract доказывает stable link identity, а этот vertical slice — возможность восстановить runtime при наличии правильных structural identities.

## Backend-neutral инвариант

`BootstrapRuntime`, `Executor`, Relations Model helpers и `ProgramBuilder` зависят от `LinkStore`, а не от internals `PersistentLinkStore`. Persistence format, index rebuild и filesystem behavior находятся за backend boundary.

Vertical slice входит в `avm_core_tests` и поэтому проверяется через:

- C++20 warnings-as-errors;
- ASan + UBSan на Linux;
- Release portable runs на Linux, Windows и macOS.

Это correctness/conformance test, а не заявление о durability или performance. Crash consistency, WAL/journaling и production backend tuning остаются отдельными задачами.
