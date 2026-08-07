# jsonRVM / AVM compatibility

This document fixes the first compatibility contract between the triplet-based Relations Model used by `jsonRVM` and the dyadic associative network used by AVM.

## Relations Model entity

`jsonRVM` represents a triune entity with three roles:

```text
(relation, subject, object)
```

The JSON projection uses:

```json
{
  "$rel": "relation",
  "$sub": "subject",
  "$obj": "object"
}
```

The execution meaning belongs to the Relations Model/executor layer. This document only defines the structural projection into AVM links.

## Canonical dyadic projection

AVM uses nested ordered pairs in one fixed order:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

Therefore:

```text
(relation, subject, object)
=
(relation, (subject, object))
```

No separate physical triplet record is required by the AVM core.

## Round-trip

For a materialized triplet `t`:

```text
encode(t) -> entity LinkId
decode(entity LinkId) -> t
```

must preserve all three roles exactly.

Because both inner and outer pairs are canonical links, repeated encoding returns the same identities inside one logical store.

## Non-mutating lookup

Finding a triplet is intentionally different from encoding/materializing it.

```text
find_relation_entity(relation, subject, object)
```

performs:

```text
find(subject, object)
then, only if it exists,
find(relation, subject_object)
```

It must not create the inner `(subject, object)` pair while checking whether the triplet exists.

This is the same engineering distinction later required by the Anum boundary:

```text
description/query != realization/write
```

## Recursive use

Every role is a `LinkId`, so an encoded entity can itself be used as the relation, subject or object of another entity. This preserves the recursive/homoiconic character of the Relations Model without adding another storage primitive.

## Execution is the next layer

This codec does not:

- dispatch a relation;
- parse JSON;
- interpret `$ref` paths;
- define MVC behavior;
- implement Anum semantics.

Those operations build on top of the structural identity established here.
