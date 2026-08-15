# Документация AVM

Этот каталог содержит инженерные контракты, доказательства совместимости и документацию tooling для AVM. Он организован не по хронологии появления файлов, а по тому, **какой вопрос читатель пытается решить**.

Главный принцип навигации:

```text
README репозитория
 -> architecture.md
 -> нужный контракт подсистемы
 -> evidence / release proof
 -> implementation и tests
```

## Что считать нормативным

Документы AVM имеют разный статус. Это важно: historical audit не должен случайно становиться новым semantic contract.

### 1. Архитектурные контракты

Описывают принятые границы системы и инварианты, которые новый код не должен нарушать:

- [architecture.md](architecture.md) — общий архитектурный контракт;
- [execution-kernel.md](execution-kernel.md) — link-native execution kernel;
- [program-model.md](program-model.md) — программы как связи;
- [functions-and-frames.md](functions-and-frames.md) — functions, bindings и call frames;
- [projection-boundary.md](projection-boundary.md) — граница внешнего представления и canonical denotation;
- [protocol-adapter-contract.md](protocol-adapter-contract.md) — внешний protocol/parser boundary;
- [raw-carrier.md](raw-carrier.md) — транспортный carrier без скрытой semantics;
- [relations-query.md](relations-query.md) — read-only Relations Model queries;
- [structural-standard-library.md](structural-standard-library.md) — link-native structural primitives;
- [execution-observability.md](execution-observability.md) — observer/trace contract;
- [effect-capabilities.md](effect-capabilities.md) — explicit authority boundary для host effects.

При противоречии между старым planning/evidence текстом и действующим архитектурным контрактом приоритет имеет **текущий код + тесты + эти архитектурные документы**.

### 2. Семантические контракты AVM 1.5

Документы, фиксирующие доказанный Relations Model subset и его canonical denotation:

- [triune-execution-contract.md](triune-execution-contract.md) — meaningful `(relation, subject, object)` execution;
- [semantic-context-contract.md](semantic-context-contract.md) — current/parent context;
- [reference-algebra.md](reference-algebra.md) — canonical reference algebra;
- [semantic-execution-primitives.md](semantic-execution-primitives.md) — explicit semantic state transitions;
- [execution-projection.md](execution-projection.md) — ordered projection/sequence semantics;
- [foreach-runtime.md](foreach-runtime.md) — deterministic foreach contexts;
- [boolean-runtime.md](boolean-runtime.md) — Boolean control semantics;
- [value-denotation-v1.md](value-denotation-v1.md) — общий value-denotation contract;
- [integer-denotation-contract.md](integer-denotation-contract.md) — Integer;
- [text-denotation.md](text-denotation.md) — Text;
- [deferred-definitions.md](deferred-definitions.md) — deferred-definition boundary.

### 3. Persistence и reopen

- [persistent-link-store.md](persistent-link-store.md) — reference persistent backend;
- [persistent-inspection-session.md](persistent-inspection-session.md) — inspection после reopen;
- [avm-1.0-vertical-slice.md](avm-1.0-vertical-slice.md) — ранний end-to-end foundation proof;
- [avm-1.5-release-proof.md](avm-1.5-release-proof.md) — финальное доказательство pure AVM 1.5 subset после canonical realization и reopen.

### 4. Frontend и JSON/Anum projection

JSON и Anum находятся **до** canonical runtime и не являются внутренним AST AVM.

- [anum-l3-l4-bridge.md](anum-l3-l4-bridge.md) — структурный мост Anum L3→L4;
- [duplet-json-format.md](duplet-json-format.md) — внешний duplet-json формат;
- [duplet-json-leaf-resolution.md](duplet-json-leaf-resolution.md) — leaf resolution;
- [json-value-codec.md](json-value-codec.md) — JSON value adapter;
- [json-compatibility-runtime.md](json-compatibility-runtime.md) — compatibility frontend/runtime boundary;
- [legacy-reference-compiler.md](legacy-reference-compiler.md) — компиляция legacy textual references;
- [legacy-interpreter-removal.md](legacy-interpreter-removal.md) — доказательство удаления второго semantic path.

## jsonRVM migration evidence

Эти документы нужны для ответа на вопрос **«почему именно такая semantics была перенесена?»**, а не для восстановления старого runtime как второго production path.

- [jsonrvm-compatibility.md](jsonrvm-compatibility.md) — карта доказанной/отложенной совместимости;
- [jsonrvm-legacy-oracle.md](jsonrvm-legacy-oracle.md) — pinned historical oracle;
- [jsonrvm-semantic-migrator.md](jsonrvm-semantic-migrator.md) — evidence-backed semantic migrator;
- `../compat/jsonrvm-semantics.json` — машиночитаемый semantic inventory;
- `../compat/jsonrvm-semantics-details.json` — подробный inventory;
- `../compat/jsonrvm-golden.json` и `../compat/jsonrvm-oracle-golden.json` — frozen evidence.

Pinned oracle для AVM 1.5:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Historical evidence не расширяет контракт автоматически. Если fixture доказывает один exact case, AVM обещает именно доказанный case или явно сформулированное обобщение, поддержанное отдельным contract/test.

## Inspection и debugging tooling

Tooling использует существующие публичные AVM API и **не создаёт второй executor**:

- [inspection-session.md](inspection-session.md) — типизированная inspection session;
- [inspection-commands.md](inspection-commands.md) — textual commands как presentation layer;
- [inspection-runner.md](inspection-runner.md) — scripted `avm-inspect` runner;
- [showcase-walkthrough.md](showcase-walkthrough.md) — walkthrough showcase;
- [showcase-calculator-state.md](showcase-calculator-state.md) — calculator state model.

## Engineering и release

- [ci.md](ci.md) — CI matrix и архитектурные vetoes;
- [performance-baseline.md](performance-baseline.md) — benchmark baseline;
- [release-policy.md](release-policy.md) — versioning/release policy;
- `../plan.md` — текущее состояние завершённых gates и направления развития.

## Рекомендуемый путь чтения

Для нового разработчика:

1. корневой `README.md` — назначение, quick start и текущий status;
2. [architecture.md](architecture.md) — физический и semantic foundation;
3. [execution-kernel.md](execution-kernel.md) и [program-model.md](program-model.md);
4. [projection-boundary.md](projection-boundary.md) — почему JSON/Anum не являются runtime AST;
5. [triune-execution-contract.md](triune-execution-contract.md) и [semantic-context-contract.md](semantic-context-contract.md);
6. [avm-1.5-release-proof.md](avm-1.5-release-proof.md) — что именно реально доказано end-to-end;
7. [effect-capabilities.md](effect-capabilities.md) — как AVM допускает host effects без скрытой authority.

Для работы с jsonRVM migration сначала читать [jsonrvm-compatibility.md](jsonrvm-compatibility.md), затем frozen manifests в `compat/`, а уже после этого — implementation/tests.

## Инварианты документации

Документация проекта должна соблюдать те же дисциплины, что и код:

- не выдавать будущий план за реализованный contract;
- не выдавать historical behavior за обязательную AVM semantics;
- явно различать `find/resolve/read` и `realize/write/effect`;
- явно различать public release version и завершённые development gates;
- не описывать удалённые compatibility paths как доступные runtime options;
- ссылаться на frozen evidence, когда утверждение основано на legacy behavior;
- после изменения архитектурного gate синхронизировать README, `plan.md`, соответствующий contract/release proof и этот индекс.
