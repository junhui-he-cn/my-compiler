# Simple Let Type Inference Design

Date: 2026-07-04

## Goal

Implement Phase 9A: infer static types for unannotated `let` declarations from their initializer expressions, then reuse those inferred binding types for later assignment and expression checks.

This phase keeps the type system intentionally small. It strengthens the existing checker without introducing new user-facing type syntax, function return inference, parameter inference, or array element types.

## Motivation

The language already type-checks explicitly annotated declarations:

```cd
let x: number = 1;
x = "oops"; // Type error today
```

But unannotated variables are currently accepted without preserving the initializer type:

```cd
let x = 1;
x = "oops"; // Currently accepted because x is unknown
```

That makes unannotated code much less safe than annotated code. The next language roadmap recommends a small first type-system slice: infer simple binding types for unannotated `let` declarations. This immediately improves assignment checking and creates a foundation for future function signatures, array element types, records, and builtins.

## Scope

In scope:

- For `let name = initializer;`, store the initializer's known `StaticType` as the binding type.
- Preserve existing explicit annotation behavior for `let name: type = initializer;`.
- Use inferred binding types for later variable reads and assignment checks.
- Keep expressions whose type is not yet known as `StaticType::Unknown`.
- Add golden coverage for successful inferred assignments and type errors for mismatched inferred assignments.
- Update README and roadmap wording to reflect this implemented Phase 9A slice.
- Keep IR, bytecode, and runtime behavior unchanged except for programs rejected earlier by type checking.

Out of scope:

- Function parameter type inference.
- Function return type inference.
- Static return type checking.
- Function type annotation syntax.
- Array element type inference.
- Generic or union types.
- Nilability rules beyond existing `StaticType::Nil` behavior.
- Changing runtime `Value` representation.
- Changing IR or bytecode opcodes.

## User-Visible Semantics

### Unannotated `let` declarations

If the initializer has a known static type, the binding receives that type:

```cd
let n = 1;
n = 2;       // ok
n = "two";   // Type error

let s = "hello";
s = "bye";   // ok
s = 3;       // Type error

let b = true;
b = false;   // ok
b = nil;     // Type error

let xs = [1, "two"];
xs = [];     // ok, both are array values
xs = 1;      // Type error

let f = fun () {
  return 1;
};
f = fun () {
  return 2;
};          // ok, both are function values
f = 3;       // Type error
```

The checker should reuse the existing assignment diagnostic style:

```text
Type error at <line>:<column>: cannot assign <actual> to `<name>` of type <expected>
```

### Unknown initializer types

If the initializer type is `unknown`, the binding remains `unknown`:

```cd
fun id(x) {
  return x;
}

let value = id(1); // call result type remains unknown in this phase
value = "later";   // still accepted because value is unknown
```

This preserves current behavior for expressions that the checker cannot type precisely yet.

### Explicit annotations

Explicit annotations remain authoritative and keep the current behavior:

```cd
let n: number = 1; // ok
let bad: number = "oops"; // Type error
```

The implementation should not change annotation parsing or supported annotation names.

## Static Type Sources

The phase relies on the existing `checkExpression()` return values:

- Number literals and numeric arithmetic produce `number`.
- String literals and string concatenation produce `string`.
- Boolean literals, comparisons, equality, and `!` produce `bool` when the checker already returns `bool`.
- `nil` produces `nil`.
- Array literals produce `array` without element information.
- Named functions and function expressions produce `function`.
- Variable reads return the resolved binding type.
- Calls and indexing remain `unknown` unless future phases add return/element type information.
- Logical expressions keep existing `logicalResultType()` behavior.

No new static type enum values are required for this phase.

## Implementation Approach

The main implementation change is in `TypeChecker::checkLetInitializer()`.

Today, unannotated declarations discard the initializer type:

```cpp
if (!statement.typeName) {
    return StaticType::Unknown;
}
```

This phase changes that branch to return the initializer type:

```cpp
if (!statement.typeName) {
    return initializer;
}
```

The existing declaration path already records that returned type in the binding:

```cpp
const StaticType declared = checkLetInitializer(*let);
declareVariable(*let, declared);
```

Assignment checking already compares known target and value types. Returning the initializer type should therefore enable mismatch diagnostics without changing assignment logic.

## Testing Strategy

Add golden tests before implementation.

Success fixtures:

- `tests/golden/inferred_let_assignment/`
  - Demonstrates inferred `number`, `string`, `bool`, `array`, and `function` bindings accepting same-kind assignments.
  - Include `run.out` and `run_bytecode.out` to prove runtime behavior is unchanged.
  - Include `ast.out` only if useful for readability; no AST format changes are expected.

Type-error fixtures:

- `tests/golden/type_errors/inferred_let_number_assignment_mismatch.cd`
  - `let x = 1; x = "oops";`
- `tests/golden/type_errors/inferred_let_string_assignment_mismatch.cd`
  - `let s = "hi"; s = 2;`
- `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.cd`
  - `let b = true; b = nil;`
- `tests/golden/type_errors/inferred_let_array_assignment_mismatch.cd`
  - `let xs = []; xs = 1;`
- `tests/golden/type_errors/inferred_let_function_assignment_mismatch.cd`
  - `let f = fun () { return 1; }; f = 1;`

Unknown-preservation fixture:

- `tests/golden/inferred_let_unknown_call_result/`
  - A function call result assigned to an unannotated `let` remains unknown, so later assignment to another type is accepted in this phase.
  - Include `run.out` and `run_bytecode.out` to document compatibility.

Run the full project verification before completion:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

## Documentation Updates

Update user-visible docs after implementation:

- `README.md`
  - Change wording from “Unannotated variables are still accepted and are not fully inferred yet” to explain that unannotated `let` bindings infer known initializer types, while function returns, function parameters, and array element types are not fully inferred.
- `docs/roadmap.md`
  - Mark Phase 9A simple `let` inference as implemented or note it under Phase 9 progress.
- `AGENTS.md`
  - Update current language semantics to say unannotated `let` bindings infer known initializer types.

Do not update `docs/language-grammar.ebnf`; grammar does not change.

## Risks and Mitigations

- **Risk:** Existing unannotated programs that changed variable types will now fail type checking.
  - **Mitigation:** This is intentional for known initializer types. Unknown initializer types preserve current flexibility.

- **Risk:** Function calls still return `unknown`, which may surprise users expecting full inference.
  - **Mitigation:** Document that this phase only infers binding types from known initializer expressions and does not infer function return types.

- **Risk:** Array literals infer only `array`, not element type.
  - **Mitigation:** Keep array element type inference explicitly out of scope and reserve it for a future Phase 9 slice.

- **Risk:** `nil` inference may make `let x = nil; x = 1;` a type error.
  - **Mitigation:** This follows existing explicit `nil` annotation behavior. A future nilability design can relax or refine this.

## Success Criteria

- Unannotated `let` bindings with known initializer types reject mismatched later assignments.
- Unannotated `let` bindings with unknown initializer types keep current flexible behavior.
- Explicit type annotations behave exactly as before.
- No parser, grammar, IR, bytecode opcode, or runtime value representation changes are required.
- IR interpreter and bytecode VM outputs remain aligned for successful fixtures.
- README, roadmap, and AGENTS describe the new type inference boundary accurately.
