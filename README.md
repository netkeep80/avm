<p align="center"><img src="EOSR.jpg"></p>

# AVM — Associative Virtual Machine

AVM is an experimental C++20 virtual machine built on the Relations Model and a canonical store of directed links.

**Current public version: 1.0.0.**

## AVM 1.0 semantic path

There is one production semantic path:

```text
external representation
  -> projection / adapter
  -> canonical LinkStore
  -> Relations Model entity codec
  -> LinkId program
  -> BootstrapRuntime / Executor
  -> result LinkId
```

JSON is an external projection and compatibility protocol. It is not the VM AST or the execution core. Execution dispatches by relation `LinkId`, not by textual operator names.

The physical primitive is a canonical directed dyad:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

`LinkId` is opaque. Reads do not materialize missing links, and `intern(a,b)` returns the canonical identity for an exact pair inside one logical store.

## Current foundation

AVM 1.0 currently provides:

- `LinkStore` as the storage contract, with in-memory and persistent reference implementations;
- Relations Model encoding `(relation, subject, object) = (relation, (subject, object))`;
- explicit bootstrap vocabulary identity;
- link-native execution through `BootstrapRuntime` and `Executor`;
- associative program structures, bindings and call frames represented by links;
- a neutral protocol-adapter boundary that keeps parsing/context semantics outside the VM core;
- separate JSON value and JSON program/session adapters at the projection boundary;
- persistent reopen/identity conformance tests independent from VM semantics;
- CI on Linux, Windows and macOS, warnings-as-errors lanes, sanitizers, quality guards and benchmark baselines.

The former pointer-based `rel_t` universe and its legacy execution/data-codec path have been removed. Git history preserves that implementation; AVM does not keep a second semantic path for compatibility.

## Supported core library API

The supported dependency-free C++ core surface is available through:

```cpp
#include <avm/avm.h>
```

A minimal link-native execution looks like this:

```cpp
#include <avm/avm.h>

int main()
{
    avm::InMemoryLinkStore store;
    avm::BootstrapRuntime runtime(store);
    avm::ProgramBuilder builder = runtime.builder();

    const avm::LinkId expression =
        builder.logical_not(builder.literal(runtime.vocabulary().false_value));
    const avm::LinkId result = runtime.execute(expression);

    return result == runtime.vocabulary().true_value ? 0 : 1;
}
```

The umbrella header intentionally does not include JSON adapters. JSON remains an optional projection layer and is not part of the dependency-free `avm::core` execution contract.

## Install and consume with CMake

Install AVM into a prefix:

```bash
cmake -S . -B build-install \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --install build-install
```

A consuming CMake project can require the AVM 1.x package:

```cmake
find_package(avm 1.0 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE avm::core)
```

`avm::core` is a header-only C++20 target. Its installed interface points only at the install prefix; it does not depend on repository-relative source or `3p` include paths.

## Version and compatibility policy

AVM uses Semantic Versioning for the documented public core contracts. The project version in CMake and `include/avm/version.h` must match exactly; CI also rejects tagged builds unless the tag is exactly `v<project-version>`.

Compatibility within AVM 1.x applies to the documented public contracts exposed through `<avm/avm.h>` and `avm::core`. Header-only implementation details, private layout and incidental helpers are not an ABI promise. See [AVM 1.x release policy](docs/release-policy.md).

## Backends

`InMemoryLinkStore` is the reference backend. `PersistentLinkStore` proves reopen and identity semantics. Additional physical backends are replaceable implementations of the same `LinkStore` contract. LinksPlatform/PMM integrations are follow-up backend work, not prerequisites for VM semantics.

Backend code must not define VM relations, JSON rules, protocol grammar or execution semantics.

## Reproducible repository validation

Build and run the test suite:

```bash
git clone https://github.com/netkeep80/avm.git
cd avm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The link-native vertical-slice tests exercise the intended flow: bootstrap a canonical store, encode program entities as Relations Model links, execute them by `LinkId`, and verify the resulting canonical identities. JSON/CLI tests exercise projection compatibility separately. CI also installs the public package into a staging prefix and builds an external consumer using only `find_package(avm 1.0 CONFIG REQUIRED)` and `avm::core`.

## Architecture documents

- [AVM 1.0 architecture contract](docs/architecture.md)
- [Execution kernel](docs/execution-kernel.md)
- [External protocol adapter contract](docs/protocol-adapter-contract.md)
- [Persistent LinkStore contract](docs/persistent-link-store.md)
- [AVM 1.x release policy](docs/release-policy.md)
- [Project analysis](analysis.md)
- [AVM 1.0 roadmap](plan.md)

## Project status

Architecture Foundation gates 1–7 are complete. The public/install contract and AVM 1.0.0 version policy are defined. The remaining release-readiness gate is portable installed-package validation on Linux, Windows and macOS before post-1.0 feature work takes priority.

## Dependencies

Core library:

- C++20 compiler
- CMake 3.20+ for the provided package/build system

Optional JSON boundary/CLI:

- bundled `nlohmann/json`

## License

MIT License. Copyright © 2022 Vertushkin Roman Pavlovich.
