# JSON compatibility is a projection layer

AVM 1.0 does not execute a JSON AST. JSON remains supported as an external compatibility syntax, but the semantic path is now:

```text
JSON
  -> JsonProgramImporter
  -> LinkStore program graph
  -> BootstrapRuntime / Executor
  -> result LinkId
  -> JSON result projection
```

Once `import_program()` returns a root LinkId, execution no longer depends on the source JSON object. The conformance suite explicitly destroys/replaces the source JSON value before executing the imported graph.

## Text names stop at the importer

Operator names (`Not`, `And`, `Or`, `If`, `Def`, `Call`), function names and formal parameter names are projection syntax. `JsonProgramImporter` resolves them to opaque LinkIds.

The runtime receives:

- relation LinkIds for dispatch;
- function-handle LinkIds;
- formal-parameter LinkIds;
- link-native call frames and bindings;
- expression payloads represented as canonical dyads.

There is no string operator dispatcher, function-name map or parameter-name map in `Executor` or `BootstrapRuntime`.

## Function symbols and deferred Def

The importer may allocate a function handle before its definition is executed. This supports recursion and forward references in the projected graph.

A JSON `Def` becomes the deferred definition expression defined by #46. Merely importing it does not create the callable function-definition row. Execution of that node performs materialization, preserving `Def`/`Call` order.

A subsequent syntactic redefinition receives a new handle so later projected calls can refer to the replacement definition without mutating an existing immutable definition row.

## JSON literals

Boolean and null values map directly to bootstrap vocabulary identities.

Other scalar JSON values currently use projection-owned opaque point identities. The importer maintains a type-aware JSON-value-to-LinkId table and the reverse LinkId-to-JSON table for result projection. This is intentionally a compatibility mechanism, not the final AVM primitive-data model.

A later storage/data-model issue can replace these opaque values without changing execution semantics.

## Compatibility sequence

The link-native core `sequence_relation` is fail-fast: an exception in a child expression aborts execution. The historical JSON interpreter behaved differently because malformed/undefined child expressions returned `E`, and an array continued with later elements.

To preserve that behavior without weakening the core, `JsonCompatibilitySession` owns a separate compatibility sequence relation. Its handler executes each already-projected child through `Executor`, converts a child runtime failure to `nil`, and continues to the next expression.

Thus compatibility policy remains at the adapter boundary rather than becoming a VM invariant.

## Error boundary

`JsonProgramImporter` throws `JsonProjectionError` for malformed syntax. Lower AVM layers throw explicit runtime/logic errors for malformed link structures or execution failures.

`JsonCompatibilitySession::interpret()` is the legacy-shaped convenience facade: it maps those failures to JSON `null`. Direct users of `import_program()` and `execute()` retain explicit errors.

## Conformance strategy

During migration, two independent suites are used:

1. `json_projection_tests` asserts intended behavior of the new projection/runtime path without depending on the old semantic interpreter.
2. `json_legacy_conformance_tests` executes a representative corpus through both the historical interpreter and the new path and compares projected results.

The side-by-side suite exists only for the migration window. After conformance is established and the old semantic interpreter is deleted in #48, the independent projection suite becomes the permanent compatibility contract.
