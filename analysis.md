# Анализ проекта AVM

## Краткий вывод

AVM уже прошёл стадию раннего прототипа и имеет устойчивое link-native ядро. Главная архитектурная проблема старой реализации — ситуация, когда данные были представлены связями, а исполнение оставалось отдельным JSON/C++-интерпретатором, — устранена в AVM 1.0.

Текущая задача сложнее: AVM должен доказать, что способен выразить существенную семантику Модели Отношений из `jsonRVM` непосредственно через связи, не возвращая JSON AST, строковый dispatch или скрытое C++-состояние в семантическое ядро.

Поэтому развитие разделяется на две уже пройденные стадии и одну текущую:

```text
AVM 1.0–1.4
  архитектурный фундамент, запросы, structural stdlib,
  наблюдаемость и inspection tooling

AVM 1.5
  перенос семантики Relations Model из jsonRVM
```

## Текущий канонический путь исполнения

В AVM существует один производственный путь:

```text
внешнее представление
  -> проекция / protocol adapter
  -> canonical LinkStore
  -> Relations Model codec
  -> LinkId программы
  -> BootstrapRuntime / Executor
  -> LinkId результата
```

Удалённые исторические механизмы (`rel_t`, JSON semantic interpreter, `func_env`, `param_stack`, protocol-only Anum bridge) не являются compatibility-ядром и не должны возвращаться.

## 1. Физическая модель связей

Физический примитив AVM — направленный дуплет:

```text
LinkId -> (begin, end)
```

`LinkId` является непрозрачной идентичностью. Семантический код не зависит от адресов C++ объектов, расположения памяти или внутренней структуры backend.

Канонизация задаётся инвариантом:

```text
intern(a,b) == intern(a,b)
```

в пределах одного логического хранилища.

Наблюдающие операции:

```text
find
get
outgoing
incoming
contains
```

не должны материализовать отсутствующие связи.

Явные операции изменения:

```text
create_point
intern
realize
```

должны быть видимы как write/effect boundary.

## 2. Модель Отношений

Триединая сущность:

```text
(relation, subject, object)
```

представляется двумя каноническими дуплетами:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

или математически:

```text
(relation, subject, object)
= (relation, (subject, object))
```

Это важный результат, но он решает только задачу представления. Он не определяет автоматически семантику исполнения `relation`, `subject` и `object`.

`LinkStore` не знает, что такое relation, subject или object. Эти роли принадлежат слою Модели Отношений.

## 3. Исполнение

`Executor` принимает `LinkId` сущности, декодирует её через Relations Model codec, формирует явный `ExecutionContext` и dispatch-ит по `LinkId` отношения.

Ключевые свойства:

- никаких строковых opcode в semantic path;
- JSON не является instruction type;
- native C++ handlers допустимы только как bootstrap boundary;
- пользовательские program structures существуют в `LinkStore`;
- bindings и call frames представлены связями;
- backend не определяет execution semantics.

AVM 1.0 доказал, что программа может быть данными той же асети, в которой находятся остальные сущности.

## 4. Программы, функции и call frames

Link-native program model включает:

- vocabulary identities;
- literals/quote;
- parameters;
- sequence;
- Boolean relations;
- lazy `IF`;
- function handles и definitions;
- call expressions;
- bindings;
- parent-linked call frames.

Состояние вызовов больше не хранится только в `std::map<string,...>` или JSON body.

Это устраняет старый разрыв гомоиконичности, когда программа физически существовала в другом мире, чем данные.

## 5. JSON и Anum как внешние проекции

JSON и Anum находятся за пределами execution kernel.

Для Anum особенно важна граница L3/L4:

```text
raw(A) != den(A)
load(A) не означает materialize(den(A))
find(A) только наблюдает
realize(A) явно материализует denotation
```

Каноническая грамматика и контекстная семантика Anum/МТС находятся в `netkeep80/anum_docs`. AVM не должен становиться второй реализацией МТС.

`RawCarrier` хранит непрозрачный сырой носитель независимо от `LinkStore`. `ProjectionDescription` описывает структурную денотацию независимо от parser. `find_projection` и `realize_projection` разделяют read/write semantics.

## 6. Backends и persistence

`InMemoryLinkStore` — эталонный backend.

`PersistentLinkStore` доказывает:

- стабильность `LinkId` после reopen;
- восстановление canonical pair identity;
- детерминированное восстановление `outgoing/incoming` indexes;
- отказ от принятия повреждённого snapshot;
- явное faulted-state поведение после exception внутри mutation region.

Это reference persistence backend, а не обещание production-grade crash consistency. WAL, atomic snapshot swap, locking и concurrent writer protocol требуют отдельного backend design.

## 7. AVM 1.1 — запросы

`RelationQuery` позволяет ограничивать `relation`, `subject` и/или `object` и строит кандидатов только на существующих `find/outgoing/incoming` индексах.

Запросы:

- не вызывают `intern`/`create_point`;
- не предполагают contiguous `LinkId`;
- не создают hidden entity registry;
- детерминированы;
- одинаково работают для in-memory и persistent backend.

Fan-out benchmarks показали стоимость расширения кандидатов, но пока нет реального SLA, оправдывающего новый физический индекс.

## 8. AVM 1.2 — структурная стандартная библиотека

Нативное structural kernel минимально:

```text
link_begin
link_end
identity_equal
link_exists
pair_intern
```

Наблюдающие операции отделены от явного эффекта `pair_intern`.

Более сложные операции должны по возможности выражаться ordinary AVM functions, а не разрастанием native handler registry.

## 9. AVM 1.3 — наблюдаемость

Наблюдение встроено в существующий `Executor::execute` и не управляет исполнением.

События:

```text
Enter
Return
Fail
```

с конечной классификацией failure phase:

```text
Dispatch
Handler
ResultValidation
```

`BoundedExecutionTrace` — tooling-collector в host memory. Он не является состоянием VM и не создаёт альтернативный executor.

Для persistent reopen требуется точное совпадение LinkId-based trace. Для независимых stores используется сравнение с точностью до биективного переименования непрозрачных IDs.

## 10. AVM 1.4 — inspection tooling

`InspectionSession` и typed inspection commands используют существующие:

- `LinkStore`;
- Relations Model query API;
- `BootstrapRuntime`;
- `BoundedExecutionTrace`.

Текстовый parser команд заканчивается типизированным представлением команды. Он не является новым языком VM и не определяет execution semantics.

Это важная архитектурная граница: debugger/inspection tooling может быть богатым, не превращаясь во второй runtime.

## 11. Главная проблема AVM 1.5

После завершения foundation становится видно, что старый `jsonRVM` содержит семантику, которую текущий minimal bootstrap ещё не выражает полностью.

В `jsonRVM` роли имеют самостоятельный смысл:

```text
relation = controller
subject  = view / receiver / manifestation
object   = model / input
entity   = проявление отношения
```

Текущий AVM исторически начинал с удобного частного случая:

```text
(relation, unit, object) -> result
```

Этого достаточно для минимальной VM, но недостаточно для полной семантики Relations Model.

Дополнительные semantic gaps:

- текущий `ent/rel/sub/obj` контекст;
- parent context chain;
- `$ent/$rel/$sub/$obj` и `$$...`;
- named/absolute/relative references;
- distinction observation vs lvalue realization;
- sequence/lambda/projection semantics;
- `foreach` child contexts;
- canonical value denotation для чисел, текста и collections;
- explicit effect/capability boundary для FS/HTTP/time/native/database lookup.

## 12. Почему нельзя просто перенести `base.rm.h`

Механическое переписывание старых операторов в `register_native` сохранило бы внешнюю видимость части функций, но потеряло бы архитектурный смысл проекта.

Старый `jsonRVM` смешивает:

- syntax;
- AST;
- runtime values;
- mutable context;
- references/lvalues;
- program storage;
- host effects

в `nlohmann::json`.

В AVM эти аспекты должны быть разделены по контрактам.

Поэтому AVM 1.5 начинается не с массового переноса операторов, а с semantic inventory и differential corpus.

## 13. Текущий AVM 1.5 plan

Epic: #122.

Последовательность gates:

1. #123 — semantic inventory `jsonRVM` и differential corpus;
2. #124 — triune execution contract;
3. #125 — link-native execution contexts;
4. #126 — reference/addressing algebra и frontend compilation;
5. #127 — sequence/lambda/projection semantics и порядок эффектов;
6. #128 — canonical value denotation и pure vocabulary;
7. #129 — explicit capabilities/effects;
8. #130 — convergence JSON/Anum на одном denotation contract;
9. #131 — end-to-end differential migration slice.

Первый slice #123 уже находится в `main`: `compat/jsonrvm-semantics.json`, `compat/jsonrvm-golden.json` и validator CI фиксируют provenance и начальную semantic classification.

## 14. Сильные стороны архитектуры

1. **Один физический примитив.** Все структуры сводятся к связям.
2. **Один semantic execution path.** Нет legacy interpreter fallback.
3. **Непрозрачная идентичность.** Семантика не зависит от pointer/address layout.
4. **Canonical pair identity.** Повторная материализация одной пары идемпотентна.
5. **Read/write separation.** Поиск и запрос не создают данные.
6. **Program-as-data.** Program graph, bindings и frames существуют в асети.
7. **Backend neutrality.** Persistence не определяет VM semantics.
8. **Protocol boundary.** JSON и Anum не проникают в executor.
9. **Наблюдаемость без второго runtime.** Trace/inspection являются consumers существующего execution path.
10. **CI как архитектурный gate.** Guards запрещают возврат удалённых side channels.

## 15. Основные риски

### 15.1. Ошибка при переносе triune semantics

Если `subject` снова будет сведён к техническому `unit`, AVM останется хорошей VM над links, но не станет полноценным исполнителем Relations Model.

### 15.2. Скрытая материализация references

Reference lookup не должен создавать то, что он ищет. Lvalue/write semantics должны быть отдельной операцией.

### 15.3. Второй мир values

Нельзя вводить `std::variant<json,...>` или аналогичный host value universe как внутренний semantic slot, иначе data/program homogeneity снова будет нарушена.

### 15.4. Эффекты внутри pure relations

FS/HTTP/time/database/native operations должны иметь explicit capability boundary. Иначе deterministic semantics и future scheduling становятся неаудируемыми.

### 15.5. Преждевременная параллельность

Старый `jsonRVM` экспериментировал с параллельными projection paths. В AVM parallel scheduler допустим только после определения purity, effect ordering и deterministic failure model.

## 16. Что сейчас не следует делать

До завершения AVM 1.5 foundation не стоит приоритетно развивать:

- GUI debugger;
- distributed execution;
- JIT;
- большой HTTP/FS ecosystem;
- speculative parallel scheduler;
- отдельную внутреннюю реализацию МТС;
- backend-specific semantic indexes;
- второй compatibility interpreter.

## Основные документы

- `README.md` — обзор и состояние проекта;
- `docs/architecture.md` — базовый архитектурный контракт;
- `docs/execution-kernel.md` — ядро исполнения;
- `docs/program-model.md` — link-native program model;
- `docs/functions-and-frames.md` — функции, bindings и call frames;
- `docs/projection-boundary.md` — граница description/find/realize;
- `docs/anum-l3-l4-bridge.md` — структурный мост Anum;
- `docs/relations-query.md` — запросы Модели Отношений;
- `docs/execution-observability.md` — observer/trace contract;
- `docs/inspection-session.md` — inspection tooling;
- `docs/jsonrvm-compatibility.md` — semantic migration contract;
- `plan.md` — dependency-ordered план развития.

## Вывод

AVM уже доказал возможность построения настоящей link-native виртуальной машины над двумерной асетью. Следующий качественный переход — не увеличение количества операторов, а восстановление полноценной семантики Модели Отношений поверх этого фундамента.

Критерий успеха AVM 1.5: нетривиальная программа старого `jsonRVM` должна после внешней проекции исполняться через единственный `LinkStore -> Relations Model -> Executor` path и давать эквивалентный наблюдаемый результат без JSON semantic interpreter.
