# Basic Type Checker Design

## Goal

Implement Phase 2 from `docs/roadmap.md`: add a basic static type-checking phase that gives explicit `let` type annotations semantic meaning while preserving existing unannotated programs.

The checker should catch statically knowable type mistakes before AST printing, IR generation, or IR execution.

## Current State

The parser accepts type annotations on `let` declarations:

```cd
let name: type = expression;
```

Those annotations are currently syntax-only. They are printed in the AST but are not validated or enforced. Runtime values currently support `nil`, numbers, booleans, and strings.

## Chosen Approach

Add a new type-checking pass after parsing and before all downstream modes:

```text
Lexer -> Parser -> TypeChecker -> AST print / IR compile / IR run
```

New files:

- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp`

The checker walks the AST, maintains a lexical scope stack matching runtime block scope, and validates explicit type annotations and statically knowable operator constraints.

## Supported Static Types

The initial checker supports these type names:

- `number`
- `bool`
- `string`
- `nil`

It also uses an internal `unknown` type for expressions whose static type is intentionally not tracked in this phase.

Unknown type behavior:

- Unannotated declarations produce `unknown` bindings.
- Unknown can be assigned to any explicit type without error.
- Unknown operands suppress operator-type errors when the checker cannot statically prove a mismatch.
- Operators with fixed boolean result, such as equality and comparisons, still produce `bool` when valid.

This keeps Phase 2 focused on explicit annotations and avoids breaking existing unannotated examples.

## Type-Checked Declarations

A `let` declaration with a known annotation checks its initializer when the initializer has a known type:

```cd
let x: number = 1;      // ok
let y: string = "hi";  // ok
let z: bool = true;     // ok
let n: nil = nil;       // ok
let x: number = "hi";  // type error
```

A `let` declaration without an annotation is allowed and records the variable as `unknown`:

```cd
let x = 1;      // ok, x has unknown static type in this phase
x = "later";   // ok in this phase
```

Unknown type names are type errors:

```cd
let x: int = 1; // type error: unknown type `int`
```

## Assignment Checking

Assignment searches lexical scopes from innermost to outermost, matching runtime behavior.

If the target variable has an explicit known type, the assigned value is checked when the assigned expression has a known type:

```cd
let x: number = 1;
x = 2;      // ok
x = "bad"; // type error
```

If the target variable is unknown, assignment is allowed:

```cd
let x = 1;
x = "ok for now";
```

Assignment to a statically unknown variable name remains outside this phase's strict checking. The existing runtime undefined-variable error can continue to handle it. This avoids introducing a full resolver before the project is ready for one.

## Duplicate Declaration Checking

The checker reports same-scope duplicate declarations before runtime:

```cd
let x = 1;
let x = 2; // type error
```

Inner scopes may shadow outer declarations:

```cd
let x: number = 1;
{
  let x: string = "inner"; // ok
}
```

## Expression Type Rules

### Literals and Variables

- number literal -> `number`
- string literal -> `string`
- `true` / `false` -> `bool`
- `nil` -> `nil`
- variable -> nearest binding type if found, otherwise `unknown`
- grouping -> grouped expression type
- assignment -> target variable type if known, otherwise assigned expression type

### Unary Operators

- `-expr` requires `expr` to be `number` when known, result `number`.
- `!expr` accepts any type, result `bool`, matching runtime truthiness.

### Binary Operators

- `+` accepts `number + number` -> `number`.
- `+` accepts `string + string` -> `string`.
- `+` with `unknown` suppresses static mismatch and returns `unknown`.
- `-`, `*`, `/` require `number + number` when both operands are known, result `number`.
- `<`, `<=`, `>`, `>=` require `number + number` when both operands are known, result `bool`.
- `==`, `!=` accept any operand types, result `bool`.

If both operands are known and an operator does not support their combination, the checker reports a type error.

## Error Model

Add `TypeError` as a `std::runtime_error` subclass with messages prefixed like:

```text
Type error: ...
```

The top-level CLI already catches `std::exception`, prints `error.what()` to stderr, and exits with code 1. Type errors should therefore use exit code 1.

Examples of stable message shapes:

```text
Type error: unknown type `int`
Type error: cannot initialize `x` of type number with string
Type error: cannot assign string to `x` of type number
Type error: variable `x` already declared in this scope
Type error: unary `-` expects number, got string
Type error: binary `*` expects numbers, got number and string
Type error: binary `+` expects two numbers or two strings, got number and bool
```

## Golden Test Runner Changes

Add a new fixture category:

```text
tests/golden/type_errors/
```

Fixture shape:

```text
tests/golden/type_errors/<case>.cd
tests/golden/type_errors/<case>.err
tests/golden/type_errors/<case>.exit
```

The runner should execute type-error fixtures in default AST mode, expect no stdout, and compare stderr and exit code. This mirrors parse-error fixture handling but represents semantically valid syntax that fails static type checking.

Add selftests to `tests/run_golden_tests_selftest.py` proving:

- type-error fixtures are discovered and checked
- unexpected stdout fails
- missing `.err` or `.exit` files fail through existing checker behavior, if practical
- `tests/golden/type_errors/input.cd` is not mistaken for a success fixture

## Initial Type Error Fixtures

Add fixtures for:

1. `typed_let_number_mismatch.cd`
   ```cd
   let x: number = "hello";
   ```

2. `typed_assignment_mismatch.cd`
   ```cd
   let x: number = 1;
   x = "hello";
   ```

3. `unknown_type_annotation.cd`
   ```cd
   let x: int = 1;
   ```

4. `duplicate_declaration.cd`
   ```cd
   let x = 1;
   let x = 2;
   ```

5. `unary_minus_non_number.cd`
   ```cd
   let x: number = -"hello";
   ```

6. `binary_number_operator_mismatch.cd`
   ```cd
   let x: number = 1 * "hello";
   ```

Keep the existing runtime duplicate-declaration fixture until implementation decisions prove it should move. If TypeChecker now catches that program earlier, update or relocate the fixture intentionally as part of implementation.

## Documentation Updates

Update `README.md`:

- Type annotations are no longer syntax-only.
- Supported annotation names are `number`, `bool`, `string`, and `nil`.
- Unannotated variables are still accepted and not fully inferred in this phase.
- Type-error fixtures live under `tests/golden/type_errors`.

Update `AGENTS.md`:

- Remove the statement that annotations are syntax-only.
- Document the limited explicit-annotation checker and `type_errors` fixture category.

Update `docs/roadmap.md`:

- Mark Phase 2 implemented after this phase is complete.

`docs/language-grammar.ebnf` does not need structural grammar changes. A comment may be updated only if useful.

## Non-Goals

- Do not infer types for unannotated variables in this phase.
- Do not add a full resolver.
- Do not reject all uses of undefined variables statically.
- Do not add function types, arrays, structs, or user-defined types.
- Do not change expression grammar.
- Do not change runtime `Value` representation.
- Do not change CLI flags.

## Success Criteria

- The CLI runs TypeChecker after parsing for default, `--ir`, and `--run` modes.
- Valid explicitly typed declarations pass.
- Invalid explicitly typed initializers fail before AST/IR/run output.
- Invalid assignments to explicitly typed variables fail before IR execution.
- Same-scope duplicate declarations fail before IR execution.
- Inner-scope shadowing remains valid.
- Existing unannotated golden programs continue to pass unless intentionally converted to type-error fixtures.
- `tests/golden/type_errors` fixtures are supported by the golden runner.
- Full verification passes:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```
