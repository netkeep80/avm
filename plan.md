# План развития AVM

## Правило планирования

Работа ведётся последовательными gates с явными зависимостями. Новый слой начинается только после того, как его зависимости получили стабильный контракт и зелёные CI-проверки.

Неизменный архитектурный инвариант:

```text
внешняя проекция / semantic adapter
  -> canonical denotation / ProjectionDescription
  -> find | explicit realize
  -> канонический LinkStore
  -> программа как LinkId
  -> BootstrapRuntime / Executor
  -> LinkId / ExecutionOutcome результата
```

Запрещены второй semantic path, скрытая база программ, legacy storage universe, compatibility Executor и backend-specific semantic index.

## Завершённые этапы

### AVM 1.0 — архитектурный фундамент ✅

Доказаны и приняты:

- один физический primitive `LinkId -> (begin,end)`;
- canonical pair identity;
- `find` как наблюдение и `intern/realize` как явная материализация;
- `(relation,subject,object) = Link(relation,Link(subject,object))`;
- один link-native `Executor` с dispatch по relation `LinkId`;
- programs/functions/bindings/call frames как links;
- parser-independent projection boundary;
- structural Anum L3→L4 adapter;
- `PersistentLinkStore` и reopen identity;
- удаление pointer-based `rel_t`, JSON semantic interpreter и protocol-only Anum bridge;
- portable Linux/Windows/macOS, package consumer, warnings-as-errors, ASan/UBSan и benchmark gates.

### AVM 1.1 — ассоциативные read-only queries ✅

Epic #83, реализация #84–#87.

`RelationQuery` использует только существующие `find/outgoing/incoming/get/contains`, не материализует данные и не создаёт отдельный semantic index. Дополнительные physical indexes отложены до реального workload/SLA.

### AVM 1.2 — структурная стандартная библиотека ✅

Epic #88, реализация #89–#94.

Приняты:

```text
link_begin
link_end
identity_equal
link_exists
pair_intern
```

Derived behavior по возможности выражается обычными AVM functions, а не новыми native handlers.

### AVM 1.3 — наблюдаемость исполнения ✅

Epic #95, реализация #96–#105.

Приняты:

```text
Enter(ExecutionContext)
Return(ExecutionContext,result)
Fail(ExecutionContext,phase)
```

Observer не управляет исполнением. `BoundedExecutionTrace` ограничен, детерминирован и не является VM state. Persistent reopen сохраняет exact trace identity в одном logical store; независимые stores сравниваются с точностью до переименования opaque LinkIds.

### AVM 1.4 — inspection tooling ✅

Типизированная `InspectionSession`, persistent inspection и scripted tooling используют существующие canonical APIs и один `Executor`; textual commands остаются presentation layer.

Persistent mutation path переведён в faulted state при неуспешной guarded mutation; tagged release зависит от полной portable matrix.

Документационный gate #134 также завершён: русский язык закреплён как нормативный для project-owned документации.

## AVM 1.5 — evidence-backed перенос Relations Model semantics ✅

Epic #122.

Главный вывод AVM 1.5:

```text
representation theorem
(rel,sub,obj) = Link(rel,Link(sub,obj))
```

сам по себе не переносит execution semantics. Поэтому перенос выполнен evidence-driven слоями поверх одного canonical runtime.

Pinned historical oracle:

```text
netkeep80/jsonRVM@843b3326141e090ccd1a106ba0a4a21ce72805b7
runtime 3.0.0
```

### Foundation gates #123–#128 ✅

Завершены:

- #123 — versioned semantic inventory и frozen differential corpus;
- #124 — triune execution contract;
- #125 — immutable `SemanticContextView`;
- #126 — canonical Current/Parent reference algebra и frontend compiler boundary;
- #127 — ordered sequence/projection/foreach semantics;
- #128 — canonical Boolean/Integer/Text/ordered-list value denotation.

Ключевые invariants:

```text
ExecutionContext.relation != semantic relation_state
result LinkId != implicit relation_state := result
find/resolve/query != realize/write/effect
textual frontend name != canonical LinkId
```

### Frozen semantic migration ladder ✅

Доказанный subset:

```text
CASE-ARITHMETIC                  -> 2       #196
CASE-SEQUENCE-ORDER              -> 3       #198
CASE-PURE-RELATION-COMPOSITION   -> 5       #198
CASE-FOREACH-CONTEXT             -> [1,2,3] #200
CASE-BOOLEAN-BRANCH              -> 42      #203
CASE-MISSING-REFERENCE           -> typed source failure #206/#211
```

#174 semantic-migrator umbrella закрыт после этого минимального corpus и не является endless operator-porting backlog.

#### Arithmetic / sequence / state

Pure Integer operations не мутируют semantic `$rel`. Legacy stateful composition выражается explicit canonical relations:

```text
commit_relation_state
resolve_reference_relation
apply_pure_relation
```

#### Foreach

Используется существующий deterministic sibling-context runtime. Exact frozen body `{"$rel":"=","$obj":{"$ref":"$obj"}}` доказан только как Current.Object identity projection; generic assignment semantics не выводится из одного fixture.

#### Boolean control

Existing lazy `if_relation` исправлен так, чтобы condition и выбранная branch сохраняли/thread-или `SemanticContextView`. Второй conditional executor не создан.

#### Missing reference

После #211 evidence boundary точная:

```text
__avm_missing_reference_oracle__
 -> MigrationFailureKind::UnresolvedReference

arbitrary unproven textual $ref
 -> InvalidSource / unsupported
```

Unknown textual name не превращается в synthetic LinkId.

### Gate #130 — JSON ↔ Anum common denotation ✅

PR #212 добавил versioned test-only corpus:

```text
avm/frontend-common-denotation/v1
```

Доказаны в обоих порядках JSON-first и Anum-first:

- anchor;
- ordered pair;
- nested pair;
- shared substructure;
- RelationEntity;
- executable `quote` root;
- non-mutating failure/miss matrix;
- repeated realization без роста store.

Ключевой proof:

```text
p = Link(a,b)
root = Link(p,p)
```

JSON может иметь две equal tree-occurrences `p`, Anum — один explicit shared node. `ProjectionDescription` topology может различаться, но canonical realization обязана сходиться к одной graph identity.

Один ordinary `Executor` исполняет общий root независимо от frontend provenance.

### Gate #131/#213 — persistent release proof ✅

Финальный context-sensitive program:

```text
1 + 1
$rel + 3
 -> 5
```

Import phase:

```text
legacy fixture
 -> existing semantic migrator
 -> duplet-json/1
 -> ProjectionDescription
 -> explicit realize exactly once
 -> ordinary Executor
```

После convergence сохраняются opaque LinkIds того же logical `PersistentLinkStore`:

- `BootstrapVocabulary`;
- `IntegerVocabulary`;
- `ReferenceVocabulary`;
- `SemanticExecutionVocabulary`;
- canonical Current.RelationState reference;
- program root;
- initial semantic-frame identities.

Reopen phase:

```text
BootstrapRuntime(reopened,saved_bootstrap)
 -> validate saved vocabularies
 -> register existing handlers
 -> execute existing persisted root
```

Reopen не читает legacy JSON, не вызывает semantic migrator, не строит `ProjectionDescription` и не вызывает `realize_projection`.

Два последовательных reopen подтверждают:

- result = Integer(5);
- final semantic relation-state = result;
- exact canonical Current.RelationState identity сохранена;
- runtime restore не создаёт replacement vocabularies;
- repeated converged execution не увеличивает store.

Нормативный итог AVM 1.5:

> После canonical realization source syntax не является runtime dependency AVM.

См. `docs/avm-1.5-release-proof.md`.

## Отдельный будущий gate — capabilities/effects #129 🚧

#129 **не блокирует завершение pure AVM 1.5**, потому что доказанный release corpus не использует filesystem/HTTP/time/database/native host effects как semantic operations.

Но #129 обязателен **до переноса первого реального host effect**.

Требуемый будущий контракт:

```text
canonical request
 -> explicit capability
 -> deterministic result / effect outcome
```

Запрещено добавлять dummy effect только ради закрытия release checklist.

## Дальнейшие направления после AVM 1.5

После закрытия pure AVM 1.5 можно рассматривать по реальным consumer requirements:

1. capability/effect model #129 перед первым host effect;
2. дополнительные evidence-backed Relations Model constructs;
3. interactive debugger с отдельным control contract;
4. дополнительные frontends поверх общего denotation contract;
5. production persistence backends;
6. visualization/GUI;
7. scheduler/parallel execution только после purity/effect proof;
8. distributed execution/storage;
9. JIT/native compilation;
10. дальнейшее расширение standard library через link-native composition.

## Правило зависимостей

Если gate зависит от незавершённого PR, независимая подготовка допустима, но dependent code не merge-ится до зелёного dependency gate.

После доказанной миграции legacy implementation удаляется, а не сохраняется вторым production path. Историю хранит Git.
