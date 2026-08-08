# Relations Model query contract

## Status

This document defines the public AVM 1.1 read-only query layer over canonical Relations Model entities.

The physical and semantic foundations remain unchanged:

```text
LinkId -> (begin, end)
(relation, subject, object) = (relation, (subject, object))
```

Queries are a derived read-only view over the existing `LinkStore` contract. They do not introduce a second database, index universe, entity registry or execution path.

## Public API

```cpp
#include <avm/relations_query.h>

avm::RelationQuery query{
    .relation = relation_or_nullopt,
    .subject = subject_or_nullopt,
    .object = object_or_nullopt,
};

std::vector<avm::RelationMatch> matches =
    avm::query_relation_entities(store, query);
```

A `RelationMatch` contains:

```text
entity_id  canonical LinkId of the outer relation link
entity     decoded RelationEntity {relation, subject, object}
```

At least one field must be constrained. An all-wildcard query is rejected because `LinkStore` deliberately exposes no full-store enumeration contract.

## Existing-index strategies

The query layer uses exactly the indexes already implied by the `LinkStore` API.

### Relation constrained

```text
outgoing(relation)
-> outer candidate links
-> decode + filter remaining fields
```

### Subject and object constrained, relation wildcard

```text
find(subject, object)
-> canonical subject/object pair, if already present
-> incoming(pair)
-> decode + filter
```

The query never calls `intern(subject, object)`. A missing pair produces an empty result without mutation.

### Subject constrained

```text
outgoing(subject)
-> candidate pair links
-> incoming(pair)
-> outer candidate links
-> decode + filter
```

### Object constrained

```text
incoming(object)
-> candidate pair links
-> incoming(pair)
-> outer candidate links
-> decode + filter
```

No backend-specific containers are visible to this layer.

## Structural totality

AVM does not maintain a hidden registry of links previously created through `encode_relation_entity`.

`decode_relation_entity(store, id)` is structural: for any existing outer link, its `end` is itself an existing link and therefore supplies the subject/object pair. Consequently a point:

```text
x = (x, x)
```

also structurally represents:

```text
relation = x
subject  = x
object   = x
```

Likewise, intermediate pair links may themselves satisfy a broader structural query. This is intentional. Introducing an external "this LinkId is an entity" registry would create a second semantic identity universe and contradict the Relations Model representation.

Applications that need a narrower domain should express that domain through additional relations/constraints rather than hidden C++ classification state.

## Result semantics

Results are:

- observational: the store size and canonical identities do not change;
- deterministic: sorted by ascending `entity_id`;
- defensively deduplicated by `entity_id`;
- structurally verified: every returned `entity` equals `decode_relation_entity(store, entity_id)`.

Unknown constrained LinkIds yield an empty result. Querying does not materialize missing links.

## Backend equivalence

The same query fixture is required to return equivalent results for:

```text
InMemoryLinkStore
PersistentLinkStore before close
PersistentLinkStore after reopen
```

This keeps query semantics above the backend boundary.

## Explicit non-goals

AVM 1.1 query v1 does not add:

```text
full-store enumeration
SQL/query-language parsing
planner statistics
secondary persistent indexes
hidden entity registries
mutation through queries
JSON or Anum query semantics
```

A future index extension must first demonstrate a measured need against the benchmark baselines and then extend the `LinkStore` contract explicitly rather than inspecting backend internals.

## Quality gates

CI rejects production `relations_query.h` if it introduces:

- `intern()` or `create_point()`;
- guessed enumeration through `store.size()`;
- direct dependency on `InMemoryLinkStore` or `PersistentLinkStore`;
- JSON/Anum dependencies.

Benchmark baselines cover relation-, subject-, object- and exact subject+object-driven queries.
