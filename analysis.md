# Анализ проекта AVM — состояние AVM 1.0 foundation

## 1. Что представляет собой AVM сейчас

AVM развивается как ассоциативная виртуальная машина, где базовая физическая структура runtime — канонический дуплет:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

Семантическая сущность Relations Model `(relation, subject, object)` представляется двумя вложенными дуплетами:

```text
(relation, subject, object)
= Link(relation, Link(subject, object))
```

Это разделяет физическую модель хранения и семантическую интерпретацию. `LinkStore` не знает о JSON, Anum grammar или конкретных операторах VM; `Executor` не знает о физической реализации backend.

## 2. Текущий production path

```text
external projection
(JSON; later external Anum/MTS adapter)
        |
        v
projection / importer
        |
        v
canonical LinkStore
        |
        v
Relations Model + executable program links
        |
        v
BootstrapRuntime / Executor
        |
        v
result LinkId
```

Ключевой сдвиг относительно прототипа начала 2026 года: JSON больше не является внутренним AST VM. Старый рекурсивный semantic interpreter удалён; Git хранит его историю.

## 3. Реализованные архитектурные инварианты

### 3.1. Canonical identity

`intern(a,b)` возвращает одну логическую identity для одной пары в пределах store. `find(a,b)` является read-only и не материализует отсутствующую связь.

### 3.2. Relations Model codec

Triplet кодируется через два dyad и декодируется обратно. Эта граница вынесена отдельно от storage и executor.

### 3.3. Program is data

В links представлены:

- expression relation identity;
- списки аргументов;
- function handles;
- formal parameters;
- function definitions;
- calls;
- bindings;
- call frames и parent-frame chain.

`Def`/`Call` и recursion больше не зависят от прежних `func_env`, JSON body и `param_stack`.

### 3.4. Associative Boolean semantics

NOT/AND/OR используют материализованные Relations Model truth-table entities. C++ handlers выполняют orchestration/evaluation, но значения таблиц находятся в links.

### 3.5. Lazy control flow

`If` вычисляет только выбранную ветку. Это проверяется тестами, включая заведомо ошибочную невыбранную ветвь.

### 3.6. Raw/denotation boundary

AVM отделяет сырой носитель от материализованной денотации:

```text
RawCarrier
    -> external parser/protocol
    -> ProjectionDescription
    -> find_projection  // read-only
    -> realize_projection // explicit write
```

Storage не парсит abits и не угадывает protocol context. Реальный Anum/MTS parser должен оставаться во внешнем protocol layer.

### 3.7. Backend boundary

Есть два reference backend:

- `InMemoryLinkStore`;
- `PersistentLinkStore`.

`PersistentLinkStore` использует versioned binary snapshot, сохраняет `LinkId` после reopen и восстанавливает exact/outgoing/incoming indexes. Он является conformance backend, а не обещанием production durability.

## 4. Тестирование и CI

CI разделяет независимые gates:

- source/architecture quality guards;
- C++20 warnings-as-errors для core;
- JSON projection lane без legacy facade;
- legacy outward facade regression lane;
- ASan + UBSan;
- Release portable matrix Linux/Windows/macOS;
- tagged Linux artifact.

Assertions принудительно остаются включёнными и в Release test targets, чтобы `NDEBUG` не превращал тестовые executable в пустые smoke runs.

Количество тестов не фиксируется вручную в документации: CTest/CMake являются источником истины, а новые suites добавляются по мере появления архитектурных gates.

## 5. Что ещё не завершено

### 5.1. Persistent execution vertical slice

Финальный integration proof должен подтвердить, что материализованная программа исполняется после close/reopen persistent backend без повторного JSON parsing/import и с теми же bootstrap relation identities. Это issue #60.

### 5.2. Performance baseline

Нужен воспроизводимый benchmark artifact для обнаружения алгоритмических регрессий без хрупких wall-clock veto на shared runners. Это issue #61.

### 5.3. Исторический `rel_t`

После удаления старого semantic interpreter остался отдельный pointer-based `rel_t` storage/data-codec compatibility path в `include/avm.h` и `src/legacy_json_compat.cpp`.

Он обслуживает старые `import_json/export_json/eval`, CLI и regression tests. Это уже не VM executor, но всё ещё второй identity/storage universe. Он должен быть удалён после миграции consumers на LinkStore-backed data projection. Это issue #68 и существенный оставшийся пункт Definition of Done #27.

### 5.4. Production storage adapters

LinksPlatform и PMM рассматриваются как будущие backend adapters. Они не должны определять семантику Executor и не являются prerequisite для доказательства архитектуры AVM 1.0. Reference persistent backend уже фиксирует интерфейсные требования reopen/identity.

## 6. Текущие технические риски

1. **Bootstrap metadata rooting.** Persistent links сохраняются, но production backend позже должен определить способ надёжно находить root/bootstrap identities после запуска, а не полагаться на process-local configuration.
2. **Legacy codec migration.** Удаление `rel_t` требует сохранить доказательство JSON roundtrip поведения, а не просто удалить старые tests.
3. **Persistence guarantees.** Snapshot backend не имеет WAL, concurrent writer model и crash-atomic commit protocol.
4. **API stability.** AVM 1.0 foundation ещё формирует публичные границы; преждевременная стандартизация большого API может закрепить неверные abstractions.
5. **Theory/protocol coupling.** Anum/MTS semantics должны развиваться в своём canonical protocol implementation и входить в AVM через projection boundary, а не копироваться в C++ storage layer.

## 7. Что не следует делать до закрытия foundation

До завершения #27 не стоит ставить выше архитектурных gates:

- большой stdlib;
- GUI/визуализацию;
- distributed runtime;
- JIT/native compiler;
- AI/knowledge application layer;
- оптимизацию backend без измерений.

Эти направления могут быть полезны позже, но сейчас они увеличили бы поверхность системы до окончательного устранения второго storage path и завершения end-to-end persistence proof.

## 8. Оценка зрелости

AVM уже вышла из стадии «JSON interpreter experiment» и имеет проверяемое link-native ядро. При этом AVM 1.0 ещё нельзя считать завершённой, пока не закрыты финальные integration/cleanup gates #60, #61, #67 и #68.

Текущая сильная сторона проекта — не объём операторов, а появившиеся чёткие архитектурные границы:

```text
storage != semantics
projection != mutation
program == link data
JSON != VM
raw != denotation
```

Именно сохранение этих инвариантов должно быть veto-gate для дальнейшего развития.

---

Актуальная нормативная архитектура: [`docs/architecture.md`](docs/architecture.md).  
Execution kernel: [`docs/execution-kernel.md`](docs/execution-kernel.md).  
CI policy: [`docs/ci.md`](docs/ci.md).  
Roadmap: [`plan.md`](plan.md).
