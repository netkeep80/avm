<p align="center"><img src="EOSR.jpg"></p>

# AVM — Associative Virtual Machine

AVM is an experimental C++20 virtual machine built on the Relations Model and a canonical store of directed links.

**Current public version: 1.1.0.**

## Semantic path

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

JSON is an external projection and compatibility protocol. Anum/MTS grammar and contextual denotation remain canonical in `netkeep80/anum_docs`; AVM consumes only the storage-neutral structural denotation handoff. Neither format is the VM AST or execution core.

The physical primitive is a canonical directed dyad:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

`LinkId` is opaque. Reads do not materialize missing links, and `intern(a,b)` returns the canonical identity for an exact pair inside one logical store.

## AVM 1.0 foundation — complete

AVM 1.0 established and validated:

- `LinkStore` as the storage contract, with in-memory and persistent reference implementations;
- canonical Relations Model encoding `(relation, subject, object) = (relation, (subject, object))`;
- explicit bootstrap vocabulary identity;
- link-native execution through `BootstrapRuntime` and `Executor`;
- associative program structures, bindings and call frames represented by links;
- parser-independent protocol projection boundary;
- canonical structural Anum L3→L4 bridge with no grammar duplication in AVM;
- separate JSON value and JSON program/session adapters;
- persistent reopen/identity conformance independent from VM semantics;
- installed-package consumer validation on Linux, Windows and macOS;
- warnings-as-errors, sanitizers, architecture quality guards and benchmark baselines.

The former pointer-based `rel_t` universe, JSON semantic interpreter and temporary protocol-value-only Anum bridge have been removed. Git history preserves them; AVM keeps one semantic path.

## AVM 1.1 — read-only associative queries

AVM 1.1 begins with a dependency-free Relations Model query layer:

```cpp
avm::RelationQuery query{
    .relation = relation,
    .subject = std::nullopt,
    .object = object,
};

const auto matches = avm::query_relation_entities(store, query);
```

At least one relation/subject/object field must be constrained. Queries use only the existing `LinkStore` `find/outgoing/incoming/get/contains` contract; they never call `intern` or `create_point`, and they do not introduce a second index universe.

Relations Model querying is structural rather than registry-based. AVM does not track a hidden list of links "created as entities"; domain restrictions must themselves be represented through links/relations.

See [Relations Model query contract](docs/relations-query.md).

## Supported core library API

The dependency-free C++ core surface is available through:

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

The umbrella header intentionally does not include JSON adapters. JSON remains optional and is not part of the dependency-free `avm::core` execution contract.

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

A consuming CMake project can require the current AVM 1.1 package:

```cmake
find_package(avm 1.1 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE avm::core)
```

`avm::core` is a header-only C++20 target. Its installed interface points only at the install prefix and does not depend on repository-relative source or `3p` include paths.

## Version and compatibility policy

AVM uses Semantic Versioning for documented public core contracts. CMake and `include/avm/version.h` must match exactly; CI also rejects tagged builds unless the tag is exactly `v<project-version>`.

Compatibility within AVM 1.x applies to documented public contracts exposed through `<avm/avm.h>` and `avm::core`. Header-only implementation details, private layout and incidental helpers are not an ABI promise. See [AVM 1.x release policy](docs/release-policy.md).

## Backends

`InMemoryLinkStore` is the reference backend. `PersistentLinkStore` proves reopen and identity semantics. Additional physical backends are replaceable implementations of the same `LinkStore` contract.

Backend code must not define VM relations, query semantics, JSON rules, protocol grammar or execution semantics.

## Reproducible repository validation

```bash
git clone https://github.com/netkeep80/avm.git
cd avm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CI installs the package into a staging prefix and builds an external consumer on Linux, Windows and macOS using only `find_package` and `avm::core`.

## Architecture documents

- [AVM architecture contract](docs/architecture.md)
- [Execution kernel](docs/execution-kernel.md)
- [External protocol adapter contract](docs/protocol-adapter-contract.md)
- [Anum L3→L4 bridge](docs/anum-l3-l4-bridge.md)
- [Relations Model query contract](docs/relations-query.md)
- [Persistent LinkStore contract](docs/persistent-link-store.md)
- [AVM 1.x release policy](docs/release-policy.md)
- [Project analysis](analysis.md)
- [Roadmap](plan.md)

## Project status

Architecture Foundation / AVM 1.0 gates are complete. AVM 1.1 now develops read-only associative query facilities while preserving the same canonical `LinkStore -> Relations Model -> Executor` foundation. New physical indexes are considered only after the existing-index query baselines demonstrate a measured need.

## Dependencies

Core library:

- C++20 compiler
- CMake 3.20+

Optional JSON boundary/CLI:

- bundled `nlohmann/json`

## License

MIT License. Copyright © 2022 Vertushkin Roman Pavlovich.
