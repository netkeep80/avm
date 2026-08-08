<p align="center"><img src="EOSR.jpg"></p>

# AVM — Associative Virtual Machine

[English](#english) | [Русский](#russian)

---

<a name="english"></a>
## English

AVM is an experimental associative virtual machine whose runtime state, program structures and Relations Model entities are represented by canonical links.

The AVM 1.0 foundation is built around one physical primitive:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

A Relations Model entity `(relation, subject, object)` is encoded without a separate physical triplet type:

```text
entity = Link(relation, Link(subject, object))
```

### Current architecture

```text
external projection (JSON; later Anum adapter)
        |
        v
projection / importer
        |
        v
LinkStore: canonical L -> L^2
        |
        v
Relations Model entities + program links
        |
        v
BootstrapRuntime / Executor
        |
        v
result LinkId
```

The executor receives link identities, not a JSON AST. JSON is an external projection and compatibility surface, not the VM's internal instruction representation.

### Foundation implemented

- canonical `LinkStore` with exact, outgoing and incoming indexes;
- non-mutating `find` and explicit materialization through `intern` / projection realization;
- Relations Model triplet-to-dyads codec;
- relation dispatch by `LinkId`;
- NOT / AND / OR represented by associative truth-table entities;
- lazy `If`;
- functions, formal parameters, calls, bindings and call frames represented in links;
- recursive `Def` / `Call` execution with call-depth protection;
- JSON importer that materializes executable structures into links;
- separate raw carrier and neutral `ProjectionDescription` boundary for future Anum/MTS integration;
- `InMemoryLinkStore` reference backend;
- versioned `PersistentLinkStore` reference backend with stable `LinkId` values across reopen and deterministic index rebuild;
- C++20 warnings-as-errors, ASan/UBSan, and portable Linux/Windows/macOS CI.

The historical recursive JSON interpreter has been removed. The remaining historical `rel_t` JSON data-codec/storage compatibility layer is scheduled for removal after its consumers are migrated to the canonical `LinkStore` path (issue #68).

### Persistence

`PersistentLinkStore` is a conformance/reference backend. It proves stable identity, canonical pair reuse and reopen semantics. It is intentionally not presented as a production durability implementation: WAL, crash-atomic commits, concurrent writers and PMM/LinksPlatform adapters are separate concerns.

### Anum / MTS boundary

AVM does not contain a second Anum parser. The intended boundary is:

```text
raw source (optional)
    -> external parse / validate / project(context)
    -> ProjectionDescription
    -> find_projection(...) | realize_projection(...)
    -> LinkStore
```

`find` remains observational; `realize` is the explicit write operation. Grammar, abits, quotation rules and protocol context belong to the external Anum/MTS implementation.

### Build and test

```bash
git clone https://github.com/netkeep80/avm.git
cd avm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For the strict core-only lane:

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

### Documentation

- [`docs/architecture.md`](docs/architecture.md) — normative AVM 1.0 layering and invariants;
- [`docs/execution-kernel.md`](docs/execution-kernel.md) — link-native execution kernel;
- [`docs/jsonrvm-compatibility.md`](docs/jsonrvm-compatibility.md) — Relations Model compatibility;
- [`docs/protocol-adapter-contract.md`](docs/protocol-adapter-contract.md) — AVM ↔ external Anum/MTS boundary;
- [`docs/persistent-link-store.md`](docs/persistent-link-store.md) — reference persistent format and reopen contract;
- [`docs/ci.md`](docs/ci.md) — CI/CD policy and test lanes;
- [`analysis.md`](analysis.md) — current architectural assessment;
- [`plan.md`](plan.md) — current roadmap.

`docs/avm-1.0-vertical-slice.md` and `docs/performance-baseline.md` are added by the corresponding final AVM 1.0 integration gates.

---

<a name="russian"></a>
## Русский

AVM — экспериментальная ассоциативная виртуальная машина, в которой состояние runtime, структуры программы и сущности Модели Отношений представлены каноническими связями.

Фундамент AVM 1.0 строится вокруг одного физического примитива:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

Сущность Модели Отношений `(отношение, субъект, объект)` кодируется без отдельного физического типа triplet:

```text
entity = Link(отношение, Link(субъект, объект))
```

### Текущая архитектура

```text
внешняя проекция (JSON; позже Anum adapter)
        |
        v
projection / importer
        |
        v
LinkStore: каноническое L -> L^2
        |
        v
Relations Model entities + программа в связях
        |
        v
BootstrapRuntime / Executor
        |
        v
result LinkId
```

Executor получает идентификаторы связей, а не JSON AST. JSON является внешней проекцией и compatibility surface, но не внутренней системой команд VM.

### Реализованный фундамент

- канонический `LinkStore` с exact/outgoing/incoming индексами;
- немутирующий `find` и явная материализация через `intern` / realization проекции;
- кодек сущности Relations Model из triplet в два вложенных dyad;
- dispatch исполнения по relation `LinkId`;
- NOT / AND / OR как ассоциативные таблицы истинности;
- ленивый `If`;
- функции, формальные параметры, вызовы, bindings и call frames в самих связях;
- рекурсивный `Def` / `Call` с ограничением глубины;
- JSON importer, материализующий исполняемую структуру в `LinkStore`;
- отдельный raw carrier и нейтральный `ProjectionDescription` для будущей интеграции Anum/MTS;
- эталонный `InMemoryLinkStore`;
- versioned `PersistentLinkStore` со стабильными `LinkId` после reopen и восстановлением индексов;
- CI с C++20 warnings-as-errors, ASan/UBSan и Linux/Windows/macOS.

Старый рекурсивный JSON-интерпретатор уже удалён. Исторический `rel_t` сейчас остаётся только как отдельный JSON data-codec/storage compatibility layer; после миграции его consumers он должен быть удалён, чтобы в production остался один canonical `LinkStore` identity universe (issue #68).

### Персистентность

`PersistentLinkStore` — reference/conformance backend. Он доказывает стабильность identity, каноническое переиспользование пары и reopen semantics. Это не заявление о production durability: WAL, crash-atomic commits, concurrent writers и адаптеры PMM/LinksPlatform являются отдельными задачами.

### Граница с Anum / МТС

В AVM не создаётся второй parser ачисел. Граница выглядит так:

```text
raw source (опционально)
    -> внешний parse / validate / project(context)
    -> ProjectionDescription
    -> find_projection(...) | realize_projection(...)
    -> LinkStore
```

`find` только наблюдает и не создаёт связи; `realize` является явной операцией записи. Грамматика, абиты, quotation и protocol context принадлежат внешней реализации Anum/MTS.

### Сборка и тесты

```bash
git clone https://github.com/netkeep80/avm.git
cd avm
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Строгая core-only проверка:

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

### Документация

- [`docs/architecture.md`](docs/architecture.md) — нормативные слои и инварианты AVM 1.0;
- [`docs/execution-kernel.md`](docs/execution-kernel.md) — link-native execution kernel;
- [`docs/jsonrvm-compatibility.md`](docs/jsonrvm-compatibility.md) — совместимость с Relations Model;
- [`docs/protocol-adapter-contract.md`](docs/protocol-adapter-contract.md) — граница AVM ↔ внешний Anum/MTS protocol;
- [`docs/persistent-link-store.md`](docs/persistent-link-store.md) — reference persistence format и reopen contract;
- [`docs/ci.md`](docs/ci.md) — CI/CD и test lanes;
- [`analysis.md`](analysis.md) — актуальный архитектурный анализ;
- [`plan.md`](plan.md) — актуальный roadmap.

`docs/avm-1.0-vertical-slice.md` и `docs/performance-baseline.md` добавляются соответствующими финальными integration gates AVM 1.0.

---

## License / Лицензия

MIT License. See [`LICENSE`](LICENSE).

## Contact / Контакты

- GitHub: [netkeep80/avm](https://github.com/netkeep80/avm)
