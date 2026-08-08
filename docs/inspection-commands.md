# Scripted inspection command contract

The AVM 1.4 command layer is a parser and renderer around the typed `InspectionSession`. It is not an execution language and does not introduce string-named AVM opcodes.

## Layering

```text
text line
  -> parse_inspection_command
  -> typed InspectionCommand variant
  -> execute_inspection_command
  -> typed InspectionSession operation
  -> existing LinkStore / Relations / BootstrapRuntime / trace contracts
  -> typed InspectionResult variant
  -> render_inspection_result
  -> presentation text
```

The only string dispatch occurs in the parser. Once parsing succeeds, execution dispatch is by C++ command type. Runtime relation dispatch remains the existing LinkId-based `Executor` path. Command/result visitors are intentionally compile-time exhaustive: adding a new variant alternative without handling it is a build error rather than a fallback action.

## Grammar

The initial scripted grammar is deliberately small and decimal-LinkId oriented:

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

Tokens are separated by ASCII whitespace. LinkIds use unsigned decimal syntax. `-` is reserved only for an absent Relations query constraint.

`query - - -` is rejected before session execution because the underlying Relations query contract requires at least one constraint and AVM intentionally exposes no guessed full-store enumeration.

## Error boundary

Parser errors are tooling errors (`InspectionCommandError`). Examples include:

- empty command;
- unknown command;
- wrong argument count;
- malformed/negative/overflowing LinkId;
- unconstrained query.

They occur before an `InspectionSession` operation is invoked and therefore cannot mutate the store or trace state.

After a command is parsed, errors from the typed operation retain their existing meaning. For example, `trace <root>` may propagate an AVM runtime exception. The session keeps the bounded trace that was observed before failure, so tooling can render `render_current_trace(session)` separately from the host exception diagnostic.

No exception string or C++ exception type is inserted into canonical `ExecutionEvent` data.

## Typed results

Command execution returns typed result variants for link inspection, pair lookup, adjacency, relation query/decode, function/frame inspection, execute and trace operations. Adjacency direction is represented by an enum; string labels appear only in the renderer.

Presentation strings such as `link`, `outgoing`, `enter`, `fail` and `dispatch` are renderer labels only. They are not persisted, interned, registered as relations or used by `Executor` to choose semantics.

## Read-only and effect boundaries

The commands

```text
link / find / outgoing / incoming / relation / query / function / frame
```

map to read-only session operations. Parsing and rendering are host-only work and must not alter `LinkStore`.

`execute` and `trace` deliberately invoke the existing runtime and therefore have exactly the same effect contract as direct `BootstrapRuntime::execute`. `trace-reset` changes only bounded host tooling state.

## Deterministic rendering

Rendering uses numeric LinkIds and deterministic ordering inherited from the underlying APIs. Trace rendering presents retained events in event order and always prints explicit completeness/truncation state.

Raw numeric LinkIds remain scoped to one logical store. Rendered numbers are not a cross-store identity protocol.

## Non-goals

This command layer does not provide:

- quoted/string data arguments or a general expression grammar;
- JSON or Anum parsing;
- symbolic LinkId names;
- breakpoint/step/continue;
- handler registration/replacement;
- implicit `intern`/`create_point` commands;
- global store enumeration;
- trace persistence;
- a second executor or evaluator.
