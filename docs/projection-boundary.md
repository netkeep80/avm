# ProjectionDescription: parser-independent find/realize boundary

`ProjectionDescription` is the neutral boundary between an external protocol/model projection and AVM storage. It deliberately contains no JSON syntax, Anum abits, parser state, quotation rules or execution semantics.

## Why projection points are not implicit

`InMemoryLinkStore::create_point()` creates a unique identity represented physically as a self-link `{id,id}`. Two points are therefore different identities even though both have the same structural shape "self-link".

A read-only query cannot correctly discover an anonymous point merely by looking for a self-link: there can be many such links and none is structurally preferred. Hiding an external-name-to-point map inside `ProjectionDescription` would turn the memory layer into an identity resolver.

AVM 1.0 v0.1 therefore uses two explicit reference kinds:

```text
Anchor(LinkId)    identity already resolved by the external adapter/context
Node(NodeId)      result of an earlier dyad node in the same projection
```

The projection boundary itself never calls `create_point()`.

## Description graph

A projection is a topologically ordered list of dyads plus a root reference:

```text
node[0] = Link(Anchor(a), Anchor(b))
node[1] = Link(Node(0),   Anchor(c))
root    = Node(1)
```

A node may only reference an earlier node. This matches the current immutable `LinkStore` construction model and makes cycles/forward references explicit validation errors rather than partially materialized states.

The root may also be an `Anchor`, including for an empty node list.

## `find_projection`

```text
find_projection(const LinkStore&, description)
    -> optional<ProjectionResult>
```

The operation is query-only by type and behavior:

1. validate the description structure;
2. resolve anchors with `contains`;
3. resolve each dyad only with `find(begin,end)`;
4. return absence as soon as a required anchor or dyad is missing;
5. return the existing canonical LinkIds when the complete projection exists.

It never calls `create_point` or `intern`. Tests assert store size before/after successful and unsuccessful queries.

## `realize_projection`

```text
realize_projection(LinkStore&, description)
    -> ProjectionResult
```

Realization is the explicit mutating operation:

1. validate the full projection before writes;
2. verify **all anchors** before writes;
3. process nodes in topological order with `intern(begin,end)`;
4. reuse existing canonical links where available;
5. return the root and per-node LinkIds.

Pre-validating every anchor prevents an ordinary missing-anchor error from creating a valid prefix and then failing halfway through the description.

Repeated realization of the same description over the same anchors is idempotent because `intern` is canonical.

## Identity ownership

The external adapter/context owns the question "which existing AVM identity corresponds to this protocol-level entity?" and supplies an `Anchor(LinkId)` after resolving it.

If a future protocol needs persistent external-symbol-to-point identity creation, that must become a separate explicit resolver contract. It must not be smuggled into query semantics, because doing so would violate the `find`-does-not-create invariant.

## Relationship to Anum/MTS

This layer corresponds to an L3/L4 boundary, not to Anum syntax itself:

```text
external raw source
  -> external parse / validate / quote / project(context)
  -> ProjectionDescription
  -> AVM find_projection | realize_projection
  -> canonical LinkStore denotation
```

`anum_docs` can later provide the real parser/projector adapter. AVM only knows the completed structural description and resolved anchors.
