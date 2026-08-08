# Контракт типизированной сессии инспекции

AVM 1.4 вводит host-side `InspectionSession`, которая композирует существующие API AVM 1.x. Это tooling, а не новый semantic layer.

## Граница

Typed session имеет один путь к каноническому состоянию и исполнению:

```text
InspectionSession
  -> существующие read operations LinkStore
  -> существующие Relations Model query/decode helpers
  -> существующие structural decoders functions/frames
  -> BootstrapRuntime::execute
  -> ExecutionObserver / BoundedExecutionTrace
```

Нет `DebugExecutor`, shell-specific relation registry, copied evaluator или shadow program database.

Implementation находится в `tools/` и намеренно не входит в installed `avm::core` headers этого gate. Стабильными публичными контрактами остаются API, которые tooling потребляет.

## Владение и lifetime

`InspectionSession` владеет своим `BootstrapRuntime` и bounded trace collector, а caller владеет `LinkStore`.

Это следует stable-address contract `Executor`/`BootstrapRuntime`: native handlers могут замыкать runtime owner, поэтому runtime не копируется и не перемещается. Следовательно session тоже neither copyable nor movable.

Session принимает `BootstrapVocabulary` по значению и передаёт явный набор identities runtime. Создание session над полным текущим vocabulary не должно выделять replacement bootstrap identities или увеличивать store.

Session не подключает collector к внешнему runtime. `Executor::set_observer` — non-owning setter без observer stack/getter; владение runtime предотвращает скрытую замену observer другого caller.

## Read-only inspection

Операции:

```text
inspect_link(id)
find_pair(begin,end)
outgoing(begin)
incoming(end)
query_relations(constraints)
decode_relation(entity)
function_definition(handle)
call_frame(frame)
```

являются observational и делегируют существующим `LinkStore`, Relations Model и program-model helpers.

Они не вызывают `intern`/`create_point`, не создают missing identities и не поддерживают второй entity index.

Missing exact pair остаётся missing. Malformed/missing entity может бросить ту же structural error, что underlying helper, но inspection не materialize-ит repair data.

## Исполнение и trace

`execute(root)` напрямую делегирует owned `BootstrapRuntime`.

`trace_execute(root)`:

1. сбрасывает owned `BoundedExecutionTrace`;
2. подключает его к тому же runtime executor;
3. выполняет один execution;
4. отключает collector и на success, и на exception path.

Original result/exception остаётся результатом runtime. Retained events используют неизменённый AVM observability contract.

Trace capacity фиксируется при construction. Exhaustion явно отражается через `trace_truncated()` и не превращается в implicit unbounded collector.

После failed traced execution события `Fail(phase)` остаются доступны. Последующий обычный `execute` не наблюдается, потому что collector отключён.

## Эффекты

Session не переопределяет execution effects. Программа, запущенная через session, выполняет те же canonical effects, что и при прямом `BootstrapRuntime::execute`: например materialization frames/bindings или explicit `pair_intern`.

Различие:

```text
inspection method   -> observation, store не растёт
execute/trace       -> существующий program/runtime effect contract
```

## Последующие tooling layers

Text/script command parser является отдельным presentation layer. Command strings отображаются в typed operations и не становятся relation identities AVM.

Persistent reopen также использует ту же `InspectionSession`, explicit persisted vocabulary/root identities и существующие LinkId/trace contracts.

## Non-goals

Session не добавляет:

- breakpoint/step/continue control;
- handler replacement/result substitution;
- symbolic `LinkId` registry;
- JSON/Anum parsing внутри session;
- implicit store enumeration;
- implicit trace persistence;
- backend-specific semantics;
- новые `LinkStore` methods/indexes.
