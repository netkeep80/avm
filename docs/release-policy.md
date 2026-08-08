# Политика релизов AVM 1.x

## Источник версии

AVM использует Semantic Versioning для документированного публичного library surface.

Release version записывается в двух местах, которые CI требует держать синхронными:

- `CMakeLists.txt` — `CMAKE_PROJECT_VERSION`;
- `include/avm/version.h` — `version_major`, `version_minor`, `version_patch`, `version_string`.

Для tagged build Git tag обязан быть точно `v<project-version>`. Несовпадение tag/version является release-blocking error.

## Публичная поверхность совместимости

Поддерживаемая точка входа C++ core:

```cpp
#include <avm/avm.h>
```

Поддерживаемый CMake target:

```cmake
find_package(avm 1.0 CONFIG REQUIRED)
target_link_libraries(app PRIVATE avm::core)
```

В линии 1.x compatibility promise относится к документированным контрактам, экспортируемым umbrella header и проверяемым installed-package consumer:

- opaque `LinkId` и `Link` semantics;
- observable contract `LinkStore`;
- Relations Model encode/decode;
- parser-independent projection boundary;
- execution path `BootstrapRuntime`/`Executor` по `LinkId`;
- link-native construction через `ProgramBuilder`;
- observable semantics reference in-memory/persistent backends;
- `avm::core` CMake package target;
- публичные observer/query contracts, если они экспортируются через поддерживаемый core API.

Преднамеренное несовместимое изменение этих контрактов требует следующей major version.

## Что не является ABI promise

Core сейчас header-only. AVM 1.x не обещает binary ABI stability для incidental implementation details, class layout, private members, undocumented helpers, benchmark values или repository-internal test/build structure.

Clients должны компилироваться против installed headers потребляемой версии. Пока отдельная ABI policy не объявлена, SemVer compatibility относится к source-level и документированному observable behavior.

## JSON и protocol adapters

JSON program/value support — adapter layer, а не semantic core VM. Дополнительные adapters, включая Anum/МТС, обязаны проецироваться в существующую canonical boundary `LinkStore`/Relations Model и не могут вводить второй executor или storage identity universe.

Adapter API получает отдельное compatibility commitment только после явного продвижения в installed public package.

## Release checklist

Release `vX.Y.Z` допустим только если:

1. CMake и public version header равны `X.Y.Z`;
2. tag точно равен `vX.Y.Z`;
3. quality/architecture guards зелёные;
4. strict core, JSON/session и CLI lanes зелёные;
5. ASan/UBSan зелёный;
6. portable Linux/Windows/macOS matrix зелёная;
7. installed-package consumer validation зелёная на поддерживаемых OS;
8. benchmark smoke успешно завершён и выдаёт валидный artifact;
9. дополнительные focused conformance gates, затронутые release, зелёные;
10. release artifacts создаются только после всех объявленных dependencies.

`Tagged Linux artifact` намеренно зависит от полной `portable` matrix, хотя сам публикуемый artifact Linux-only. Публикация не должна становиться доступной, пока Windows/macOS portable validation ещё выполняется или падает.

Failed gate — release veto, а не warning.

## Что пока не входит в release contract

Отдельными задачами остаются:

- GitHub Release publication;
- cryptographic signing;
- multi-platform binary bundles;
- long-term ABI policy;
- production backend durability certification.
