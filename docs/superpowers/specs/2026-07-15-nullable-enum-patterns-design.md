# Nullable Enum Patterns Design

## Scope

Allow statement- and expression-level `match` to inspect a nullable enum value.
For an enum `Option`, a value of type `Option?` may be matched with `nil` or
with the existing `Option.Variant(...)` patterns. Existing wildcard, binding,
nested, named-payload, and guarded patterns keep their current behavior.

The slice does not add generic enums, OR patterns, or new bytecode operations.

## Semantics

- `nil` is a valid literal pattern when the expected pattern type is nullable.
- A nullable enum pattern unwraps only for variant validation; variant payloads
  keep their declared types.
- An unguarded `nil` arm covers the nullable case. Each unguarded enum variant
  arm covers its variant. An unguarded wildcard or binding covers the complete
  nullable domain.
- Guarded arms are checked in their arm-local binding scope but contribute no
  coverage, including `nil if condition`.
- A nullable enum match is exhaustive only when both `nil` and every enum
  variant are covered, or when an unguarded wildcard/binding covers everything.
- Non-`nil` literal patterns remain unsupported. Matching a non-nullable enum
  with `nil` remains a type error.

## Implementation

The type checker tracks the existing variant coverage set plus one nullable
coverage bit. It unwraps `T?` before checking a variant pattern. The IR already
lowers literal patterns through `Equal`, so `nil` matching uses the existing
constant/equality path; variant checks continue to use `variant_tag` and
`variant_field`. Bytecode text and Rust VM behavior therefore remain unchanged.

## Verification

Add focused goldens for statement and expression matches, guards, imported
nullable enum types, non-exhaustive `nil`/variant cases, and Rust VM/artifact
parity. Update the language grammar, README, AGENTS semantics, and roadmap.
