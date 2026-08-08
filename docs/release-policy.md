# AVM 1.x release policy

## Version source contract

AVM follows Semantic Versioning for the documented public library surface.

The release version is recorded in two places that CI requires to match exactly:

- `CMakeLists.txt` — `CMAKE_PROJECT_VERSION`;
- `include/avm/version.h` — `version_major`, `version_minor`, `version_patch` and `version_string`.

For a tagged build, the Git tag must be exactly `v<project-version>`. A tag/version mismatch is a release-blocking CI error.

## Public compatibility surface

The supported C++ core entry point is:

```cpp
#include <avm/avm.h>
```

The supported CMake target is:

```cmake
find_package(avm 1.0 CONFIG REQUIRED)
target_link_libraries(app PRIVATE avm::core)
```

Within the 1.x line, compatibility promises apply to the documented contracts exposed by that umbrella header and used by the installed-package consumer test:

- opaque `LinkId` and `Link` semantics;
- `LinkStore` observable contract;
- Relations Model encode/decode contract;
- parser-independent projection boundary;
- `BootstrapRuntime`/`Executor` LinkId execution path;
- `ProgramBuilder` link-native construction contract;
- reference in-memory and persistent backend observable semantics;
- `avm::core` CMake package target.

A change that intentionally breaks those supported contracts requires the next major version.

## What is not an ABI promise

AVM core is currently header-only. Version 1.x does not promise binary ABI stability for incidental implementation details, class layout, private members, helper functions that are not documented as public contracts, benchmark values, or repository-internal test/build structure.

Clients should compile against the installed headers for the version they consume. SemVer compatibility concerns source-level/documented behavior unless a future release explicitly establishes an ABI policy.

## JSON and protocol adapters

JSON program/value support is an adapter layer, not the VM semantic core. Additional protocol adapters, including Anum/MTS work, must project into the existing canonical LinkStore/Relations Model boundary and may not introduce a second executor or storage identity universe.

Adapter APIs can receive their own compatibility commitments when they are promoted into the installed public package. They are not implicitly part of `avm::core` merely because headers exist in the repository.

## Release checklist

A `vX.Y.Z` release is valid only when:

1. CMake and public header versions both equal `X.Y.Z`;
2. the tag is exactly `vX.Y.Z`;
3. quality architecture guards are green;
4. strict core, JSON/session and CLI lanes are green;
5. ASan/UBSan is green;
6. portable Linux/Windows/macOS tests are green;
7. installed-package consumer validation is green;
8. benchmark smoke completes and emits its validated artifact;
9. release artifacts are produced only after their declared dependencies pass.

A failed gate is a release veto, not a warning.
