<p align="center"><img src="EOSR.jpg"></p>

# AVM — ассоциативная виртуальная машина

AVM — экспериментальная виртуальная машина на C++20, в которой данные, программы и execution identities представлены через каноническую сеть направленных связей.

**Публичная версия пакета: 1.3.0.** В `main` также завершены архитектурные development gates AVM 1.4–1.5, включая inspection tooling, evidence-backed перенос выбранной семантики `jsonRVM` и первый explicit capability boundary для host effects. Это не означает полного `jsonRVM` parity и не меняет опубликованную SemVer-версию автоматически.

## В двух словах

Физический примитив AVM:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

`LinkId` — непрозрачная identity. Для одной точной пары в пределах одного logical store существует одна canonical link identity.

Модель Отношений кодируется без отдельного физического типа «триплет»:

```text
(relation, subject, object)
= Link(relation, Link(subject, object))
```

Основной runtime path один:

```text
внешнее представление
 -> projection / semantic adapter
 -> canonical denotation
 -> find | explicit realize
 -> LinkStore
 -> программа как LinkId
 -> BootstrapRuntime / Executor
 -> LinkId / ExecutionOutcome
```

JSON и Anum являются frontends/projections. Они не являются внутренним AST виртуальной машины.

## Главные архитектурные инварианты

AVM намеренно держит несколько границ жёсткими:

```text
find / resolve / query / read
!=
realize / intern / write
!=
host effect
```

Из этого следуют практические правила:

- чтение не материализует отсутствующие связи;
- program structure сама по себе не выдаёт host authority;
- execution dispatch идёт по canonical relation `LinkId`;
- нет второго compatibility Executor или JSON semantic interpreter;
- observer и inspection tooling не управляют execution;
- frontend syntax после canonical realization не является runtime dependency;
- backend не определяет VM semantics;
- numeric `LinkId` нельзя считать универсальным cross-store значением.

Подробно: [архитектурный контракт](docs/architecture.md).

## Что уже доказано

### AVM 1.0–1.4

Завершены:

- canonical `LinkStore` и Relations Model codec;
- один link-native `Executor`;
- programs, functions, bindings и call frames как links;
- structural Anum L3→L4 boundary;
- JSON projection adapters без JSON AST внутри executor;
- `PersistentLinkStore` с reopen/identity proof;
- read-only Relations Model queries;
- structural standard library;
- deterministic execution observer и bounded trace;
- typed inspection session и scripted `avm-inspect` tooling;
- portable/package-consumer CI на Linux, Windows и macOS.

### AVM 1.5 — evidence-backed semantic migration

AVM 1.5 не пытается механически перенести весь старый `base.rm.h`. Вместо этого semantics переносится только через frozen evidence.

Pinned historical oracle:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Доказанный pure corpus:

```text
CASE-ARITHMETIC                  -> 2
CASE-SEQUENCE-ORDER              -> 3
CASE-PURE-RELATION-COMPOSITION   -> 5
CASE-FOREACH-CONTEXT             -> [1,2,3]
CASE-BOOLEAN-BRANCH              -> 42
CASE-MISSING-REFERENCE           -> typed source failure
```

Persistent release proof показывает более сильное свойство: canonical root materialize-ится один раз, store закрывается и открывается снова, после чего существующий root исполняется без legacy JSON, remigration, reprojection или повторного realize.

Подробно: [доказательства AVM 1.5](docs/avm-1.5-release-proof.md) и [карта совместимости jsonRVM](docs/jsonrvm-compatibility.md).

### Explicit host-effect capability boundary

Первый host-effect contract завершён отдельно от pure AVM 1.5 proof. Evidence source — `REF-LAZY-DB-001`, historical lazy external entity retrieval.

Нормативный путь:

```text
canonical effect RelationEntity
 -> ordinary Executor
 -> explicit capability policy
 -> explicit ExternalEntityProvider
 -> existing LinkId | deterministic failure
```

Provider не имеет права автоматически materialize-ить arbitrary graph. Он может вернуть только identity, уже существующую в текущем store; foreign identity отклоняется. Pure programs не требуют provider вообще.

Это доказывает архитектурную границу effects, но **не** заявляет готовность реальных filesystem/HTTP/clock/native adapters.

Подробно: [capability boundary](docs/effect-capabilities.md).

## Минимальный C++ пример

Публичный JSON-independent API доступен через umbrella-header:

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

`<avm/avm.h>` намеренно не превращает JSON adapters в зависимость `avm::core`.

## Установка через CMake

```bash
cmake -S . -B build-install \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --install build-install
```

В consuming project:

```cmake
find_package(avm 1.3 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE avm::core)
```

`avm::core` — header-only C++20 target. Installed package проверяется отдельными consumers на Ubuntu, Windows и macOS.

## CLI и inspection tooling

Обычный JSON frontend запускается через тот же canonical executor:

```text
avm program.json
avm --trace program.json
avm --trace-limit 64 program.json
```

Для read-only/diagnostic сценариев есть `avm-inspect`. Его textual commands являются presentation layer: после parsing используются типизированные inspection APIs, а не второй runtime.

См. [inspection session](docs/inspection-session.md), [inspection commands](docs/inspection-commands.md) и [inspection runner](docs/inspection-runner.md).

## Документация

Начинать лучше с [карты документации](docs/README.md). В ней документы разделены по статусу и назначению: архитектурные контракты, semantic contracts, persistence/release proofs, frontend adapters, jsonRVM evidence и tooling.

Рекомендуемый маршрут:

```text
README
 -> docs/architecture.md
 -> docs/execution-kernel.md
 -> docs/projection-boundary.md
 -> docs/triune-execution-contract.md
 -> docs/semantic-context-contract.md
 -> docs/avm-1.5-release-proof.md
 -> docs/effect-capabilities.md
```

Текущий dependency/status overview находится в [plan.md](plan.md).

## Связь с Anum/МТС

AVM не дублирует грамматику и полную теорию Anum/МТС внутри storage/runtime layer. Канонический источник этих правил — `netkeep80/anum_docs`.

Для frontend boundary сохраняется принцип:

```text
raw(A) != den(A)
find(A) не создаёт den(A)
realize(A) выполняется явно
```

AVM принимает структурный результат проекции и работает дальше только с canonical links.

## Версионирование

AVM использует Semantic Versioning для документированных публичных контрактов ядра. Версии в CMake и `include/avm/version.h` должны совпадать; tagged release дополнительно проверяется CI.

Важно различать:

- **public package version** — опубликованный SemVer contract;
- **development gate** — завершённая архитектурная работа в `main`;
- **historical evidence** — зафиксированное поведение legacy runtime, которое не становится AVM contract автоматически.

См. [release policy](docs/release-policy.md).

## Разработка и проверки

Типовая проверка:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CI дополнительно проверяет warnings-as-errors, ASan/UBSan, portable builds, installed-package consumers, documentation language, architecture guards, benchmarks и showcase там, где workflow применим.

Подробно: [CI contract](docs/ci.md).

## Лицензия

Проект распространяется на условиях лицензии из [LICENSE](LICENSE).
