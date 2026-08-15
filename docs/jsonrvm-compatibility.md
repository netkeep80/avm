# Совместимость jsonRVM и AVM

Родительский audit: #123. Epic AVM 1.5: #122.

Этот документ отвечает на два разных вопроса:

1. **что исторический `jsonRVM` реально делал**;
2. **какая часть этого поведения доказана или намеренно не перенесена в AVM**.

Эти вопросы нельзя смешивать. Historical behavior сам по себе не является текущим AVM contract.

## Источник evidence

Аудит привязан к фиксированной версии legacy runtime:

```text
repository: netkeep80/jsonRVM
commit:     843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime:    jsonRVM 3.0.0
```

Машиночитаемые источники:

- `compat/jsonrvm-semantics.json` — semantic inventory;
- `compat/jsonrvm-semantics-details.json` — подробный inventory;
- `compat/jsonrvm-golden.json` — frozen assertions;
- `compat/jsonrvm-oracle-golden.json` — исполняемый oracle evidence.

Если prose и machine-readable frozen evidence расходятся, сначала проверяется manifest/oracle и соответствующие tests.

## Центральный архитектурный вывод

Проблемой legacy runtime было не представление триплета через дуплеты. AVM использует каноническое encoding:

```text
(relation, subject, object)
= Link(relation, Link(subject, object))
```

Сложность `jsonRVM` состояла в другом: `nlohmann::json` одновременно играл роли syntax, runtime values, mutable execution context и database/view layer.

AVM переносит **observable semantics**, а не эту архитектурную связанность.

Нормативная boundary:

```text
legacy/source syntax
 -> frontend / semantic adapter
 -> canonical denotation
 -> find | explicit realize
 -> LinkStore
 -> ordinary Executor
```

Нет JSON semantic interpreter после projection и нет второго compatibility runtime.

## Категории legacy behavior

### `canonical-semantic`

Поведение относится к смыслу Relations Model/VM и должно быть выражено независимо от frontend syntax: triune roles, contexts, ordered sequence, Boolean control, pure arithmetic и т. п.

### `projection-syntax`

Полезная surface syntax, которую frontend компилирует в canonical structure до execution: context pronouns, textual references, legacy addressing forms.

### `effect-adapter`

Поведение взаимодействует с host/process state и требует explicit authority: filesystem, HTTP, clock/sleep, stdout, plugins, lazy external entity retrieval.

### `implementation-artifact`

Деталь mutable JSON implementation, которую нельзя превращать в AVM semantics: JSON type tags, mutex containers, implicit `operator[]` create-on-access и подобное.

### `defer`

Поведение не переносится до появления достаточного lower-level contract/evidence. Главный пример — implicit parallel projection до доказанного effect/purity ordering.

## Исходный baseline #123

Первоначальная compatibility matrix была **planning snapshot до реализации AVM 1.5**. Поэтому статусы вида `partial`/`missing` из старой версии документа нельзя читать как состояние текущего `main`.

Ниже приведён актуальный overlay поверх того baseline.

## Текущее состояние доказанной совместимости

| Область | Legacy evidence | Текущий AVM status | Граница обещания |
|---|---|---|---|
| triune `ent/rel/sub/obj` | RM-TRIUNE-001 | contract proven | meaningful non-unit subject/object/relation execution |
| current/parent context | CTX-CURRENT-001, CTX-PARENT-001 | contract proven | immutable compositional context chain |
| `$ent/$rel/$sub/$obj` | REF-PRONOUN-001 | compiler/contract proven | canonical Current/Parent references |
| arbitrary textual/named/path addressing | REF-ABSOLUTE-001 | partial evidence only | не обобщать beyond proven compiler cases |
| missing textual reference | frozen missing-reference oracle | exact typed failure proven | exact marker -> `UnresolvedReference`; arbitrary unknown name remains unsupported |
| ordered executable sequence | EXEC-ARRAY-SEQ-001 | proven subset | deterministic canonical sequence semantics |
| foreach child context | EXEC-FOREACH-OBJ-001 subset | proven frozen case | deterministic sibling context; generic assignment не выводится автоматически |
| Boolean branch | CTRL-IF-001 | proven frozen case | lazy branch with threaded semantic context |
| Integer arithmetic | VALUE-ARITH-001 subset | canonical denotation + frozen arithmetic proven | только supported Integer operations/cases |
| Boolean/value equality | VALUE-COMPARE-001 subset | canonical Boolean/value contracts | без наследования JSON coercion quirks |
| Text denotation | VALUE-STRING-001 foundation | canonical Text proven | string library parity не заявляется |
| ordered list/value denotation | VALUE-COLLECTION-001 foundation | canonical list/value contract | полный `where/union/...` parity не заявляется |
| JSON get/set/erase quirks | VALUE-GETSET-001 | intentionally not migrated | implementation artifact |
| lazy external entity retrieval | REF-LAZY-DB-001 | capability boundary proven | explicit provider; реальный DB protocol не входит в contract |
| stdout/print | DISPLAY-PRINT-001 | tooling/effect only | не implicit core side effect |
| filesystem | EFFECT-FS-001 | capability architecture available | реальный FS adapter не доказан |
| HTTP | EFFECT-HTTP-001 | capability architecture available | реальный HTTP adapter не доказан |
| clock/sleep | EFFECT-TIME-001 | capability architecture available | wall-clock semantics не доказана |
| native/dynamic plugins | EFFECT-DLL-001 | capability architecture available | loader/ABI/plugin semantics не доказаны |
| parallel object projection | EXEC-OBJECT-PAR-001 | deferred | требуется отдельный purity/effect ordering proof |
| JSON как code/data/context | IMPL-JSON-AST-001 | removed | не возвращать второй semantic path |

Эта таблица описывает **уровень доказательства**, а не процент перенесённых строк legacy code.

## Доказанный AVM 1.5 corpus

Pinned executable migration ladder:

```text
CASE-ARITHMETIC                  -> 2
CASE-SEQUENCE-ORDER              -> 3
CASE-PURE-RELATION-COMPOSITION   -> 5
CASE-FOREACH-CONTEXT             -> [1,2,3]
CASE-BOOLEAN-BRANCH              -> 42
CASE-MISSING-REFERENCE           -> typed source failure
```

Это минимальный sufficient corpus для release proof. Он не является обещанием полного operator vocabulary старого runtime.

## Context и reference semantics

Legacy `vm_ctx` нес `ent/rel/sub/obj` и parent `$`. AVM заменяет mutable JSON context на canonical/immutable execution context contracts.

Важное различие:

```text
ExecutionContext.relation
!=
semantic relation_state
```

И pure result не означает скрытое:

```text
$rel := result
```

State transition выполняется только явной semantic relation/outcome.

Для references действует:

```text
parse/project reference
 -> pure resolve/find
 -> optional explicit host lookup effect
 -> optional explicit realization/write
```

Read miss не создаёт links.

## Граница отсутствующей ссылки

Exact frozen compatibility:

```text
__avm_missing_reference_oracle__
 -> MigrationFailureKind::UnresolvedReference
```

Произвольный неизвестный textual `$ref` не превращается в synthetic `LinkId`. Пока нет evidence/contract, он остаётся `InvalidSource` / unsupported.

Это сознательно более узкое обещание, чем «любой legacy name lookup работает».

## Sequence, projection и foreach

Executable JSON arrays в legacy runtime играли роль ordered execution structure. AVM сохраняет порядок через canonical link-native sequence/projection contracts.

Foreach доказан через deterministic child/sibling contexts. Frozen `CASE-FOREACH-CONTEXT` подтверждает точный migrated behavior; из него нельзя автоматически выводить весь legacy mutation vocabulary.

Implicit parallel execution не переносится. Параллелизм допустим только после отдельного contract, который делает observable ordering/effects явными.

## Values и canonical denotation

Legacy `nlohmann::json` coercion rules не являются нормативными для AVM.

AVM сначала определяет canonical denotation:

```text
Boolean
Integer
Text
ordered list/value structure
```

и только затем переносит behavior поверх этих identities.

Поэтому наличие старых operators `sqrt`, `split`, `join`, `where`, `union`, typed switch и других в legacy vocabulary **не означает**, что AVM обязан иметь одноимённый native handler.

Если operation выражается link-native composition, предпочтителен обычный AVM program/function.

## Explicit host effects и capability boundary

Legacy runtime мог смешивать pure lookup и external/database retrieval. После #129 это разделено архитектурно.

Первый доказанный effect slice — `REF-LAZY-DB-001`:

```text
canonical effect RelationEntity
 -> ordinary Executor
 -> explicit capability policy
 -> ExternalEntityProvider
 -> existing LinkId | deterministic failure
```

Доказаны:

- allow/deny authority;
- deterministic fake provider;
- provider miss/failure boundary;
- no-growth/read-vs-realize invariant;
- отсутствие provider dependency у pure programs;
- request/success/failure observability через existing observer;
- отсутствие второго EffectExecutor.

Не доказаны автоматически реальные filesystem paths, URLs, wall-clock behavior, DLL ABI или network protocol semantics.

См. [effect-capabilities.md](effect-capabilities.md).

## Сходимость JSON ↔ Anum

Native JSON и canonical Anum L3 сходятся к общему:

```text
ProjectionDescription
 -> find | realize
 -> canonical LinkStore
```

Versioned corpus:

```text
avm/frontend-common-denotation/v1
```

проверяет в том числе shared-substructure convergence. Разная frontend topology может обозначать один canonical graph после realization.

## Persistent release proof после reopen

Финальный proof использует context-sensitive program:

```text
1 + 1
$rel + 3
 -> 5
```

После первого explicit realization тот же `PersistentLinkStore` открывается заново. Existing root исполняется без legacy fixture, semantic migrator, reprojection и повторного realize.

Нормативный итог:

> После canonical realization source syntax не является runtime dependency AVM.

См. [avm-1.5-release-proof.md](avm-1.5-release-proof.md).

## Явные non-goals

AVM не обещает:

- одинаковые numeric `LinkId` в independent stores;
- одинаковый JSON tree/layout;
- одинаковый C++ exception type/text;
- quirks `nlohmann::json` coercion;
- automatic member/link creation на read;
- arbitrary textual-reference compatibility без evidence;
- implicit database/network/filesystem access;
- automatic parallel execution;
- полный `jsonRVM` operator parity;
- сохранение legacy implementation artifacts только ради source similarity.

## Как расширять compatibility дальше

Новая migration задача должна начинаться не с поиска одноимённого C++ handler, а с evidence:

```text
legacy behavior
 -> exact observable fixture/oracle
 -> категория semantics/projection/effect/artifact
 -> existing AVM lower-level contract?
 -> минимальный canonical vertical slice
 -> deterministic success/failure conformance
 -> full CI
```

Если evidence нет, behavior не считается обязательным. Если contract уже существует, новый слой не должен создавать parallel runtime path.

## Связанные документы

- [README документации](README.md) — карта нормативных и evidence документов;
- [triune-execution-contract.md](triune-execution-contract.md);
- [semantic-context-contract.md](semantic-context-contract.md);
- [reference-algebra.md](reference-algebra.md);
- [value-denotation-v1.md](value-denotation-v1.md);
- [jsonrvm-legacy-oracle.md](jsonrvm-legacy-oracle.md);
- [jsonrvm-semantic-migrator.md](jsonrvm-semantic-migrator.md);
- [avm-1.5-release-proof.md](avm-1.5-release-proof.md);
- [effect-capabilities.md](effect-capabilities.md).
