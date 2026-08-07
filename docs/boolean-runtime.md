# AVM 1.0 associative Boolean runtime

The bootstrap runtime executes the first link-native AVM program subset without JSON semantics or string operator dispatch.

## Truth tables are data

Boolean semantics are materialized in `LinkStore` as Relations Model entities. Native C++ handlers do not encode the truth table with `if`/`switch` statements; they evaluate arguments and perform associative lookup.

Unary NOT rows are represented as:

```text
(not, true,  false)
(not, false, true)
```

Binary functions use a canonical argument-pair LinkId as the subject:

```text
key = Link(left, right)
(and, key, result)
(or,  key, result)
```

The lookup path uses `LinkStore::find(left, right)` and therefore does not materialize missing argument keys while evaluating a query.

## Executable expressions

Program expressions keep the #35 shape:

```text
(relation, unit, payload)
```

The bootstrap handlers currently cover:

- quote/literal;
- sequence;
- NOT;
- AND;
- OR;
- IF.

Expression handlers reject Relations Model rows whose subject is not the `unit` execution marker. This keeps truth-table data and executable application nodes in the same relation namespace without conflating their roles.

## Lazy IF

`IF` uses two associative condition rows:

```text
(if, true,  true)
(if, false, false)
```

Execution is:

1. evaluate only the condition expression;
2. resolve its Boolean selector through the stored relation;
3. execute exactly one selected branch.

The test suite proves laziness by placing an entity with an unregistered relation in the unselected branch. The expression succeeds while the same entity fails when moved into the selected branch.

## Mutation rule

All truth-table rows are materialized during `BootstrapRuntime` construction. Executing an already constructed Boolean program performs read-only table lookup; tests assert that `LinkStore::size()` does not change for nested Boolean evaluation or invalid Boolean lookup.

Runtime frame/binding materialization is intentionally not part of this gate. Calls and parameters are implemented by #37.
