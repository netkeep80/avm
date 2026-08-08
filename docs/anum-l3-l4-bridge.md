# Структурный мост денотации Anum L3 → AVM L4

## Статус

AVM принимает канонический storage-neutral handoff `AnumDenotation` v0.2, формируемый `netkeep80/anum_docs`.

Production adapter:

```text
adapters/anum_denotation_bridge.h
```

Старый protocol-value-only bridge `AnumL3Projection` удалён. Существует один путь L3→L4.

## Разделение ответственности

Upstream L3 отвечает за:

```text
parsing raw [ ] 1 0
валидацию контекста
root / quote / relative projection
root-opening collapse
recursive structural denotation
canonical inverse serialization
```

AVM не определяет ни одно из этих правил.

AVM получает только typed result одного из видов:

```text
structural
raw
quoted-raw
```

Для `structural` handoff содержит:

```text
отсортированные opaque anchor keys
топологически упорядоченные description-local nodes
start/end refs на anchors или более ранние nodes
один root ref
```

## Отображение в ProjectionDescription

Caller передаёт resolver anchors:

```text
opaque upstream anchor key -> LinkId
```

Bridge выполняет только структурное преобразование:

```text
Anum anchor ref -> ProjectionRef::anchor(resolved LinkId)
Anum node ref   -> ProjectionRef::node(same local id)
Anum node       -> ProjectionNode(start,end)
Anum root       -> ProjectionDescription.root
```

Порядок nodes и повторные references сохраняются точно. Adapter не intern-ит и не deduplicate-ит структуру.

`raw` и `quoted-raw` не создают `ProjectionDescription`.

## Эффекты

Bridge не принимает `LinkStore`, поэтому не может читать или изменять память AVM.

L4 effects остаются явными:

```text
bridge_anum_denotation(...)  # только structural translation
find_projection(...)         # observation, без materialization
realize_projection(...)      # явная materialization
```

Если resolver возвращает синтаксически допустимый `LinkId`, отсутствующий в конкретном store, bridge всё равно остаётся pure. `find_projection` возвращает отсутствие без mutation, а `realize_projection` отклоняет missing physical anchor до создания обычных projection nodes.

## Граница валидации

AVM проверяет transport contract, а не грамматику Anum:

- anchor keys отсортированы, уникальны и непусты;
- node IDs непрерывны и топологически корректны;
- anchor refs ссылаются на объявленные anchors;
- node refs указывают только на более ранние nodes;
- structural и non-structural payload shapes не смешиваются;
- каждый structural anchor разрешается в non-invalid `LinkId`.

AVM намеренно **не** проверяет:

```text
валиден ли raw carrier как Anum
кодируют ли скобки конкретную вложенную связь
корректно ли выполнен root-opening collapse
какой ProjectionContext следует использовать
```

Это каноническая ответственность L3.

## Межъязыковой conformance

Adapter tests используют versioned snapshots из `netkeep80/anum_docs` v0.2:

```text
test/conformance/anum-denotation-conformance-v0.2.json
test/conformance/anum-recursive-denotation-conformance-v0.2.json
test/conformance/anum-v0.2-provenance.json
```

JSON parsing применяется только в тестах. `adapters/anum_denotation_bridge.h` остаётся JSON-free и parser-free.

Tests проверяют generic anchor-only/nested/shared-substructure vectors, реальные recursive L3 expected denotations и существующий lifecycle AVM projection.

## Идентичность

Upstream node IDs — локальные позиции description, а не persistent identities. Bridge отображает их 1:1 в `ProjectionNodeId` только внутри одной projection description.

Физическая identity остаётся ответственностью `LinkStore`. Equal pairs могут сойтись при explicit realization; L3 и bridge не pre-intern-ят occurrence structure.
