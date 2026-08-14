# Неразрешённое имя jsonRVM и граница canonical identity

Родительская задача: #205. Semantic migrator: #174. Reference contract: #126.

## Зачем нужен отдельный failure contract

Frozen `jsonRVM` corpus содержит сценарий:

```json
{
  "$rel/result": {
    "$ref": "__avm_missing_reference_oracle__"
  }
}
```

Pinned oracle завершает программу ошибкой и называет отсутствующее source name.

Главное для AVM — не воспроизвести текст C++/jsonRVM exception, а сохранить semantic boundary:

```text
source name не разрешено
    !=
canonical LinkId существует, но отсутствует в конкретном store
```

Эти ситуации нельзя сводить к одной операции через synthetic identity.

## Пространство имён source frontend

Legacy `$ref:"name"` содержит **текстовое имя frontend-а**. Сам текст не является canonical AVM LinkId.

Semantic migrator получает caller-owned bindings:

```text
LegacyNameBindings
name -> LinkId
```

Migrator:

- не принимает `LinkStore`;
- не создаёт point для имени;
- не пытается искать имя в database/host environment;
- не кеширует global symbol registry.

Отсутствующий binding означает source-level failure.

## Typed migration failure

`MigrationError` теперь имеет минимальную classification:

```text
UnsupportedSource
UnresolvedReference
```

Для unresolved name дополнительно сохраняются:

```text
source_path
source_identity
```

Frozen case должен возвращать:

```text
kind = UnresolvedReference
source_path = $.$rel/result.$ref
source_identity = __avm_missing_reference_oracle__
```

Human-readable `what()` содержит имя для диагностики, но точный текст сообщения не является canonical semantic contract.

Это не глобальная error ontology AVM. Classification добавлена только там, где differential compatibility требует различить source namespace failure от generic unsupported source shape.

## Почему нельзя создать synthetic LinkId

Запрещённый путь:

```text
"missing-name"
  -> create_point()
  -> Named(new_point)
  -> runtime miss
```

Он ложен по двум причинам.

Во-первых, source name, которого нет в caller namespace, внезапно становится существующей semantic identity.

Во-вторых, read/failure начинает materialize-ить LinkStore и нарушает фундаментальный invariant:

```text
find / resolve / verify != realize / write
```

Поэтому unresolved name завершает migration **до** `ProjectionDescription`, realization и Executor.

## Known binding компилируется structural

Если caller явно связывает имя с уже известным LinkId:

```text
"known-target" -> target_link_id
```

migrator строит canonical Named reference:

```text
Named(target_link_id)
```

в Native Duplet JSON как structural pair:

```text
Link(reference_named, target_link_id)
```

и оборачивает его в уже существующую executable relation:

```text
resolve_reference_relation
```

Transport использует:

```text
$symbol:"reference_named"
$link:<target LinkId>
```

`reference_named` разрешается caller-owned Native JSON symbol table в `ReferenceVocabulary::named_reference`.

`$link` не создаёт identity и не подтверждает наличие target в store; это explicit anchor declaration.

## Known LinkId, отсутствующий в store

Если caller namespace содержит binding:

```text
"known-but-absent" -> LinkId X
```

но конкретный `LinkStore` не содержит `X`, это уже **не UnresolvedReference source failure**.

Pipeline проходит migration и projection description construction, после чего существующие canonical contracts дают:

```text
find_projection -> miss, no mutation
realize_projection -> reject missing anchor before writes
```

Так сохраняется правильное разделение ответственности:

```text
frontend name resolution
    !=
store anchor validation
```

## Frozen differential conformance

Focused test `jsonrvm_missing_reference_test` проверяет три независимых сценария.

### 1. Frozen unknown name

Проверяется:

- exact frozen source marker;
- `MigrationFailureKind::UnresolvedReference`;
- exact source path/name fields;
- diagnostic содержит source name;
- `LinkStore::size()` не меняется;
- projection/realization/Executor не вызываются.

### 2. Known valid binding

Проверяется:

- caller binding компилируется в canonical Named reference;
- source DOM можно уничтожить после migration;
- projection/find не materialize-ят store;
- explicit realization создаёт canonical program;
- ordinary semantic reference handler возвращает exact target LinkId;
- semantic context остаётся неизменным;
- повторное execution после convergence не растит store.

### 3. Known binding на отсутствующий LinkId

Проверяется:

- migration успешно завершается, потому что source name разрешено;
- projection construction non-mutating;
- `find_projection` возвращает miss;
- `realize_projection` reject-ит absent anchor;
- store не растёт.

Именно этот третий сценарий не позволяет ошибочно смешать source-name failure с canonical store failure.

## Граница supported source

Первый #205 slice принимает top-level legacy envelope с exact:

```json
{"$ref":"name"}
```

Смешанные/неоднозначные object shapes, non-string `$ref` и invalid caller binding reject-ятся как `UnsupportedSource`.

Другие legacy reference forms уже имеют отдельный #126 compiler contract и могут добавляться в semantic migrator только по реальному frozen evidence/consumer requirement.

## Запреты

- synthetic `create_point()` для unknown textual name;
- implicit LinkStore lookup из migrator;
- database/network fallback;
- global mutable source-name registry;
- перевод exact host exception message в canonical semantics;
- запуск Executor для unresolved source name;
- скрытая realization ради проверки существования;
- смешение unknown source name и absent known LinkId.

## Следующий архитектурный шаг

После #205 первоначальный frozen corpus #123 покрыт success и failure cases.

Следующая работа должна перейти к:

1. #130 — common-denotation JSON/Anum equivalence proof;
2. #131 — end-to-end AVM 1.5 release assembly и persistent reopen без remigration;
3. #129 — capability/effect boundary до первого реального host effect.

Новые legacy constructs добавляются только если появляется реальный consumer/evidence, а не ради формальной полноты старого operator vocabulary.