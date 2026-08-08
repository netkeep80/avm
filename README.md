<p align="center"><img src="EOSR.jpg"></p>

# AVM — Associative Virtual Machine

AVM is an experimental C++20 virtual machine built on the Relations Model and a canonical store of directed links.

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

- `LinkStore` as the storage contract, with an in-memory reference implementation;
- Relations Model encoding `(relation, subject, object) = (relation, (subject, object))`;
- explicit bootstrap vocabulary identity;
- link-native execution through `BootstrapRuntime` and `Executor`;
- associative program structures, bindings and call frames represented by links;
- a neutral protocol-adapter boundary that keeps parsing/context semantics outside the VM core;
- separate JSON value and JSON program/session adapters at the projection boundary;
- persistent-store contracts and conformance tests independent from VM semantics;
- CI on Linux, Windows and macOS, warnings-as-errors lanes, sanitizers, quality guards and benchmark regression gates.

The former pointer-based `rel_t` universe and its legacy execution/data-codec path have been removed. Git history preserves that implementation; AVM does not keep a second semantic path for compatibility.

## Backends

`InMemoryLinkStore` is the reference backend. Persistent backends are replaceable implementations of the same `LinkStore` contract. LinksPlatform/PMM integrations are follow-up backend work, not prerequisites for VM semantics.

Backend code must not define VM relations, JSON rules, protocol grammar or execution semantics.

## Reproducible vertical slice

Build and run the test suite:

```bash
git clone https://github.com/netkeep80/avm.git
cd avm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The link-native vertical-slice tests exercise the intended flow: bootstrap a canonical store, encode program entities as Relations Model links, execute them by `LinkId`, and verify the resulting canonical identities. JSON/CLI tests exercise projection compatibility separately.

## Architecture documents

- [AVM 1.0 architecture contract](docs/architecture.md)
- [Execution kernel](docs/execution-kernel.md)
- [External protocol adapter contract](docs/protocol-adapter-contract.md)
- [Persistent LinkStore contract](docs/persistent-link-store.md)
- [Project analysis](analysis.md)
- [AVM 1.0 roadmap](plan.md)

## Project status

The active roadmap is **Architecture Foundation 2.0 / AVM 1.0**. Feature expansion such as GUI, a broad standard library or additional frontends follows completion of foundation gates and must reuse the single link-native semantic path.

## Dependencies

- C++20 compiler
- CMake 3.20+
- `nlohmann/json` for the JSON boundary

## License

MIT License. Copyright © 2022 Vertushkin Roman Pavlovich.
