# Anum L3 -> AVM L4 bridge

## Scope

This adapter is the integration boundary between the canonical Anum/MTS L3 protocol and AVM L4 storage/projection. It deliberately does **not** implement Anum syntax, validation, context selection or protocol projection.

The canonical L3 pipeline lives in `netkeep80/anum_docs`:

```text
raw [ ] 1 0 carrier
  -> parse_raw_quaternary
  -> validate_anum(explicit context)
  -> project_anum(explicit context)
  -> typed L3 projection result
```

AVM begins only after that typed result exists:

```text
typed L3 projection result
  -> Anum projection bridge
  -> optional ProjectionDescription
  -> find_projection | realize_projection
  -> LinkStore
```

This keeps grammar/context semantics out of the VM and preserves the AVM rule that `find` is observational while `realize` is the explicit materializing operation.

## Canonical L3 result categories

The current Anum protocol v0.1 distinguishes four projection kinds:

```text
protocol-value
boundary-form
quoted-raw
raw
```

The bridge mirrors these categories only as a typed handoff contract. It does not derive them from `[`, `]`, `1` or `0` and does not inspect a raw carrier.

## Defined L4 mapping

Only the currently defined protocol values have an AVM L4 mapping:

```text
protocol-value 0 -> caller supplied zero LinkId anchor
protocol-value 1 -> caller supplied one LinkId anchor
```

The caller resolves those identities in the target logical `LinkStore`. The adapter does not create point identities and does not guess that an AVM Boolean vocabulary identity is the same thing as an Anum protocol value.

The result is an anchor-only `ProjectionDescription`. Therefore:

- bridge construction does not mutate the store;
- `find_projection` succeeds only when the selected anchor is already present;
- `find_projection` never creates the anchor;
- `realize_projection` validates that the anchor exists but creates no new projection nodes for this mapping.

## Unresolved L3 results

These current L3 result kinds have no general L4 denotation in the canonical v0.1 protocol:

```text
boundary-form
quoted-raw
raw
```

The bridge returns no `ProjectionDescription` for them. It does not replace an unresolved denotation with a synthetic link.

In particular, the canonical protocol currently treats `[[` and `]]` as boundary forms without a protocol value, preserves relative-context carriers as raw, and returns typed raw results when no general root denotation is assigned.

## Experimental root projection

The current canonical L3 implementation contains the experimental root-context candidates:

```text
[] -> protocol value 0
][ -> protocol value 1
```

The AVM adapter does not encode those character patterns. It accepts only the already projected typed result. Consequently a future change in the L3 grammar or context rules does not require a second parser inside AVM.

The bridge also makes no claim that the experimental protocol projection is a normative L1 identity or formula.

## Validation

Malformed handoff data is rejected before it reaches `find_projection` or `realize_projection`:

- a `protocol-value` result without a value is invalid;
- a non-protocol result carrying a protocol value is invalid;
- zero/one anchors must be valid and distinct `LinkId` values.

Whether an otherwise valid anchor actually exists in a particular store remains the responsibility of the existing AVM projection operations.

## General anum deserialization is intentionally not implemented

AVM issue #79 records the remaining theory dependency. A recursive transformation of arbitrary raw `[ ] 1 0` carriers into link structures cannot be implemented correctly until the canonical MTS/Anum specification defines that denotation, including any boundary/relative rules and inverse serialization requirements.

Until then, preserving a typed unresolved result is part of correctness. Inventing a link structure would create an undocumented semantic path and violate the L3/L4 boundary.
