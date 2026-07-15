# Primitive Literal Patterns Design

## Scope

Make the literal pattern forms already accepted by the parser executable for
`nil`, booleans, numbers, and strings. Literal patterns are valid at the top
level when the match scrutinee has a compatible primitive type, and inside
variant payload patterns. Nullable primitive values may use `nil` together
with patterns for the non-null payload type. Existing nullable-enum behavior
is unchanged.

This slice does not add OR patterns, range patterns, array/map patterns,
struct patterns, or new bytecode operations.

## Static semantics

- `nil` matches `nil` or a nullable value and contributes coverage for the
  nullable case.
- `true` and `false` match `bool` values only. An unguarded match covering both
  literals is exhaustive for `bool`; a nullable `bool?` also needs `nil`.
- Numeric and string literals match `number` and `string` respectively. Their
  domains are open-ended, so an unguarded wildcard or binding is required for
  exhaustiveness. A nullable number or string also needs `nil`.
- A literal nested in an enum variant payload is checked against that payload's
  declared type. Enum coverage remains variant-based, as in existing nested
  patterns; guards do not contribute coverage.
- A literal with an incompatible known type is a type error. Primitive matches
  with unknown scrutinee types remain rejected because exhaustiveness cannot be
  established statically.

## Lowering and runtime

Literal patterns continue to lower to a constant followed by the existing
`Equal` operation and a conditional jump. No IR opcode, bytecode opcode,
`.cdbc` format, or Rust VM dispatch change is required. The existing source
order and first-matching-arm behavior apply to literal arms.

## Verification

Add statement- and expression-level success coverage for primitive and nullable
primitive matches, nested enum payload literals, and runtime parity through
bytecode artifacts and the Rust VM. Add type-error coverage for incompatible
literal payloads and non-exhaustive boolean, open-domain, and nullable matches.
