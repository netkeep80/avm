# Доказательства готовности AVM 1.5

Epic: #122. Финальный pure release gate: #131/#213.

AVM 1.5 доказывает перенос **ограниченного, явно зафиксированного subset** семантики Relations Model из historical `jsonRVM` в один canonical link-native runtime AVM.

Это не заявление о полной совместимости со всем `base.rm.h` и не обещание автоматического переноса любого legacy relation.

## Что именно является release proof

После frontend/migration boundary существует один путь:

```text
source / frontend
 -> canonical denotation / ProjectionDescription
 -> find | explicit realize
 -> canonical LinkStore
 -> RelationEntity = Link(rel, Link(sub,obj))
 -> ordinary Executor
```

Legacy JSON interpreter, compatibility Executor и второй storage universe отсутствуют.

Release proof намеренно использует pure deterministic corpus. Host effects проверяются отдельным capability contract и не нужны для доказательства того, что canonical AVM runtime больше не зависит от legacy source после realization.

## Замороженный semantic corpus

Pinned historical oracle:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

Доказанный corpus:

```text
CASE-ARITHMETIC                  -> 2       #196
CASE-SEQUENCE-ORDER              -> 3       #198
CASE-PURE-RELATION-COMPOSITION   -> 5       #198
CASE-FOREACH-CONTEXT             -> [1,2,3] #200
CASE-BOOLEAN-BRANCH              -> 42      #203
CASE-MISSING-REFERENCE           -> typed source failure #206/#211
```

Stateful compatibility выражается явными canonical primitives/context outcomes, а не скрытой мутацией pure operations.

## Точная граница отсутствующей ссылки

После #211 typed compatibility доказана только для exact frozen marker:

```text
__avm_missing_reference_oracle__
 -> MigrationFailureKind::UnresolvedReference
```

Произвольный неподтверждённый textual `$ref` остаётся `InvalidSource` / unsupported.

AVM не создаёт synthetic `LinkId` и не вводит runtime miss только ради представления неизвестного source name.

## Независимость от frontend provenance

#212 добавил versioned corpus:

```text
avm/frontend-common-denotation/v1
```

Native JSON и canonical Anum L3 независимо строят `ProjectionDescription`, после чего проверяется одна canonical graph semantics.

Ключевой case:

```text
p = Link(a,b)
root = Link(p,p)
```

JSON содержит две одинаковые tree-occurrences `p`, Anum — один shared node. После realization:

```text
root.begin == root.end == p
p == Link(a,b)
```

Один ordinary `Executor` исполняет общий `quote` root независимо от frontend provenance.

## Persistent reopen без remigration

Финальный #213 proof использует context-sensitive frozen program:

```text
1 + 1
$rel + 3
 -> 5
```

Import scope:

```text
legacy source
 -> existing semantic migrator
 -> duplet-json/1
 -> ProjectionDescription
 -> explicit realize once
 -> ordinary Executor
 -> result 5 / relation_state = result
```

Между scopes сохраняются только opaque `LinkId` того же logical `PersistentLinkStore`:

```text
BootstrapVocabulary
IntegerVocabulary
ReferenceVocabulary
SemanticExecutionVocabulary
Current.RelationState reference
program root
initial semantic-frame identities
```

После close/reopen:

```text
BootstrapRuntime(store, saved_bootstrap)
 -> validate saved vocabularies
 -> register existing handlers
 -> execute existing persisted root
```

Reopen path не читает legacy fixture, не вызывает semantic migrator, не строит `ProjectionDescription` и не вызывает `realize_projection`.

После convergence runtime restore и repeated execution не увеличивают store.

Нормативный вывод:

> После canonical realization source syntax не является runtime dependency AVM.

## Граница host effects после release proof

Pure AVM 1.5 release corpus по-прежнему не использует filesystem/HTTP/time/database/native host effects как semantic operations. Это свойство proof, а не пробел в архитектуре.

После завершения pure release gate отдельный #129 доказал первый explicit capability boundary на evidence case `REF-LAZY-DB-001`:

```text
canonical effect RelationEntity
 -> ordinary Executor
 -> explicit capability policy
 -> explicit ExternalEntityProvider
 -> existing LinkId | deterministic failure
```

Таким образом теперь доказаны **две разные вещи**:

1. pure AVM 1.5 runtime не зависит от legacy source после canonical realization;
2. host authority может быть добавлена к тому же ordinary `Executor` явно, без второго runtime и без hidden materialization.

#129 не расширяет frozen pure corpus и не означает готовность реальных DB/FS/HTTP/clock/native adapters. Он фиксирует архитектурный контракт, поверх которого такие adapters могут появляться по отдельным evidence-backed задачам.

См. [capability boundary](effect-capabilities.md).

## Запреты release proof

- никакого второго Executor/interpreter;
- никакой remigration после reopen;
- никакой зависимости runtime от legacy source DOM/file;
- никаких replacement vocabulary identities при reopen;
- никакого synthetic release-only opcode;
- никакой полной jsonRVM parity без evidence;
- никакой автоматической совместимости arbitrary textual `$ref`;
- никакого cross-store сравнения numeric `LinkId` как универсального значения;
- никакого скрытого host lookup внутри pure reference resolver.

## Проверки CI

Принятый gate требует exact-head green для применимых workflow:

- Quality gates;
- Core warnings-as-errors;
- JSON compatibility;
- CLI;
- ASan+UBSan;
- installed-package consumers;
- portable Linux/macOS/Windows;
- Documentation language;
- Benchmark;
- Showcase, если workflow triggered.

## Связанные документы

- [README документации](README.md) — карта contract/evidence документов;
- [jsonrvm-compatibility.md](jsonrvm-compatibility.md) — границы совместимости и frozen evidence;
- [jsonrvm-semantic-migrator.md](jsonrvm-semantic-migrator.md) — semantic migration boundary;
- [effect-capabilities.md](effect-capabilities.md) — explicit authority для host effects;
- `../compat/jsonrvm-semantics.json` — machine-readable inventory.
