# Контракт scripted-команд инспекции

Command layer AVM 1.4 — parser и renderer вокруг типизированной `InspectionSession`. Это не язык исполнения и не источник string-named opcode AVM.

## Слои

```text
text line
  -> parse_inspection_command
  -> typed InspectionCommand variant
  -> execute_inspection_command
  -> typed InspectionSession operation
  -> существующие LinkStore / Relations / BootstrapRuntime / trace contracts
  -> typed InspectionResult variant
  -> render_inspection_result
  -> presentation text
```

Единственный string dispatch находится в parser. После успешного parsing execution dispatch идёт по C++ type команды. Runtime relation dispatch остаётся существующим `LinkId`-based path `Executor`.

Visitors command/result намеренно compile-time exhaustive: новая variant alternative без обработчика является build error, а не fallback action.

## Грамматика

Начальный scripted syntax намеренно мал и ориентирован на decimal `LinkId`:

```text
link <id>
find <begin> <end>
outgoing <begin>
incoming <end>
relation <entity>
query <relation|-> <subject|-> <object|->
function <handle>
frame <frame-id>
execute <root>
trace <root>
trace-reset
```

Tokens разделяются ASCII whitespace. `LinkId` записываются unsigned decimal. `-` зарезервирован только для отсутствующего constraint `RelationQuery`.

`query - - -` отклоняется до session execution, потому что Relations query требует хотя бы одно ограничение, а AVM не предоставляет guessed full-store enumeration.

## Граница ошибок

Parser errors являются tooling errors (`InspectionCommandError`):

- пустая команда;
- неизвестная команда;
- неверное число arguments;
- malformed/negative/overflowing `LinkId`;
- unconstrained query.

Они возникают до вызова `InspectionSession` и поэтому не могут изменить store или trace state.

После parsing ошибки typed operation сохраняют исходный смысл. Например `trace <root>` может распространить runtime exception AVM. Session при этом сохраняет bounded trace до точки отказа, который tooling может отдельно отобразить через `render_current_trace(session)`.

Exception string/type не вставляются в canonical `ExecutionEvent`.

## Типизированные результаты

Command execution возвращает typed result variants для link inspection, pair lookup, adjacency, Relations query/decode, function/frame inspection, execute и trace.

Направление adjacency задаётся enum; строковые labels появляются только в renderer.

Presentation strings вроде `link`, `outgoing`, `enter`, `fail`, `dispatch` не persist-ятся, не intern-ятся, не регистрируются как relations и не используются `Executor` для выбора semantics.

## Read-only и effect boundaries

Команды:

```text
link / find / outgoing / incoming / relation / query / function / frame
```

отображаются в read-only session operations. Parsing/rendering — host-only работа и не меняет `LinkStore`.

`execute` и `trace` намеренно вызывают существующий runtime и имеют ровно тот же effect contract, что прямой `BootstrapRuntime::execute`. `trace-reset` изменяет только bounded host tooling state.

## Детерминированный rendering

Rendering использует numeric `LinkId` и deterministic ordering underlying APIs. Trace показывает retained events в event order и всегда явно сообщает complete/truncated state.

Raw numeric `LinkId` ограничены одним logical store. Отображённые числа не являются cross-store identity protocol.

## Non-goals

Command layer не предоставляет:

- quoted/string data arguments или general expression grammar;
- JSON/Anum parsing;
- symbolic `LinkId` names;
- breakpoint/step/continue;
- handler registration/replacement;
- implicit `intern`/`create_point` commands;
- global store enumeration;
- trace persistence;
- второй executor/evaluator.
