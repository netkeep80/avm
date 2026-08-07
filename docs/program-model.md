# AVM 1.0 link-native program model

This document defines the first bootstrap program representation for AVM 1.0. It is an engineering protocol built on the canonical `LinkStore` and Relations Model codec. It is not declared to be a fundamental MTS vocabulary.

## Core invariant

After projection/import, executable program structure lives in the same link store as data:

```text
program representation ∈ LinkStore
```

The executor must not require operator names, JSON nodes, C++ function-definition maps or parameter-name maps to understand the structure.

## Bootstrap vocabulary

`BootstrapVocabulary` materializes opaque LinkId identities for:

- `unit` — marker used as the subject of executable expression entities;
- `nil` — terminator for link-list chains;
- Boolean values `true_value` and `false_value`;
- expression relations: quote, parameter, sequence, NOT, AND, OR, IF and CALL;
- runtime structure relations: function, binding and frame.

The IDs are deliberately opaque. Textual names are projection-level labels and do not participate in core identity or dispatch.

## Expression shape

Executable expression nodes are Relations Model entities:

```text
Expression(relation, payload)
    = (relation, unit, payload)
    = Link(relation, Link(unit, payload))
```

Examples:

```text
Literal(value)     = (quote, unit, value)
Parameter(formal)  = (parameter, unit, formal)
NOT(arg)           = (not, unit, List(arg))
AND(a,b)           = (and, unit, List(a,b))
OR(a,b)            = (or, unit, List(a,b))
IF(c,t,e)          = (if, unit, List(c,t,e))
Sequence(e...)     = (sequence, unit, List(e...))
```

Repeated materialization of the same immutable structure reuses the canonical LinkId because all constituent dyads are interned.

## Lists

Argument, parameter and sequence collections use a canonical dyad chain:

```text
List()      = nil
List(a,b,c) = Link(a, Link(b, Link(c, nil)))
```

`decode_link_list` detects cycles, missing LinkIds and a configurable maximum item count. List decoding therefore does not rely on recursion or on JSON arrays.

## Functions

A function handle is an independently created point. This indirection is important: the body may reference the handle before the immutable function definition is materialized, which makes recursive program graphs representable without mutable link identities.

A definition is:

```text
params = List(formal1, formal2, ...)
payload = Link(params, bodyRoot)
definition = (function, handle, payload)
```

A handle has at most one definition. Repeating an identical definition is idempotent; attempting to attach a different definition to the same handle is an error.

A call expression is:

```text
args = List(actualExpr1, actualExpr2, ...)
payload = Link(functionHandle, args)
call = (call, unit, payload)
```

At this stage the model only represents calls. Frame materialization, binding lookup and recursion semantics are implemented in the next runtime gate (#37).

## Boundary with JSON and Anum

This layer does not parse JSON and does not parse Anum/abits. A projection layer may map external names and syntax to vocabulary LinkIds and construct this graph, but execution receives only LinkIds and runtime vocabulary.

That boundary is intentional: AVM storage/execution semantics must not become another parser implementation, and future Anum integration should connect through the projection/load/find/realize boundary described by #26.
