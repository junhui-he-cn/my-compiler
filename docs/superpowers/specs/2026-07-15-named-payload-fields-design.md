# Named Enum Payload Fields Design

## Goal

Add names to enum payload declarations and allow match patterns to select
payloads by name while preserving the existing positional runtime layout.

```cd
enum Result {
  Ok(value: number),
  Err(message: string),
  Empty,
}

let result = Result.Ok(7);
let label = match result {
  Result.Ok(value: numberValue) => "ok:" + str(numberValue),
  Result.Err(message: text) => "err:" + text,
  Result.Empty => "empty",
};
```

## Scope and syntax

- An enum payload is either positional (`number`) or named
  (`value: number`). A variant must use one style consistently, and named
  payload names must be unique.
- Variant constructors remain positional (`Result.Ok(7)`). Their argument
  count and types are unchanged; names are static metadata for patterns and
  interfaces, not a new runtime object shape.
- A variant pattern may remain positional (`Result.Ok(value)`) or use named
  arguments (`Result.Ok(value: numberValue)`). Named pattern arguments may be
  written in any order, must name every argument, and must refer to every
  declared payload exactly once.
- A named pattern is rejected for a positional variant. Named and positional
  pattern arguments cannot be mixed within one variant pattern.
- Nested variant patterns, match expressions, guards, imported enums, and
  re-exported enums keep their existing behavior.
- Named payloads do not add general field access, mutation, record patterns,
  rest patterns, OR patterns, generic enums, or nullable enum patterns.

## Static checking and interfaces

- Enum declarations carry aligned payload-name metadata beside payload types.
- Pattern checking resolves each named argument to its positional payload index,
  validates names/duplicates/completeness, and checks the nested pattern
  against the resolved payload type.
- Resolved positional indexes are recorded for IR lowering, so a reordered
  named pattern still reads the correct runtime payload.
- Module symbols and textual module interfaces preserve payload names, allowing
  direct, namespace, and re-exported enum patterns to check consistently.

## Runtime and backend

No new runtime representation, IR opcode, bytecode opcode, or Rust VM behavior
is needed. The IR compiler uses the resolved payload index when emitting the
existing `variant_field` operation. Constructors and `.cdbc` variant values
remain positional.

## Verification

Cover local declarations, reordered and nested named patterns, positional
backward compatibility, imported enum metadata, module-interface output, AST/
IR/bytecode output, Rust VM parity, unknown/duplicate/mixed field errors, and
named patterns on positional variants.
