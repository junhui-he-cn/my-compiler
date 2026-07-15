# OR Patterns Design

## Scope

Add the `pattern | pattern` syntax to statement- and expression-level
`match`. Alternatives are tried from left to right, and the arm succeeds when
the first alternative succeeds. OR patterns may be nested inside variant
payloads and may combine existing wildcard, binding, literal, nullable, and
variant patterns.

The single `|` token is distinct from the existing logical `||` operator.
This slice does not add pattern grouping, range patterns, array/map patterns,
struct patterns, or rest patterns.

## Static semantics

- Every alternative is checked against the same expected scrutinee or payload
  type.
- Every alternative must bind exactly the same set of source names. A binding
  with the same name must have mutually compatible types in every alternative.
  The binding is declared once in the arm-local scope and is available to the
  arm guard/body or match-expression result.
- Coverage from an unguarded OR pattern is the union of the alternatives'
  coverage. An OR pattern is universal if any alternative is universal.
  Existing guarded-arm rules remain unchanged: a guarded arm contributes no
  coverage, including when its pattern is an OR pattern.
- Pattern alternatives are not independently arm-scoped; successful bindings
  are unified into one arm-local binding so the arm cannot observe which
  alternative matched.

## Lowering and runtime

OR patterns lower to existing conditional jumps. Each alternative's failure
jumps to the next alternative; successful alternatives copy their captures
into shared registers before joining the arm's existing binding/guard path.
The last alternative's failures flow to the next match arm. No IR opcode,
bytecode opcode, `.cdbc` format, or Rust VM dispatch change is required.

## Verification

Add success coverage for literal, nullable, nested variant, and binding OR
patterns in both match statements and expressions. Add type-error coverage for
inconsistent binding sets and incompatible binding types, plus parse-error
coverage for a missing right-hand pattern. Verify IR, bytecode artifacts, and
Rust VM output.
