# AVM CI/CD policy

The AVM 1.0 core is intentionally testable without the legacy JSON interpreter or the optional LinksPlatform dependency. JSON projection tests are also independently buildable, so the compatibility boundary can be verified without compiling the historical semantic implementation.

## CMake switches

- `AVM_BUILD_LEGACY=ON|OFF` — build the historical JSON-facing executable and legacy serializer/interpreter tests.
- `AVM_BUILD_CORE_TESTS=ON|OFF` — build the link-native AVM 1.0 core suites.
- `AVM_BUILD_JSON_COMPAT_TESTS=ON|OFF` — build the new JSON projection tests and, when legacy is enabled, the temporary old-vs-new conformance suite.
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

The JSON projection can be checked without legacy execution semantics:

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

The C++ suites use `assert(...)` for invariant checks. CMake Release configurations normally define `NDEBUG`, which would compile those checks out. Therefore every C++ test target explicitly undefines `NDEBUG` (`-UNDEBUG` or `/UNDEBUG`) regardless of build type.

`assertions_enabled_tests` additionally fails at compile time if `NDEBUG` reaches a core test target and verifies at runtime that an assertion expression is actually evaluated. The strict lanes use Debug to maximize diagnostic visibility; the portable matrix still exercises Release builds with assertions forced active in test executables.

## Pull-request gates

The `CI` workflow separates five concerns:

1. `Quality gates` — source-size policy and strict clang-format verification for the AVM 1.0 core and compatibility source files.
2. `Core C++20 / warnings-as-errors` — Linux Debug core-only build with strict warnings and CTest.
3. `JSON projection / warnings-as-errors` — Linux Debug build of the new JSON projection path with legacy execution disabled.
4. `Core + JSON ASan + UBSan` — core and JSON projection suites under AddressSanitizer and UndefinedBehaviorSanitizer.
5. `Portable / <os>` — full Release build and all tests on Linux, Windows and macOS. During #47 this includes the temporary old-vs-new JSON conformance suite.

Recommended branch-protection required checks for `main` are:

- `Quality gates`;
- `Core C++20 / warnings-as-errors`;
- `JSON projection / warnings-as-errors`;
- `Core + JSON ASan + UBSan`.

The portable matrix should also remain green before merge. Separating fast gates keeps the link-native core and the JSON projection independently diagnosable while the historical path is being removed.

## Test-suite growth rule

Every new semantic layer must have a separately named CTest suite and an aggregate target appropriate to its boundary.

Core progression:

- `assertions_enabled_tests`;
- `link_store_tests`;
- `relations_model_tests`;
- `executor_tests`;
- `program_model_tests`;
- `boolean_runtime_tests`;
- `function_runtime_tests`;
- `frame_runtime_tests`;
- `deferred_definition_tests`.

Compatibility migration:

- `json_projection_tests` — permanent intended behavior of JSON -> links -> runtime -> result projection;
- `json_legacy_conformance_tests` — temporary side-by-side migration guard, removed after #48 deletes the legacy semantic interpreter.

Tests should prefer explicit invariants over broad snapshots: canonical identity reuse, non-mutating lookup, malformed-link rejection, lazy branch behavior, frame materialization, declaration order, recursion bounds, projection independence from source JSON lifetime and adapter-vs-core error semantics should each be asserted directly.

## Tagged delivery

A push of a tag matching `v*` builds the Linux `avm` executable and uploads it as a workflow artifact named `avm-<tag>-linux-x86_64` after the core, JSON projection and sanitizer gates pass.

This is deliberately a minimal CD step. Publishing a GitHub Release, binary signing and a multi-platform release bundle should be enabled only after the versioning/signing policy and the legacy-to-AVM-1.0 executable transition are settled.
