# Named Struct Record Patterns Design

## Scope

Add named record patterns for existing declared structs:

```cd
Person { name: "Ada", age: age }
```

Patterns may list any subset of declared fields, fields may appear in any
order, and omitted fields behave as wildcards. Record patterns may be nested
inside other record or enum patterns and may use existing OR patterns. A
qualified form such as `geo.Point { x: x }` is accepted for namespace-imported
struct types. Nullable named structs may be matched with `nil` and a record
pattern.

This slice does not add anonymous record patterns, record construction,
structural typing, rest syntax, or dynamic dispatch.

## Static semantics

- A record pattern must name the expected named struct type. A qualified
  pattern must match the expected qualified type name.
- Every listed field must exist and may occur only once. Each field pattern is
  checked against the declared field type, including nullable, enum, nested
  record, literal, and OR patterns.
- Omitted fields are wildcards. A record pattern is universal when every
  declared field is omitted or has a universal subpattern. `_` and binding
  patterns remain universal for the complete struct value.
- A non-nullable struct match is exhaustive when it contains a universal
  pattern. A nullable struct match must additionally cover `nil`.
- Record pattern bindings are arm-local and follow the existing OR binding
  rule when record patterns appear as OR alternatives.
- Runtime struct values retain their existing nominal type metadata. Static
  struct matching validates the type before lowering; field reads use declared
  field names.

## Lowering and runtime

Each listed field lowers through the existing `field` IR/bytecode operation and
then recursively compiles its subpattern. Omitted fields emit no instructions.
No IR opcode, bytecode opcode, `.cdbc` format, or Rust VM dispatch change is
required.

## Verification

Add statement- and expression-level success coverage for reordered/partial
fields, nested records, nullable structs, imported qualified records, and
record patterns combined with OR patterns. Add parse/type-error coverage for
unknown and duplicate fields, wrong expected types, non-exhaustive matches, and
missing nullable coverage. Verify bytecode artifacts and Rust VM parity.
