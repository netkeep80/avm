# AVM CI/CD policy

AVM 1.0 has one production storage/identity path: `LinkStore`. JSON program projection, JSON value roundtrip and runtime execution are tested as separate concerns over that path.

## CMake switches

- `AVM_BUILD_CLI=ON|OFF` — build the JSON CLI and its file roundtrip fixtures.
- `AVM_BUILD_CORE_TESTS=ON|OFF` — build link-native AVM 1.0 core suites.
- `AVM_BUILD_JSON_COMPAT_TESTS=ON|OFF` — build JSON program projection/session compatibility suites.
- `AVM_WARNINGS_AS_ERRORS=ON|OFF` — enable strict compiler diagnostics.
- `AVM_ENABLE_SANITIZERS=ON|OFF` — enable AddressSanitizer and UndefinedBehaviorSanitizer on supported non-MSVC toolchains.

Fast core validation:

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=ON \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-core --target avm_core_tests --parallel
ctest --test-dir build-core --output-on-failure
```

Independent JSON program/session validation:

```bash
cmake -S . -B build-json \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=ON \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-json --target avm_json_compat_tests --parallel
ctest --test-dir build-json --output-on-failure
```

## Assertions are part of the test contract

The C++ suites use `assert(...)` for invariant checks. CMake Release configurations normally define `NDEBUG`, which would compile those checks out. Every C++ test target explicitly undefines `NDEBUG` (`-UNDEBUG` or `/UNDEBUG`) regardless of build type.

`assertions_enabled_tests` fails at compile time if `NDEBUG` reaches a core test target and verifies at runtime that an assertion expression is evaluated. Strict semantic lanes use Debug; the portable matrix also exercises Release builds with assertions forced active in test executables.

## Pull-request gates

The `CI` workflow separates six concerns:

1. `Quality gates` — source-size policy, removed semantic/storage path guards, RawCarrier isolation, external Anum/parser boundary and strict clang-format verification.
2. `Core C++20 / warnings-as-errors` — Linux Debug core-only build and CTest, including the JSON value codec.
3. `JSON projection + session / warnings-as-errors` — JSON program projection and stateful compatibility-session behavior without the CLI.
4. `CLI + JSON roundtrip / warnings-as-errors` — built executable and file-based JSON data roundtrip fixtures.
5. `Core + JSON ASan + UBSan` — core, JSON value codec and program/session projection under sanitizers.
6. `Portable / <os>` — full Release build and all permanent tests on Linux, Windows and macOS.

Recommended required checks for `main` are:

- `Quality gates`;
- `Core C++20 / warnings-as-errors`;
- `JSON projection + session / warnings-as-errors`;
- `CLI + JSON roundtrip / warnings-as-errors`;
- `Core + JSON ASan + UBSan`.

The portable matrix should also be green before merge.

## Removed-path guards

The historical recursive JSON interpreter was removed after conformance migration. CI rejects the old semantic side-channel identifiers:

- `resolve_operator`;
- `func_def`;
- `param_stack`.

The later pointer-based `rel_t` data/storage facade was also removed after its consumers migrated to `JsonValueCodec` and `JsonCompatibilitySession`. CI rejects production/build references to:

- `rel_t`;
- `legacy_json_compat`;
- `UnitedMemoryLinks`.

This prevents an apparently convenient compatibility patch from silently reintroducing a second storage/identity universe.

## Protocol-layer guards

`RawCarrier` remains storage-only. CI rejects AVM semantic includes, JSON, Anum or abit references in `raw_carrier.h`.

Production `src`/`include/avm` are also checked for direct Anum/abit parser coupling. Canonical grammar, tokenization, quotation/context projection and the L3 parser belong outside AVM. The stable handoff into the VM is a completed `ProjectionDescription` over externally resolved `Anchor(LinkId)` values.

## Test suites

Core includes:

- `assertions_enabled_tests`;
- `link_store_tests`;
- `relations_model_tests`;
- `executor_tests`;
- `program_model_tests`;
- `boolean_runtime_tests`;
- `function_runtime_tests`;
- `frame_runtime_tests`;
- `deferred_definition_tests`;
- `projection_tests`;
- `raw_carrier_tests`;
- `protocol_adapter_boundary_tests`;
- `persistent_link_store_tests`;
- `vertical_slice_tests`;
- `json_value_codec_tests`.

JSON compatibility includes:

- `json_projection_tests` — permanent JSON program projection/runtime contract;
- `json_session_tests` — persistent Def/Call session behavior, recursion, lazy If and sequence-error compatibility.

CLI compatibility includes `json_roundtrip_*` fixtures. They now exercise `JsonValueCodec`; there is no `rel_t` conversion behind the executable.

## Benchmark workflow

`.github/workflows/benchmark.yml` is intentionally separate from correctness CI. It builds and runs the performance baseline, validates the TSV schema/expected operations and uploads the result as an artifact. Shared-runner nanosecond values are observations, not merge veto thresholds.

## Tagged delivery

A `v*` tag builds the Linux `avm` executable and uploads it as a workflow artifact after core, JSON compatibility, CLI roundtrip and sanitizer gates pass.

GitHub Release publication, signing and multi-platform release bundles remain separate delivery-policy work.
