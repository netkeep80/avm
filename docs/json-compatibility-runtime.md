# JSON compatibility is a projection layer

AVM 1.0 does not execute a JSON AST. JSON remains supported as an external compatibility syntax, but the only semantic path is:

```text
JSON
  -> JsonProgramImporter
  -> LinkStore program graph
  -> BootstrapRuntime / Executor
  -> result LinkId
  -> JSON result projection
```

Once `import_program()` returns a root LinkId, execution no longer depends on the source JSON object. `json_projection_tests` explicitly replace the source JSON value before executing the imported graph.

## Text names stop at the importer

Operator names (`Not`, `And`, `Or`, `If`, `Def`, `Call`), function names and formal parameter names are projection syntax. `JsonProgramImporter` resolves them to opaque LinkIds.

The runtime receives:

- relation LinkIds for dispatch;
- function-handle LinkIds;
- formal-parameter LinkIds;
- link-native call frames and bindings;
- expression payloads represented as canonical dyads.

There is no `resolve_operator`, JSON function-body map or parameter-name stack in production execution.

## Function symbols and deferred Def

The importer may allocate a function handle before its definition is executed. This supports recursion and forward references in the projected graph.

A JSON `Def` becomes the deferred definition expression defined by #46. Merely importing it does not create the callable function-definition row. Execution of that node performs materialization, preserving `Def`/`Call` order.

A subsequent syntactic redefinition receives a new handle so later projected calls can refer to the replacement definition without mutating an existing immutable definition row.

## JSON literals

Boolean and null values map directly to bootstrap vocabulary identities.

Other scalar JSON values currently use projection-owned opaque point identities. The importer maintains a type-aware JSON-value-to-LinkId table and the reverse LinkId-to-JSON table for result projection. This is intentionally a compatibility mechanism, not the final AVM primitive-data model.

A later storage/data-model issue can replace these opaque values without changing execution semantics.

## Compatibility sequence

The link-native core `sequence_relation` is fail-fast: an exception in a child expression aborts execution. The historical JSON-facing API returned null for malformed/undefined child expressions and continued with later array elements.

To preserve that outward behavior without weakening the core, `JsonCompatibilitySession` owns a separate compatibility sequence relation. Its handler executes each already-projected child through `Executor`, converts a child runtime failure to `nil`, and continues to the next expression.

Compatibility policy therefore remains at the adapter boundary rather than becoming a VM invariant.

## Error boundary

`JsonProgramImporter` throws `JsonProjectionError` for malformed syntax. Lower AVM layers throw explicit runtime/logic errors for malformed link structures or execution failures.

`JsonCompatibilitySession::interpret()` is the JSON-shaped convenience facade: it maps those failures to JSON `null`. Direct users of `import_program()` and `execute()` retain explicit errors.

## Historical `interpret(json)` API

The old C++ API symbol is retained temporarily for source compatibility, but it no longer interprets JSON recursively:

```text
interpret(json)
  -> persistent JsonCompatibilitySession
  -> JSON result
  -> legacy import_json(result)
  -> rel_t* outward value
```

`clear_func_env()` is likewise only a compatibility name. It resets the persistent `JsonCompatibilitySession`; there is no function-environment map or parameter stack behind it.

The historical `import_json` / `export_json` functions remain as a data codec for callers and roundtrip tests that still consume `rel_t*`. They do not define program execution semantics.

## Migration evidence

Before removing the historical recursive interpreter, `json_legacy_conformance_tests` executed a representative corpus through both implementations. The suite passed in the portable Linux, Windows and macOS CI matrix together with the independent JSON projection tests and ASan/UBSan checks.

After that evidence was obtained, the duplicate interpreter and the side-by-side harness were deleted. `json_projection_tests`, `legacy_facade_tests`, the historical data-codec tests and portable CI now protect the single production semantic path.
