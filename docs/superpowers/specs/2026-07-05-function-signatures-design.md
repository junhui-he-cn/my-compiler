# Phase 9D Function Signatures Design

## Goal

Add optional parameter type annotations and optional return type annotations for named functions and function expressions.

This phase strengthens the existing function-value type checks without introducing first-class function type annotation syntax yet.

Example:

```cd
fun add(x: number, y: number): number {
  return x + y;
}

let inc = fun (x: number): number {
  return x + 1;
};

print add(2, inc(3));
```

Expected output:

```text
6
```

## Scope

This phase includes:

- Parameter annotations on named function declarations.
- Parameter annotations on anonymous function expressions.
- Return annotations on named function declarations.
- Return annotations on anonymous function expressions.
- Static checking of annotated parameter use inside function bodies.
- Static checking of explicit and implicit returns against annotated return types.
- Propagation of annotated return types through known function values and call-result inference.
- Documentation and golden tests.

This phase does not include:

- First-class function type annotations for variables, parameters, arrays, or fields.
- Function overloads.
- Generic functions.
- Union types.
- Type aliases.
- Control-flow-complete return analysis beyond a conservative implicit-`nil` rule.
- Runtime enforcement for statically known function annotations.

## User-Visible Syntax

### Named functions

Current syntax remains valid:

```cd
fun id(x) {
  return x;
}
```

New syntax allows optional parameter and return annotations:

```cd
fun add(x: number, y: number): number {
  return x + y;
}
```

### Function expressions

Current syntax remains valid:

```cd
let f = fun (x) {
  return x;
};
```

New syntax allows optional parameter and return annotations:

```cd
let f = fun (x: number): number {
  return x + 1;
};
```

### Supported annotation names

Use the same annotation names as current `let` annotations in this phase:

- `number`
- `bool`
- `string`
- `nil`

The internal static types `array` and `function` are not exposed as annotation names in this phase. That keeps the public type grammar aligned with the existing `let` annotation behavior and avoids committing to function/array type syntax before Phase 9's later slices.

## Grammar

Update function declarations and expressions to use typed parameter lists and optional return annotations:

```ebnf
funDecl      = "fun", identifier,
               "(", [ parameters ], ")",
               [ ":", typeName ],
               block ;

functionExpr = "fun", "(", [ parameters ], ")",
               [ ":", typeName ],
               block ;

parameters   = parameter,
               { ",", parameter } ;

parameter    = identifier,
               [ ":", typeName ] ;
```

`typeName` remains an identifier parsed by the parser and validated by the type checker.

## AST Shape and Printing

Replace raw `std::vector<Token>` parameters with a parameter structure shared by function declarations and function expressions:

```cpp
struct Parameter {
    Token name;
    std::optional<Token> typeName;
};
```

Update:

- `FunctionStmt` to store `std::vector<Parameter> parameters` and `std::optional<Token> returnTypeName`.
- `FunctionExpr` to store `std::vector<Parameter> parameters` and `std::optional<Token> returnTypeName`.

AST printing should include annotations only when present:

```text
Fun add(x: number, y: number): number
```

Inline function expression printing should mirror the same parameter and return annotation style:

```text
(fun (x: number): number (return (+ x 1)))
```

Exact indentation remains consistent with the current AST printer.

## Type Checking

### Parameter bindings

When checking a function body:

- If a parameter has an annotation, resolve it with the same rules used by `let` annotations.
- Declare the parameter binding with that static type.
- If a parameter has no annotation, declare it as `StaticType::Unknown`, preserving current behavior.

This enables existing expression checks inside the function body:

```cd
fun bad(x: number) {
  print x + "s";
}
```

This should be a type error because `x` is statically known as `number`.

### Return annotations

When a function has a return annotation:

- Resolve the annotation to an expected `StaticType`.
- Every explicit `return expression;` must be compatible with the expected type.
- `return;` is treated as returning `nil` and must be compatible with the expected type.
- Reaching the end of a function is treated as an implicit `nil` return and must be compatible with the expected type.

Examples:

```cd
fun ok(): nil {
  return;
}
```

```cd
fun bad(): number {
}
```

The second example should report a type error because the function may implicitly return `nil`.

### Return inference

When a return annotation exists, the function's known return type is the annotated type after successful checking.

When no return annotation exists, keep current conservative return inference:

- No explicit return means inferred `nil`.
- Same known return types merge to that type.
- Conflicting or unknown return types merge to `unknown`.

### Calls

Known function values already carry arity and conservative return type. This phase updates those known return types to use annotated return types when present, so call-result inference continues to work:

```cd
fun value(): number {
  return 42;
}

let x = value();
x = "bad"; // type error because x inferred number
```

### Assignment compatibility for functions

Keep the existing same-arity function assignment checks. Do not add full function signature compatibility in this phase because there is no first-class function type syntax yet.

If a known function variable is reassigned another known function with the same arity but a different return type, preserve the existing behavior of updating the variable's known return type from the assigned function value. Parameter annotation compatibility between function values is out of scope.

## Diagnostics

Use existing `Type error at <line>:<column>: ...` diagnostics.

Suggested stable messages:

- Unknown annotation:

```text
Type error at 1:12: unknown type `foo`
```

- Return mismatch:

```text
Type error at 2:10: cannot return string from function returning number
```

- Implicit return mismatch:

```text
Type error at 1:1: function `value` may return nil but is annotated number
```

- `return;` mismatch:

```text
Type error at 2:3: cannot return nil from function returning number
```

Exact wording may vary, but it should be stable and covered by type-error goldens.

## IR, Bytecode, and Runtime

No IR opcode, bytecode opcode, `.cdbc` format, IR interpreter, or Rust VM runtime behavior changes are required.

Function signatures are compile-time metadata only. Lowering still uses the existing function table and parameter name lists. Resolved parameter names remain recorded by `ResolvedNames` so closures, functions, and function expressions keep their current runtime behavior.

## Tests

### Success fixtures

Add fixtures such as:

1. `function_typed_params_success`
   - A named function with `number` parameters uses arithmetic and returns/prints the expected result.
   - Include `ast.out`, `run.out`, and optionally `ir.out` if concise.

2. `function_typed_return_inference`
   - A named function annotated `: number` feeds a call result into unannotated `let` inference.
   - Reassignment to the same type succeeds.

3. `lambda_typed_signature`
   - A function expression with typed parameter and typed return works through a variable call.

### Type-error fixtures

Add fixtures such as:

1. `function_param_type_mismatch.cd`
   - `x: number` used as a string or added to a string.

2. `function_return_type_mismatch.cd`
   - `: number` function returns a string.

3. `function_return_nil_mismatch.cd`
   - `: number` function uses `return;`.

4. `function_missing_return_mismatch.cd`
   - `: number` function can fall off the end.

5. `lambda_return_type_mismatch.cd`
   - `fun (): bool { return 1; }` or similar mismatch.

6. `function_unknown_parameter_type.cd`
   - Parameter annotation uses an unknown type name.

### Parse-error fixtures

Add parse-error coverage only for new syntax boundary cases that are easy to misparse:

- Missing parameter type after `:`.
- Missing return type after `):`.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` for typed parameters and return annotations.
- `README.md` function syntax and type-checking description.
- `docs/roadmap.md` to mark Phase 9D implemented after implementation lands.
- `AGENTS.md` current language semantics and limitation notes.

## Acceptance Criteria

- Existing untyped named functions and function expressions continue to parse and run.
- Named function parameters may be annotated with supported type names.
- Function expression parameters may be annotated with supported type names.
- Named functions may have return annotations.
- Function expressions may have return annotations.
- Annotated parameter bindings affect static checks in function bodies.
- Annotated return types check explicit returns, `return;`, and implicit fallthrough.
- Annotated return types propagate through known function calls into existing `let` inference and assignment checks.
- No IR/bytecode/Rust VM format or runtime semantics change is required.
