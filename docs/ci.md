# Политика CI/CD AVM

AVM имеет один production storage/identity path — `LinkStore`. Core execution, JSON projection/value roundtrip, Anum structural adapter, tooling и persistence проверяются как отдельные concerns над этим фундаментом.

## Настройки CMake

- `AVM_BUILD_CLI=ON|OFF` — собирать JSON CLI и file-based fixtures;
- `AVM_BUILD_CORE_TESTS=ON|OFF` — собирать link-native core suites;
- `AVM_BUILD_JSON_COMPAT_TESTS=ON|OFF` — собирать JSON projection/session suites;
- `AVM_BUILD_ANUM_ADAPTER_TESTS=ON|OFF` — собирать conformance Anum L3→L4 adapter;
- `AVM_WARNINGS_AS_ERRORS=ON|OFF` — включать strict compiler diagnostics;
- `AVM_ENABLE_SANITIZERS=ON|OFF` — включать ASan/UBSan на поддерживаемых non-MSVC toolchains.

Быстрая проверка core:

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=ON \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DAVM_BUILD_ANUM_ADAPTER_TESTS=ON \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-core --target avm_core_tests avm_anum_adapter_tests --parallel
ctest --test-dir build-core --output-on-failure
```

Независимая JSON compatibility validation:

```bash
cmake -S . -B build-json \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=ON \
  -DAVM_BUILD_ANUM_ADAPTER_TESTS=OFF \
  -DAVM_WARNINGS_AS_ERRORS=ON
cmake --build build-json --target avm_json_compat_tests --parallel
ctest --test-dir build-json --output-on-failure
```

## Assertions входят в test contract

C++ suites используют `assert(...)` для инвариантов. Release configuration обычно определяет `NDEBUG`, что отключило бы проверки. Поэтому test targets явно undefine-ят `NDEBUG` (`-UNDEBUG` или `/UNDEBUG`) независимо от build type.

`assertions_enabled_tests` проверяет это compile-time/runtime. Strict semantic lanes работают в Debug; portable matrix также выполняет Release tests с активными assertions.

## Основные pull-request gates

Workflow `CI` разделяет несколько независимых проверок.

### `Quality gates`

Защищает архитектурные ограничения:

- лимит размера source files;
- запрет возврата удалённых JSON semantic side channels;
- запрет возврата `rel_t` storage path;
- isolation `RawCarrier`;
- запрет Anum grammar/parser coupling в AVM core;
- structural-only Anum adapter;
- запрет старого protocol-only Anum bridge;
- read-only contract Relations query;
- isolation execution observer/trace;
- trace CLI остаётся consumer canonical runtime;
- inspection tooling не определяет собственный executor/storage semantics;
- public version contract;
- `clang-format --Werror`.

### `Core C++20 / warnings-as-errors`

Linux Debug build core + Anum adapter suites с strict warnings.

### `JSON projection + session / warnings-as-errors`

Проверяет JSON program projection и stateful compatibility session без CLI.

### `CLI + JSON roundtrip / warnings-as-errors`

Проверяет executable и file-based JSON fixtures через реальный CLI.

### `Core + JSON + Anum adapter ASan + UBSan`

Запускает core, JSON и Anum adapter под sanitizers.

### `Installed package consumer / <os>`

Устанавливает AVM во временный prefix и собирает внешний consumer через `find_package(avm ...)` на Linux, Windows и macOS.

### `Portable / <os>`

Полный Release build и tests на Linux, Windows и macOS.

Перед merge все затронутые semantic/portable gates должны быть зелёными. Markdown-only PR может не запускать основной CI из-за `paths-ignore`; для documentation contracts используется отдельный focused audit.

## Guards удалённых путей

Исторический recursive JSON interpreter удалён. CI запрещает identifiers:

```text
resolve_operator
func_def
param_stack
```

в production `src/include`.

После полной миграции удалён и pointer-based storage compatibility path. CI запрещает:

```text
rel_t
legacy_json_compat
UnitedMemoryLinks
```

Это не позволяет удобному compatibility patch скрыто вернуть второй storage/identity universe.

## Guards протокольного слоя

`RawCarrier` остаётся storage-only и не зависит от semantic AVM headers, JSON, Anum или abits.

Production `src`/`include/avm` не может напрямую зависеть от Anum grammar/parser. Canonical tokenization, quotation/context projection и L3 semantics находятся вне VM. Handoff в AVM — completed structural denotation/`ProjectionDescription` с externally resolved anchors.

Production Anum adapter также остаётся JSON-free; JSON используется только для versioned conformance snapshots в tests.

## Guards для query/observer/tooling

CI отдельно фиксирует, что:

- `relations_query.h` не materialize-ит links и не угадывает full-store enumeration;
- `execution_observer.h` не содержит backend/protocol/host exception identity;
- `execution_trace.h` остаётся collector-only tooling;
- CLI trace не регистрирует собственные handlers;
- inspection tooling использует существующие `BootstrapRuntime`/`Executor` и read/query APIs.

## Наборы тестов

Core включает suites для:

- assertions;
- `LinkStore` и persistence;
- Relations Model encode/decode/query;
- Executor/program model;
- Boolean runtime;
- functions/frames/deferred definitions;
- projection/RawCarrier/protocol boundary;
- structural library;
- observability/trace/persistent trace;
- inspection session/commands/persistent inspection;
- vertical slice;
- JSON value codec, где он входит в aggregate core validation.

Отдельно проверяются JSON compatibility/session, Anum adapter conformance, CLI fixtures и package consumer.

## Отдельный gate semantic inventory jsonRVM

`.github/workflows/jsonrvm-compatibility.yml` проверяет versioned manifest и frozen golden assertions из AVM 1.5 #123.

Validator является metadata/conformance tool и **не** исполняет `jsonRVM`; он не создаёт второй interpreter.

## Workflow измерений производительности

`.github/workflows/benchmark.yml` отделён от correctness CI. Он собирает performance baseline, проверяет TSV schema/expected operations и публикует artifact.

Nanosecond values shared runner являются наблюдениями, а не merge-veto thresholds.

## Публикация по тегу

Tag `v*` может создать Linux artifact только после успешных:

```text
core
package-consumer
json-compat
cli-roundtrip
sanitizers
portable
```

Таким образом Linux artifact намеренно gated полной Linux/Windows/macOS portable matrix.

GitHub Release publication, signing и multi-platform binary bundles остаются отдельной delivery policy.
