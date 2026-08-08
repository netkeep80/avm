# План развития AVM

## Правило планирования

Работа ведётся последовательными gates с явными зависимостями. Новый слой начинается только после того, как его зависимости получили стабильный контракт и зелёные CI-проверки.

Неизменный архитектурный инвариант:

```text
внешняя проекция
  -> канонический LinkStore
  -> кодек Модели Отношений
  -> программа как LinkId
  -> BootstrapRuntime / Executor
  -> LinkId результата
```

Запрещены второй semantic path, скрытая база программ, legacy storage universe и backend-specific semantic index.

## AVM 1.0 — архитектурный фундамент завершён ✅

### Gate 1 — архитектурный контракт ✅

- один физический примитив: направленный дуплет;
- непрозрачный `LinkId`;
- каноническая идентичность пары;
- чтение не материализует отсутствующие данные;
- семантика backend отделена от семантики VM.

### Gate 2 — канонический LinkStore ✅

- эталонный `InMemoryLinkStore`;
- явное разделение `find` и `intern`;
- conformance canonical identity/query;
- семантика не зависит от pointer identity или layout.

### Gate 3 — кодек Модели Отношений ✅

```text
(relation, subject, object)
= (relation, (subject, object))
```

### Gate 4 — ядро исполнения ✅

- `Executor` принимает entity `LinkId`;
- dispatch идёт по relation `LinkId`;
- execution context явный;
- bootstrap native handlers не являются второй базой программ.

### Gate 5 — program-as-links ✅

- programs, bindings и frames представлены links;
- есть link-native vertical slice;
- legacy pointer-based `rel_t` semantic/storage universe удалён.

### Gate 6 — граница внешних протоколов ✅

- parser/grammar/context остаются вне AVM core;
- AVM получает structural projection/denotation;
- `raw(A)` отделён от `den(A)`;
- `find` наблюдающий, `realize` явный и материализующий.

Каноническая семантика Anum/МТС поддерживается в `netkeep80/anum_docs`; AVM содержит только structural L3→L4 boundary.

### Gate 7 — integration hardening ✅

- persistent reopen/identity conformance;
- end-to-end link-native vertical slice;
- benchmark baselines;
- warnings-as-errors;
- ASan/UBSan;
- архитектурные CI guards.

### Gate 8 — готовность AVM 1.0 к релизу ✅

Проверены:

- Linux/Windows/macOS portable builds/tests;
- installed-package consumer на Linux/Windows/macOS;
- публичный `avm::core`;
- один документированный execution path;
- отсутствие legacy semantic fallback.

## AVM 1.1 — ассоциативные запросы завершены ✅

Epic: #83.

### Gate 9 — ограниченные запросы Модели Отношений ✅

Реализовано через #84/#85.

```text
relation? / subject? / object?
-> существующие find/outgoing/incoming
-> structural decode/filter
-> deterministic RelationMatch[]
```

Свойства:

- минимум одно ограничение;
- `intern`/`create_point` не вызываются;
- нет guessed full-store enumeration;
- детерминированные ordering/deduplication;
- эквивалентность InMemory/Persistent/reopen;
- public installed-package consumption.

### Gate 10 — измеряемое развитие индексов ✅ решение принято

Fan-out scaling измерен на 1/8/64/256 в #86/#87.

Решение:

- сохранить `find/outgoing/incoming` единственным primitive lookup/index contract;
- дополнительные indexes отложить до появления реального workload/SLA;
- любое будущее расширение обязано сравниваться с записанным baseline и доказать InMemory/Persistent conformance.

## AVM 1.2 — структурная стандартная библиотека завершена ✅

Epic: #88.

### Gate 11 — наблюдающие structural primitives ✅

Реализовано через #89/#90:

```text
link_begin(expr)
link_end(expr)
identity_equal(a,b)
link_exists(a,b)
```

Свойства:

- аргументы вычисляются как обычные AVM expressions;
- handlers используют только наблюдающие `LinkStore::get/find`;
- отсутствующие связи не материализуются;
- predicates возвращают canonical Boolean values;
- `nil` не используется как missing sentinel.

### Gate 12 — явный эффект канонической пары ✅

Реализовано через #91/#92:

```text
pair_intern(a,b) -> canonical LinkId(a,b)
```

Свойства:

- handler делегирует `LinkStore::intern`;
- missing pair создаётся ровно один раз;
- repeated materialization идемпотентна;
- persistent reopen сохраняет LinkId.

### Gate 13 — композиция стандартной библиотеки ✅

Реализовано через #93/#94.

Высокоуровневые structural operations — обычные AVM functions:

```text
is_self_link(x) = identity_equal(link_begin(x), link_end(x))

pair_matches(x,b,e) = AND(identity_equal(link_begin(x), b),
                          identity_equal(link_end(x), e))
```

Новые native relations добавляются только для действительно неразложимых observation/effect boundaries.

## AVM 1.3 — наблюдаемость исполнения завершена ✅

Epic: #95.

### Gate 14 — детерминированный неуправляющий observer ✅

Реализовано через #96/#97.

```text
Enter(ExecutionContext)
Return(ExecutionContext, result LinkId)
Fail(ExecutionContext, phase)
```

Свойства:

- только canonical LinkId/context data;
- нет timestamps, host pointers, textual opcode names, JSON/Anum или backend data;
- observer не получает mutable `Executor`, `LinkStore`, context или result;
- исключения observer не меняют program control flow;
- nested calls наблюдаются через тот же `Executor::execute`;
- observation не материализует links.

### Gate 15 — классификация фаз отказа ✅

Реализовано через #98/#99:

```text
Dispatch
Handler
ResultValidation
```

Классификация отражает boundary AVM, а не тип или текст host exception.

### Gate 16 — ограниченный collector trace ✅

Реализовано через #100/#101:

```text
BoundedExecutionTrace(max_events)
```

Он хранит точный префикс `ExecutionEvent`, явно сообщает truncation и не становится VM state.

### Gate 17 — persistent/backend-neutral trace conformance ✅

Реализовано через #102/#103.

```text
тот же PersistentLinkStore после reopen
  -> точное совпадение событий и LinkId

независимые InMemory/Persistent stores
  -> эквивалентность с точностью до биективного переименования LinkId
```

### Gate 18 — trace-enabled CLI consumer ✅

Реализовано через #104/#105.

CLI использует существующий `JsonProgramImporter`, `BootstrapRuntime::execute` и public observer boundary:

```text
avm program.json
avm --trace program.json
avm --trace-limit 64 program.json
```

Второго evaluator не создано.

## AVM 1.4 — inspection tooling завершён ✅

Цель AVM 1.4 — дать разработчику возможность исследовать асеть и исполнение без появления debugger-specific semantics.

### Gate 19 — typed inspection session ✅

`InspectionSession` объединяет только существующие публичные contracts:

- read-only link/entity inspection;
- `RelationQuery`;
- `BootstrapRuntime` execution;
- `BoundedExecutionTrace`.

Inspection layer не владеет отдельным executor или storage semantics.

### Gate 20 — persistent inspection session conformance ✅

Проверено, что после reopen:

- LinkId identities стабильны для того же store;
- query/inspection результаты сохраняются;
- execution и trace используют тот же runtime path.

### Gate 21 — typed scripted inspection commands ✅

Текстовый syntax parsing заканчивается типизированной командой. Исполнение команд происходит через `InspectionSession` и canonical API.

String commands являются tooling presentation, а не VM opcodes.

### Gate 22 — hardening persistent mutation/release path ✅

- `PersistentLinkStore` переходит в faulted state при exception внутри guarded `insert_link + persist` mutation region;
- после failed mutation частичное in-memory состояние невозможно принять за committed store state;
- tagged release artifact теперь зависит от полной portable Linux/Windows/macOS matrix.

## AVM 1.5 — перенос семантики Relations Model из jsonRVM 🚧

Epic: #122.

### Почему нужен новый этап

AVM 1.0–1.4 доказал архитектуру VM над каноническими связями. Но minimal bootstrap главным образом исполняет expressions вида:

```text
(relation, unit, object) -> result
```

В исходной Модели Отношений все роли имеют самостоятельную семантику:

```text
relation = controller
subject  = view / receiver / manifestation
object   = model / input
```

Кроме того, `jsonRVM` содержит контексты, references, lambda/projection semantics и большой vocabulary.

Representation theorem `(r,s,o) = (r,(s,o))` уже реализована. AVM 1.5 переносит **семантику исполнения**.

### Gate 23 — semantic inventory и differential corpus #123 🚧

Первый slice уже merged через #132.

В `main` находятся:

- `compat/jsonrvm-semantics.json` — versioned classification manifest;
- `compat/jsonrvm-golden.json` — frozen legacy assertions;
- `docs/jsonrvm-compatibility.md` — migration contract;
- `tools/validate_jsonrvm_compatibility.py` — metadata/golden consistency validator;
- отдельный CI gate.

Manifest привязан к конкретному commit `netkeep80/jsonRVM`.

Оставшаяся работа:

- уточнить mutation/context/failure metadata;
- добавить репрезентативные deterministic cases для Boolean/branch;
- sequence/foreach child contexts;
- arithmetic;
- missing-reference failures;
- pure relation composition.

Массовый перенос `base.rm.h` до завершения классификации запрещён.

### Gate 24 — triune execution contract #124

Нужно определить каноническую link-native семантику:

```text
execute(entity = (relation, subject, object), context)
```

без требования `subject == unit`.

Должны быть различены:

- identity исполняемой entity;
- input/model;
- subject/view/receiver;
- вычисленный result;
- manifestation/projection;
- materialization/effect.

Pure execution не должен скрыто мутировать existing links.

### Gate 25 — link-native execution contexts #125

Нужно представить наблюдаемую семантику:

```text
ent
rel
sub
obj
parent context
```

и подготовить canonical relations для frontend pronouns `$ent/$rel/$sub/$obj` и parent traversal.

Context не должен существовать только как C++ map/JSON object.

### Gate 26 — reference/addressing algebra #126

Frontend syntax:

```text
$ent
$rel
$sub
$obj
$$...
named/path references
```

должен компилироваться в canonical link-native reference expressions.

Главный инвариант:

```text
resolve/find — наблюдение
write/realize — отдельная явная операция
```

Runtime не должен интерпретировать JSON Pointer strings.

### Gate 27 — sequence/lambda/projection semantics #127

Нужно определить:

- ordered sequence;
- child context creation;
- foreach/map semantics;
- projection result aggregation;
- fail-fast/failure propagation;
- deterministic effect ordering.

Automatic parallelism откладывается до появления formal purity/effect model.

### Gate 28 — canonical value denotation и pure vocabulary #128

Требуется link-native представление значений, необходимое для совместимости с `jsonRVM`:

- Boolean;
- числа;
- text/bytes;
- ordered collections;
- structural equality/order where justified.

Нельзя вводить второй host-language value universe как semantic core.

После контракта переносится минимальный pure vocabulary: arithmetic, comparisons, selected collection/string operations.

### Gate 29 — explicit capability/effect boundary #129

FS/HTTP/time/native/database lookup не должны скрываться внутри pure relation handlers.

Нужно определить:

```text
canonical request
-> explicit capability
-> deterministic result/effect outcome
```

и fake providers для CI.

### Gate 30 — convergence JSON/Anum #130

JSON и Anum должны сходиться к одному canonical denotation/find/realize contract.

Frontend-specific syntax не определяет runtime identity или execution semantics.

### Gate 31 — end-to-end differential migration #131

Минимум одна нетривиальная программа старого `jsonRVM` должна:

1. быть зафиксирована как deterministic golden case;
2. спроецироваться в canonical links;
3. исполниться единственным `BootstrapRuntime/Executor` path;
4. дать эквивалентный observable result/error/context behavior;
5. не требовать старый JSON interpreter.

## Документационный gate #134 🚧

Нормативный язык project-owned документации AVM — русский.

Технические identifiers, API names, formats и code snippets сохраняются в исходном виде. `LICENSE` и явно vendor-owned документация не переводятся.

Этот gate не меняет runtime semantics.

## Дальнейшие направления после AVM 1.5

Только после semantic migration имеет смысл приоритетно рассматривать:

1. interactive debugger с отдельным control contract;
2. дополнительные frontends;
3. production persistence backends;
4. visualization/GUI;
5. distributed execution/storage;
6. scheduler/parallel execution после purity/effect model;
7. JIT или native compilation;
8. дальнейшее расширение standard library через link-native composition.

## Правило зависимостей

Если gate зависит от незавершённого PR, независимая подготовка допустима, но dependent code не merge-ится до зелёного dependency gate.

После миграции consumers legacy implementation удаляется, а не сохраняется вторым production path. Историю хранит Git.
