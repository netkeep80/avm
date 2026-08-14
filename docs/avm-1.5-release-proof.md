# Доказательства готовности AVM 1.5

Epic: #122. Финальный release gate: #131/#213.

AVM 1.5 доказывает перенос ограниченного, явно зафиксированного subset семантики Relations Model из historical `jsonRVM` в один link-native runtime AVM.

Это **не** заявление о полной совместимости со всем `base.rm.h`.

## Канонический runtime

После frontend/migration boundary существует только один путь:

```text
source / frontend
 -> canonical denotation / ProjectionDescription
 -> find | explicit realize
 -> canonical LinkStore
 -> RelationEntity = Link(rel, Link(sub,obj))
 -> ordinary Executor
```

Legacy JSON interpreter, compatibility Executor и второй storage universe отсутствуют.

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

Не создаются synthetic LinkId и runtime miss только ради представления неизвестного source name.

## Независимость от frontend provenance

#212 добавил versioned test-only corpus:

```text
avm/frontend-common-denotation/v1
```

Native JSON и canonical Anum L3 независимо строят `ProjectionDescription`, после чего доказывается одна canonical graph semantics.

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

Один ordinary `BootstrapRuntime::Executor` исполняет общий `quote` root независимо от frontend provenance.

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

Между scopes сохраняются только opaque LinkIds того же logical `PersistentLinkStore`:

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

## Граница effects

Pure AVM 1.5 release proof не использует filesystem/HTTP/time/database/native host effects как semantic operations.

Issue #129 остаётся отдельным обязательным prerequisite **до переноса первого реального host effect**. Dummy effect ради release checklist не добавляется.

## Запреты release gate

- никакого второго Executor/interpreter;
- никакой remigration после reopen;
- никакой зависимости runtime от legacy source DOM/file;
- никаких replacement vocabulary identities при reopen;
- никакого synthetic release-only opcode;
- никакой полной jsonRVM parity без evidence;
- никакой автоматической совместимости arbitrary textual `$ref`;
- никакого cross-store сравнения numeric LinkId как универсального значения.

## Проверки CI

Финальный merge AVM 1.5 требует exact-head green:

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
