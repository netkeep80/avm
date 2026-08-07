# Removal of the legacy JSON semantic interpreter

AVM 1.0 has one production execution model. The historical recursive JSON interpreter was removed after the link-native path passed side-by-side conformance on Linux, Windows and macOS.

## Removed production state

The old `src/main.cpp` combined unrelated responsibilities and contained a second virtual-machine implementation based on:

- string-to-operator dispatch in `resolve_operator()`;
- `func_def` structures containing JSON function bodies;
- the global `func_env` function-name map;
- the global `param_stack` parameter-binding stack;
- recursive traversal of JSON nodes in `interpret(const json&)`.

Those mechanisms no longer exist in the working tree. Git remains the history of the implementation; no dead copy is retained in production source.

## Replacement source boundaries

`src/cli.cpp`
: File I/O and the command-line compatibility decision: treat the input as an AVM JSON expression or as historical JSON data. Operator strings here are syntax recognition only; they never select runtime semantics.

`include/avm/json_compat.h`
: JSON syntax projection into the link-native program graph and result projection back to JSON.

`src/legacy_json_compat.cpp`
: Historical `rel_t` data codec and a thin outward `interpret(json)` shim. The shim delegates execution to `JsonCompatibilitySession` and converts the resulting JSON value to the old `rel_t*` return representation.

`BootstrapRuntime` / `Executor`
: The only production semantic execution path.

## Compatibility reset

`clear_func_env()` is kept temporarily because existing callers/tests use the name. Its implementation now only destroys the persistent `JsonCompatibilitySession`. It does not clear a hidden second function environment because no such environment remains.

## Why the old conformance harness was deleted

During #47, `json_legacy_conformance_tests` was valuable because two semantic implementations genuinely existed. It compared the old interpreter with JSON -> LinkStore -> Executor for Boolean operations, control flow, functions, recursion, errors and scalar results.

Once #47 was green across all supported CI operating systems, retaining that test would require retaining the obsolete implementation solely to compare against itself forever. #48 therefore deletes both at the same architectural boundary.

Permanent coverage is now provided by:

- independent core suites;
- `json_projection_tests`;
- `legacy_facade_tests`;
- historical `rel_t` codec/unit tests;
- CLI JSON roundtrip fixtures;
- strict warnings and formatting gates;
- ASan/UBSan for core + JSON projection;
- Linux/Windows/macOS portable builds.

## CI regression guard

The quality job rejects reintroduction of the former semantic side-channel identifiers `resolve_operator`, `func_def` or `param_stack` anywhere under production `src`/`include`.

This is deliberately stronger than a documentation promise: a pull request that recreates those mechanisms cannot pass the required quality gate without explicitly changing the architecture policy.
