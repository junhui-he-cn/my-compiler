# Algebraic Data Types and Pattern Matching Design

## Status

Proposed for Phase 18. This document defines the first executable enum and
pattern-matching slice; it deliberately does not describe a general object,
trait, or protocol system.

## Goals

- Define named enum types with a finite set of named variants.
- Allow each variant to carry zero or more positional payload values.
- Construct variants with qualified calls such as `Result.Ok(42)`.
- Match enum values with an exhaustive statement:

  ```cd
  match result {
    Result.Ok(value) => { print value; }
    Result.Err(message) => { print message; }
  }
  ```

- Bind payloads in an arm-local scope and support nested patterns.
- Preserve the behavior through C++ execution, register IR, `.cdbc`, and the
  Rust VM.
- Permit recursive enum payload types, so recursive data such as lists and
  trees can be represented without enabling recursive structs.

## Syntax

Enum declarations use positional type payloads. A trailing comma is allowed.

```cd
enum Result {
  Ok(number),
  Err(string),
  Empty,
}

let result = Result.Ok(7);
```

The constructor must use the enum name and variant name. Unit variants are
constructed with an empty call, for example `Result.Empty()`.

Patterns are wildcard `_`, a binding identifier, a literal (`nil`, boolean,
number, or string), or a qualified variant pattern with recursively nested
patterns:

```cd
enum List {
  Nil,
  Cons(number, List),
}

fun sum(list: List): number {
  match list {
    List.Nil => { return 0; }
    List.Cons(head, tail) => { return head + sum(tail); }
  }
}
```

Each arm has a `=>` followed by a block. Match is a statement, not an
expression in this phase.

## Static semantics

- Enum names share the existing named-type namespace with structs. Duplicate
  enum names and duplicate variant names within one enum are errors.
- A variant constructor checks its arity and each payload against the declared
  positional type. Its result has the named enum type.
- Recursive enum references are legal after all enum declarations in the
  current scope have been predeclared. Recursive enum values are immutable
  value objects; recursive structs remain rejected.
- A match scrutinee must have a known enum type. Every arm must be compatible
  with that enum. A qualified variant must name the scrutinee enum; an
  unqualified variant pattern is not part of this slice.
- Variant payload patterns are checked recursively. A binding pattern declares
  a new variable whose type is the matched value or payload. `_` binds nothing.
- A match is exhaustive when it covers every variant, or contains `_` or a
  binding pattern at the top level. Multiple arms may cover the same variant
  so nested payload patterns can be tried in source order.
- Literals are only valid for a future nullable/non-enum extension and are
  rejected for enum scrutinees in this first slice. They are nevertheless
  parsed as patterns so the grammar has a stable extension point.
- Match arm bindings are visible only in that arm's block. Branches execute in
  source order; only the first matching arm runs.

## Runtime representation

An enum value stores its fully qualified enum name, variant name, and an
immutable ordered vector of payload values. Equality is structural for enum
values: enum name, variant name, payload count, and payload values must all
match. Formatting is `Enum.Variant` for unit variants and
`Enum.Variant(value, ...)` for payload variants. `typeOf` reports the enum
name.

The compiler introduces three pipeline operations:

- `variant`: construct an enum value;
- `variant_tag`: produce a boolean for an enum/variant pair;
- `variant_field`: read a positional payload after a successful tag check.

Pattern matching lowers to tag checks, field reads, conditional jumps, arm
bindings, and the existing block code-generation path. The bytecode artifact
uses the same operations; no backend-specific matching semantics are added.

## Deliberate non-goals

- Match expressions, `case` syntax, guards, or fall-through arms.
- Unqualified constructor calls or unqualified variant patterns.
- Named payload fields, record patterns, rest patterns, or OR patterns.
- Generic enums and explicit type arguments.
- Nullable enum patterns and exhaustiveness across `nil` in this phase.
- Mutating enum payloads, inheritance, traits, protocols, or dynamic dispatch.
