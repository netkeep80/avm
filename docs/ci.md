# AVM CI/CD policy

The AVM 1.0 semantic core is independently testable without the historical `rel_t` JSON codec or LinksPlatform dependency. JSON projection is a separate adapter layer, and the remaining legacy outward API is tested as a facade over the same link-native runtime rather than as a second interpreter.

## CMake switches

- `AVM_BUILD_LEGACY=ON|OFF` — build the historical `rel_t` JSON data codec, thin compatibility facade and CLI.
- `AVM_BUILD_CORE_TESTS=ON|OFF` — build the link-native AVM 1.0 core suites.
- `AVM_BUILD_JSON_COMPAT_TESTS=ON|OFF` — build the JSON projection/runtime compatibility suite.
- `AVM_WARNINGS_AS_ERRORS=ON|OFF` — enable strict compiler diagnostics for targets using the AVM quality profile.
- `AVM_ENABLE_SANITIZERS=ON|OFF` — enable AddressSanitizer and UndefinedBehaviorSanitizer on supported non-MSVC toolchains.

The fast core validation command is:

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_LEGACY=OFF \
  -DAVM_BUILD_CORE_TESTS=ON \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-core --target avm_core_tests --parallel
ctest --test-dir build-core --output-on-failure
```

The JSON projection can be checked without `rel_t` or the CLI:

```bash
cmake -S . -B build-json \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_LEGACY=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=ON \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-json --target avm_json_compat_tests --parallel
ctest --test-dir build-json -R json_projection_tests --output-on-failure
```

## Assertions are part of the test contract

The C++ suites use `assert(...)` for invariant checks. CMake Release configurations normally define `NDEBUG`, which would compile those checks out. Every C++ test target explicitly undefines `NDEBUG` (`-UNDEBUG` or `/UNDEBUG`) regardless of build type.

`assertions_enabled_tests` fails at compile time if `NDEBUG` reaches a core test target and verifies at runtime that an assertion expression is evaluated. Strict semantic lanes use Debug; the portable matrix still exercises Release builds with assertions forced active in test executables.

## Pull-request gates

The `CI` workflow separates six concerns:

1. `Quality gates` — source-size policy, removal guard for `resolve_operator`/`func_def`/`param_stack`, and strict clang-format verification.
2. `Core C++20 / warnings-as-errors` — Linux Debug core-only build and CTest.
3. `JSON projection / warnings-as-errors` — Linux Debug build of JSON -> links -> runtime -> JSON with legacy facade disabled.
4. `Legacy facade / warnings-as-errors` — strict build of the CLI and compatibility facade plus projection and roundtrip tests. This proves the historical outward API delegates to the new semantic path.
5. `Core + JSON ASan + UBSan` — the link-native core and JSON projection under AddressSanitizer and UndefinedBehaviorSanitizer.
6. `Portable / <os>` — full Release build and all permanent tests on Linux, Windows and macOS.

Recommended branch-protection required checks for `main` are:

- `Quality gates`;
- `Core C++20 / warnings-as-errors`;
- `JSON projection / warnings-as-errors`;
- `Legacy facade / warnings-as-errors`;
- `Core + JSON ASan + UBSan`.

The portable matrix should also remain green before merge.

## Single-semantic-path guard

The historical recursive JSON interpreter was removed after its behavior was compared against the new projection/runtime path. CI now rejects production source containing the removed side-channel identifiers:

- `resolve_operator`;
- `func_def`;
- `param_stack`.

The compatibility function name `clear_func_env()` remains only as an API reset shim. It destroys the current `JsonCompatibilitySession`; there is no `func_env` data structure behind it.

## Test suites

Core:

- `assertions_enabled_tests`;
- `link_store_tests`;
- `relations_model_tests`;
- `executor_tests`;
- `program_model_tests`;
- `boolean_runtime_tests`;
- `function_runtime_tests`;
- `frame_runtime_tests`;
- `deferred_definition_tests`;
- `projection_tests` — parser-independent `ProjectionDescription`, read-only `find_projection`, explicit `realize_projection`, canonical reuse, invalid graph/anchor rejection and no-partial-write checks for missing anchors.

Compatibility:

- `json_projection_tests` — permanent JSON projection/runtime contract;
- `legacy_facade_tests` — outward `interpret(json)` compatibility shim, persistent Def/Call session behavior, recursion, lazy If and sequence error compatibility;
- `unit_tests` — historical `rel_t` data-codec and outward-behavior regression suite;
- `json_roundtrip_*` — CLI/data-codec roundtrip fixtures.

The temporary `json_legacy_conformance_tests` suite was deleted together with the old interpreter after it had passed on Linux, Windows and macOS. Keeping it after that point would require keeping two production semantic implementations merely so they could compare each other.

## Tagged delivery

A push of a tag matching `v*` builds the Linux `avm` executable and uploads it as a workflow artifact named `avm-<tag>-linux-x86_64` after the core, JSON projection, legacy-facade and sanitizer gates pass.

This remains deliberately minimal. GitHub Release publication, signing and multi-platform release bundles should be added only after versioning/signing policy and the final replacement of the historical `rel_t` data codec are decided.
