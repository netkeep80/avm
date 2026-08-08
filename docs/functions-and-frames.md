# Функции, bindings и call frames в AVM

Этот слой переносит runtime-state функций из исторического C++ side channel `func_env` / `param_stack` в тот же `LinkStore`, где находятся программы и данные.

## Определения функций

Функция использует encoding, заданный link-native program model:

```text
parameters = List(formal1, formal2, ...)
payload    = Link(parameters, bodyRoot)
definition = (function, handle, payload)
```

`handle` — независимо созданный `LinkId`. Поэтому recursive body может ссылаться на собственную функцию ещё до материализации immutable definition entity.

Выполнение definition валидирует сохранённую структуру и возвращает `nil`. Доступность определения структурна: link-native call напрямую ссылается на handle.

JSON compatibility importer отвечает только за сохранение textual declaration order и resolution имён в handles; сама runtime-функция не зависит от textual names.

## Связывания (bindings)

Каждый вычисленный actual argument связывается со своим formal parameter сущностью Модели Отношений:

```text
(binding, formal, actualValue)
```

В новом runtime нет `map<string, value>`. Formal identity — это `LinkId`, actual values — тоже `LinkId`.

## Кадры вызова (call frames)

Bindings собираются в канонический link-list и присоединяются к immutable call frame:

```text
bindings = List(binding1, binding2, ...)
payload  = Link(functionHandle, bindings)
frame    = (frame, parentFrameOrNil, payload)
```

`ExecutionContext` несёт текущий frame `LinkId`. Рекурсивное исполнение child expression сохраняет frame, пока function call явно не создаст дочерний frame.

Nested calls формируют parent-frame chain целиком в ассоциативном store. Parameter resolution идёт от текущего frame к `nil` и проверяет, что каждая используемая строка binding действительно имеет `binding_relation`.

## Выполнение call

Для `(call, unit, payload)` runtime выполняет:

1. декодирует function handle и список argument expressions;
2. находит immutable function definition;
3. проверяет arity;
4. проверяет текущую глубину frame относительно recursion limit;
5. вычисляет actual arguments в caller frame;
6. материализует binding entities;
7. материализует child frame, связанный с caller frame;
8. исполняет body функции в child frame.

Так lexical parameter identity сохраняется без глобального C++ stack. Recursive calls работают, потому что body уже содержит тот же function-handle `LinkId`.

## Ограничение рекурсии

`BootstrapRuntime` принимает максимальную call depth; значение по умолчанию — 1000.

Depth вычисляется обходом parent-frame links, а не размером C++ container. Malformed frame chains, non-frame parents и invalid frame payloads приводят к явным ошибкам.

Vocabulary identity отношения `frame` сама является self-link, как и другие bootstrap identities. Её нельзя путать с экземпляром frame; `decode_call_frame` явно отклоняет этот случай. Тест нужен потому, что relation identity естественно появляется в `LinkStore::outgoing(frame_relation)`.

## Материализация execution state

Bindings и frames являются реальными links. Поэтому function call может увеличивать `LinkStore`, даже если body содержит только наблюдающие operations.

Это не скрытый host-language side effect, а явная часть текущего link-native call-state contract.

AVM 1.5 должен учитывать это при формализации более общего execution context и distinguishing pure value computation от materialized manifestation/call state.

## Покрытие тестами

`function_runtime_tests` проверяет:

- функции с одним и двумя параметрами;
- Boolean expressions внутри body;
- nested calls;
- конечную recursion;
- unbounded recursion и depth guard;
- arity mismatch;
- undefined functions;
- unbound parameters;
- malformed frames/definitions;
- физическое наличие binding/frame entities в `LinkStore`.

`frame_runtime_tests` отдельно проверяет:

- структуру декодированного root frame;
- formal→actual binding rows;
- execution parameter с явным frame `LinkId`;
- parent/child linkage nested calls;
- отклонение frame vocabulary self-link;
- отклонение non-binding entries и invalid parents;
- запрет zero recursion-depth configuration.

Совместно с остальными suites это делает function semantics независимо тестируемой без JSON и без legacy LinksPlatform path.

## Инварианты

- function definitions находятся в links;
- bindings находятся в links;
- call frames находятся в links;
- runtime parameters не хранятся в string maps;
- recursion depth выводится из frame chain;
- textual function names принадлежат projection layer;
- function execution использует тот же `Executor` и canonical `LinkStore`.
