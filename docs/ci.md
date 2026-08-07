# AVM CI/CD policy

The AVM 1.0 core is intentionally testable without the legacy JSON interpreter or the optional LinksPlatform dependency.

## CMake switches

- `AVM_BUILD_LEGACY=ON|OFF` — build the historical JSON-facing executable and compatibility tests.
- `AVM_BUILD_CORE_TESTS=ON|OFF` — build the link-native AVM 1.0 test suites.
- `AVM_WARNINGS_AS_ERRORS=ON|OFF` — enable strict compiler diagnostics for targets using the core quality profile.
- `AVM_ENABLE_SANITIZERS=ON|OFF` — enable AddressSanitizer and UndefinedBehaviorSanitizer on supported non-MSVC toolchains.

The fast validation command is:

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Release \
  -DAVM_BUILD_LEGACY=OFF \
  -DAVM_BUILD_CORE_TESTS=ON \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-core --target avm_core_tests --parallel
ctest --test-dir build-core --output-on-failure
```

## Pull-request gates

The `CI` workflow has four independent concerns:

1. `Quality gates` — source-size policy and strict clang-format verification for the AVM 1.0 core files.
2. `Core C++20 / warnings-as-errors` — Linux core-only build with strict warnings and CTest.
3. `Core ASan + UBSan` — Debug core-only build under AddressSanitizer and UndefinedBehaviorSanitizer.
4. `Portable / <os>` — full compatibility build and test matrix on Linux, Windows and macOS.

Recommended branch-protection required checks for `main` are:

- `Quality gates`;
- `Core C++20 / warnings-as-errors`;
- `Core ASan + UBSan`.

The portable matrix should also stay green, but the three fast core gates are the minimum architectural merge barrier. This separation keeps the new link-native core independently verifiable even if a legacy/platform-specific dependency is temporarily unavailable.

## Test-suite growth rule

Every new AVM 1.0 semantic layer must add a separately named CTest suite and be included in the `avm_core_tests` aggregate target. The intended progression is:

- `link_store_tests`;
- `relations_model_tests`;
- `executor_tests`;
- program-model tests;
- Boolean/If runtime tests;
- function/frame/recursion tests;
- JSON compatibility/conformance tests during the migration phase.

Tests should prefer explicit invariants over broad end-to-end snapshots: canonical identity reuse, non-mutating lookup, malformed-link rejection, lazy branch behavior, frame materialization and recursion bounds should each be asserted directly.

## Tagged delivery

A push of a tag matching `v*` builds the Linux `avm` executable and uploads it as a workflow artifact named `avm-<tag>-linux-x86_64` after the core and sanitizer gates pass.

This is deliberately a minimal CD step. Publishing a GitHub Release, binary signing and a multi-platform release bundle should be enabled only after the versioning/signing policy and the legacy-to-AVM-1.0 executable transition are settled.
