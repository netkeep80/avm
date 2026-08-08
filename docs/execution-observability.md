# Контракт наблюдаемости исполнения

AVM 1.3 добавляет наблюдение к существующему execution path, не создавая traced executor, debugger-specific runtime или второе semantic representation.

## Граница

Путь исполнения остаётся единственным:

```text
entity LinkId
  -> decode Relations Model entity
  -> ExecutionContext
  -> optional Enter observation
  -> relation-LinkId dispatch
  -> handler / nested Executor::execute calls
  -> result validation
  -> optional Return observation
  -> result LinkId
```

Если execution завершается ошибкой после создания валидного `ExecutionContext`, invocation публикует `Fail` и повторно бросает исходное exception. Ошибка до декодирования context события не создаёт.

## Публичная модель событий

`ExecutionEvent` содержит только детерминированные данные AVM:

```text
kind = Enter | Return | Fail
context = {
  entity,
  relation,
  subject,
  object,
  optional parent,
  optional frame
}
optional result LinkId
optional failure phase
```

`result` присутствует только у `Return`. `failure_phase` — только у `Fail` и принимает одно из значений:

```text
Dispatch          — для relation нет native handler
Handler           — выбранный handler бросил exception
ResultValidation  — handler вернул LinkId, отсутствующий в LinkStore
```

Phase указывает, на какой канонической границе AVM произошёл отказ. Она не кодирует exception message/type, backend error или protocol-specific diagnostic.

В event contract намеренно нет timestamps, thread IDs, pointers, textual opcode names, JSON/Anum values или backend-specific data.

## Наблюдатель не управляет программой

`ExecutionObserver` получает immutable `ExecutionEvent`. Ему не передаются `Executor`, `LinkStore`, mutable context или mutable result reference.

Доставка события изолирована внутри `Executor`: exceptions из `ExecutionObserver::observe` подавляются и не могут заменить program result или program exception. Это правило действует и для success, и для каждой failure phase.

Observer принадлежит caller. `Executor` хранит только non-owning pointer; caller обязан обеспечить lifetime observer, пока он подключён.

## Порядок событий и unwind

Каждый invocation с валидным context создаёт:

```text
Enter(context)
  ...nested execute events...
Return(context,result)
```

или:

```text
Enter(context)
  ...nested execute events...
Fail(context,phase)
```

Nested calls наблюдаются естественно, потому что handlers рекурсивно используют тот же `Executor::execute`. Для ошибки child внутри parent handler порядок остаётся stack-shaped:

```text
Enter(parent)
Enter(child)
Fail(child, Handler)
Fail(parent, Handler)
```

Исходное child exception продолжает распространяться без изменения.

## Детерминизм и execution state

Для одного canonical store/program/vocabulary state observer получает ту же последовательность LinkId-based events и те же AVM failure phases.

Function calls остаются link-native: bindings и call frames могут materialize-иться существующим runtime contract. После convergence этих canonical structures одинаковые повторные calls дают одинаковый trace.

Само наблюдение links не материализует. Подключение observer не меняет semantics `LinkStore`.

## Ограниченный collector для tooling

`BoundedExecutionTrace` — reusable host-memory consumer `ExecutionObserver`, а не semantic state AVM:

```cpp
avm::BoundedExecutionTrace trace(128);
runtime.executor().set_observer(&trace);
const avm::LinkId result = runtime.execute(root);

for (const avm::ExecutionEvent &event : trace.events())
{
    // анализ детерминированных событий AVM
}

if (trace.truncated())
{
    // заданной ёмкости не хватило
}
```

Capacity фиксируется при construction, а storage резервируется до обычного подключения к executor. Ошибка host allocation поэтому может быть сообщена caller до начала execution.

При observation переполнение заданной capacity не alloc-ит, не бросает exception и не выдумывает event. Collector сохраняет точный помещающийся prefix и устанавливает `truncated()==true`. Zero-capacity collector хранит ноль событий и помечается truncated после первого события.

`reset()` очищает events и truncation, сохраняя `max_events()` и reserved-storage policy. Events доступны как immutable `std::span<const ExecutionEvent>`.

Следует различать:

```text
ExecutionEvent semantics  = канонический контракт наблюдения AVM
BoundedExecutionTrace     = опциональное host-memory хранение tooling
```

Collector не владеет `Executor`, `LinkStore`, backend, protocol adapter или persistence target и ничего не записывает в links/files скрыто.

## Persistent reopen и backend-neutral equivalence

Сравнение trace должно уважать opaque `LinkId`. Numeric values имеют смысл только внутри одного logical store; глобального namespace `LinkId` AVM не определяет.

### Один persistent logical store после reopen

`PersistentLinkStore` сохраняет `LinkId` после close/reopen. После convergence program и link-native frame/binding state одна и та же сохранённая программа с тем же bootstrap vocabulary должна давать **точно одинаковую** полную последовательность `ExecutionEvent`, включая numeric `LinkId`.

Сравниваются:

```text
entity / relation / subject / object
optional parent / frame
optional result
kind / failure_phase
```

Повторное execution после reopen не должно увеличивать store только из-за attached observation.

### Независимые stores/backends

Независимые `InMemoryLinkStore` и `PersistentLinkStore` могут присвоить equivalent structure разные numeric IDs. Сравнивать raw numbers между ними некорректно.

Backend-neutral conformance сравнивает полные traces с точностью до биективного переименования наблюдаемых `LinkId`. Reference test использует first-occurrence normalization:

1. идти по events в execution order;
2. обходить поля `entity`, `relation`, `subject`, `object`, `parent`, `frame`, `result` в фиксированном порядке;
3. выдавать local ordinal при первом появлении `LinkId`;
4. переиспользовать ordinal при каждом повторе той же identity;
5. сохранять `nullopt`, `ExecutionEventKind` и `ExecutionFailurePhase` точно.

Normalization сохраняет equality/aliasing relations и отбрасывает только backend-local numeric allocation choices. Это conformance helper, не production identity registry и не serialization format.

Truncated trace не может использоваться как доказательство полной equivalence.

## CLI как consumer observer boundary

CLI — первый user-facing consumer AVM observability.

Обычный режим:

```text
avm program.json
```

Trace mode:

```text
avm --trace program.json
avm --trace-limit 64 program.json
```

Путь остаётся каноническим:

```text
JSON input
  -> JsonProgramImporter
  -> LinkId program
  -> attach BoundedExecutionTrace
  -> BootstrapRuntime::execute(LinkId)
  -> result LinkId
  -> JSON result projection
```

Нет `TraceExecutor`, copied evaluator или trace-specific opcode dispatch. Text labels в rendering являются presentation, а не semantic identity.

Summary всегда сообщает число retained events и `complete`/`truncated` status. `--trace-limit` делает ресурсное ограничение явным.

При failed execution retained `Fail(phase)` events могут быть выведены до обычного host diagnostic; original exception и exit-status policy сохраняются.

Non-expression JSON допустим в value-roundtrip mode, но trace mode его отклоняет: execution context для наблюдения не существует, и AVM не выдумывает события сериализации.

## Политика diagnostics

Детерминированный contract заканчивается на AVM-owned failure phase. Human-readable exception text — runtime diagnostic, а не stable event identity.

C++ exception type names, `std::exception_ptr`, addresses и backend/protocol errors не хранятся в `ExecutionEvent`. CLI может показать пойманное host exception рядом с trace, не объявляя его canonical semantics AVM.

## Явные non-goals

Observability не предоставляет:

- breakpoints/step control;
- handler replacement или result substitution;
- global trace singleton;
- implicit persistence trace в `LinkStore` или files;
- неограниченную гарантию complete trace;
- profiler timestamps;
- exception objects/strings в deterministic events;
- globally comparable numeric `LinkId` между stores;
- production trace-normalization registry;
- JSON/Anum-specific canonical trace events;
- отдельный interactive debugger runtime.

Будущий debugger может быть consumer этого boundary, но не вторым execution path.
