# Array Element Types Design

## Overview

Add first-class static array element types to the existing type annotation system using `[T]` syntax. This is Phase 9F of the richer type system roadmap. The feature improves static checking for typed arrays while preserving the current runtime array representation and dynamic escape hatches.

Example:

```cd
let xs: [number] = [1, 2, 3];
fun first(values: [number]): number {
  return values[0];
}
```

## Goals

- Allow array type annotations in every place that currently accepts a type annotation: `let` declarations, function parameters, function returns, struct fields, and nested function type annotations.
- Statically check elements of explicitly typed array literals.
- Propagate known element types through array indexing, index assignment, `push`, and `pop`.
- Support nested arrays and arrays of existing supported types, including functions and named structs.
- Preserve existing behavior for untyped mixed arrays and values whose element type is unknown.

## Non-Goals

- Do not add a general generic type system or `array<T>` syntax.
- Do not change runtime `Value` array storage; arrays remain mutable reference values and can still hold mixed runtime values when static type information is unknown.
- Do not add nullable syntax or special `nil` compatibility rules beyond the current compatibility behavior.
- Do not add method syntax such as `xs.push(value)`.
- Do not require full inference for empty unannotated arrays.

## Syntax

Extend `typeExpr` with array types:

```ebnf
typeExpr  = arrayType | functionType | simpleType ;
arrayType = "[", typeExpr, "]" ;
```

Examples:

```cd
let numbers: [number] = [1, 2, 3];
let matrix: [[number]] = [[1], [2]];
let callbacks: [fun(number): number] = [fun (x: number): number { return x; }];
fun make(): [string] { return ["a", "b"]; }
struct Bag { values: [number] }
```

The AST printer should emit array type annotations as `[<type>]`, matching source syntax and existing compact annotation printing.

## Static Type Model

`TypeAnnotation` gains an `Array` kind with one child annotation. `TypeInfo` gains an optional element type for `StaticType::Array`.

Array type names should format recursively:

- Unknown element type: `array`
- Known number array: `[number]`
- Nested array: `[[number]]`
- Function element array: `[fun(number): string]`

Compatibility rules:

- Unknown expected or actual types remain compatible.
- Array without known element type remains compatible with any array.
- Array with known element type is compatible with another known-element array only when element types are compatible recursively.
- Existing struct and function compatibility rules continue to apply inside array element types.

## Checking Behavior

### Array Literals

- When an array literal is checked without contextual expected type, infer a known element type only if the literal is non-empty and all elements have the same compatible known type.
- Mixed known element types infer plain `array` with unknown element type rather than producing an error.
- Empty unannotated array literals infer plain `array` with unknown element type.
- When an array literal is checked in a context that expects `[T]` (for example a typed `let`, assignment to a typed binding, function argument, function return, or struct field initializer), each array literal element must be assignable to `T`. This contextual check is required so `[1, "x"]` cannot silently degrade to plain `array` when the destination explicitly requires `[number]`.

Examples:

```cd
let xs: [number] = [1, 2];      // ok
let ys: [number] = [];          // ok
let bad: [number] = [1, "x"];  // type error
let mixed = [1, "x"];          // ok, plain dynamic array
```

### Indexing

- Index target must still be `array` or unknown.
- Index expression must still be `number` or unknown.
- Indexing `[T]` returns `T`.
- Indexing plain `array` returns unknown.

Example:

```cd
let xs: [number] = [1];
let n: number = xs[0]; // ok
let s: string = xs[0]; // type error
```

### Index Assignment

- Target and index checks remain unchanged.
- If target element type is known, assigned value must be compatible with it.
- The assignment expression continues to evaluate to the assigned value's type, matching current expression semantics.

Example:

```cd
let xs: [number] = [1];
xs[0] = 2;      // ok
xs[0] = "bad";  // type error
```

### Native Collection Builtins

`push(array, value)`:

- First argument must still be array or unknown.
- If the first argument has known element type `T`, the second argument must be compatible with `T`.
- Return type remains `nil`.

`pop(array)`:

- First argument must still be array or unknown.
- If the first argument has known element type `T`, return `T`.
- If the first argument is a plain array or unknown, return unknown.

`len(value)` is unchanged and continues to return `number` for arrays and strings.

## Runtime and Backend Impact

This feature is primarily static. Runtime array representation, IR operations, bytecode lowering, `.cdbc` formatting, and Rust VM execution do not need semantic changes.

Existing runtime paths should continue to execute typed-array programs because type annotations are erased before IR execution. Backend parity tests should include representative typed-array source programs to confirm no accidental runtime divergence.

## Diagnostics

Use existing type diagnostic conventions. Suggested messages:

- `array element expects number, got string`
- `array index assignment expects number, got string`
- `push value expects number, got string`
- Existing call/assignment diagnostics may be reused where they already identify the expected and actual type clearly.

Located diagnostics should point at the most actionable token:

- Array literal mismatch: offending element token if available, otherwise the array bracket.
- Index assignment mismatch: assignment bracket or value token, consistent with nearby checker style.
- `push` mismatch: call paren or offending argument token, consistent with existing native stdlib diagnostics.

## Implementation Touch Points

- `include/Ast.hpp`, `src/Ast.cpp`: add array `TypeAnnotation` node shape and printer support.
- `include/Parser.hpp`, `src/Parser.cpp`: parse `[` typeExpr `]` in type annotations.
- `docs/language-grammar.ebnf`: document array type grammar.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add array element type to `TypeInfo`, recursive formatting and compatibility, array literal inference/checking, index result typing, index assignment checks, and `push`/`pop` typing.
- `README.md`: document `[T]` annotations and the dynamic escape hatch for untyped mixed arrays.
- `tests/golden/`: add success and type-error fixtures.
- `tests/run_rust_vm_tests.py` or existing parity fixtures: include at least one typed-array program that exercises runtime execution through the Rust VM path.

## Testing Plan

Add or update golden coverage for:

- Successful typed numeric array declaration and indexing.
- Successful empty typed array declaration.
- Nested array type annotation AST output.
- Function parameter and return annotations using `[T]`.
- Array literal element mismatch in an explicitly typed array.
- Index assignment mismatch on `[T]`.
- `push` mismatch on `[T]`.
- `pop` result type feeding a typed `let` successfully and failing for the wrong type.
- Dynamic mixed unannotated arrays remaining accepted.

Before claiming implementation complete, run the repository's full verification set from `AGENTS.md`.

## Open Decisions Resolved

- Syntax is `[T]`, not `array<T>`.
- Untyped mixed arrays remain valid and dynamic.
- Runtime representation is unchanged.
- Empty unannotated arrays keep unknown element type.
