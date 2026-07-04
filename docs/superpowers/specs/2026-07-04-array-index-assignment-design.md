# Array Index Assignment Design

## Goal

Add mutable array element assignment as Phase 10B:

```cd
let xs = [1, 2, 3];
xs[1] = 42;
print xs[1]; // 42
```

Index assignment updates an existing array element and evaluates to the assigned value:

```cd
print xs[0] = 99; // 99
```

The feature must work in both runtime paths: the register IR interpreter (`--run`) and bytecode VM (`--run-bytecode`).

## Scope

This phase implements assignment to array index expressions only.

Supported assignment target forms after this phase:

```cd
name = value;
array[index] = value;
```

This phase does not implement:

- `push` or `pop`;
- array element static types;
- compound assignment such as `xs[i] += 1`;
- assigning into strings;
- slice assignment;
- record or field assignment.

## Syntax and Parsing

No new tokens are needed. The existing `=`, `[`, and `]` tokens are reused.

Update assignment parsing to allow an already-parsed index expression as an assignment target:

```ebnf
assignment = logicalOr, [ "=", assignment ] ;
```

Semantic target validation happens after parsing the left expression:

- if the left expression is `VariableExpr`, produce the existing `AssignExpr`;
- if the left expression is `IndexExpr`, produce a new `IndexAssignExpr`;
- otherwise, report parse error `invalid assignment target`.

This preserves existing right-associative assignment behavior:

```cd
xs[0] = xs[1] = 5;
```

The parser should continue to reject invalid targets such as:

```cd
(1 + 2) = 3;
foo() = 4;
```

## AST Design

Add a new expression node:

```cpp
struct IndexAssignExpr final : Expr {
    IndexAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
    ExprPtr value;
};
```

Fields:

- `collection`: expression producing the array value;
- `bracket`: the `[` token from the index expression, used for diagnostics;
- `index`: expression producing the index value;
- `value`: expression producing the assigned value.

AST print format:

```text
(= (index xs 0) 42)
```

This keeps assignment display visually aligned with variable assignment:

```text
(= x 42)
```

## Static Type Checking

Type checking rules for:

```cd
collection[index] = value
```

1. Check `collection`, `index`, and `value` expressions.
2. If `collection` has a known type and it is not `array`, report:

   ```text
   Type error at <bracket>: can only assign array elements
   ```

3. If `index` has a known type and it is not `number`, report:

   ```text
   Type error at <bracket>: array index must be number
   ```

4. Do not check the assigned value against an element type in this phase. Arrays remain mixed-element runtime values.
5. The assignment expression type is the assigned value type.

Unknown `collection` and `index` types are allowed and checked at runtime.

## Runtime Semantics

Runtime behavior for index assignment mirrors existing array indexing checks, then mutates the element.

Steps:

1. Evaluate `collection`.
2. Evaluate `index`.
3. Evaluate `value`.
4. Validate `collection` is an array; otherwise runtime error:

   ```text
   Runtime error: can only assign array elements
   ```

5. Validate `index` is a number; otherwise runtime error:

   ```text
   Runtime error: array index must be number
   ```

6. Validate `index` is an integer; otherwise runtime error:

   ```text
   Runtime error: array index must be integer
   ```

7. Validate `index` is in range; otherwise runtime error:

   ```text
   Runtime error: array index out of range
   ```

8. Replace the existing array element.
9. Return the assigned value.

## Array Mutability Model

Arrays become mutable reference values. The current runtime representation already stores array elements behind shared ownership, so mutating through one variable is visible through aliases:

```cd
let xs = [1];
let ys = xs;
ys[0] = 9;
print xs[0]; // 9
```

This phase intentionally does not add copy-on-write behavior.

Nested arrays follow the same reference semantics:

```cd
let inner = [1];
let outer = [inner];
outer[0][0] = 7;
print inner[0]; // 7
```

## IR Design

Add a new IR operation:

```cpp
IROp::AssignIndex
```

Instruction shape:

- `dest`: result register;
- `left`: collection register;
- `right`: index register;
- `arguments[0]`: assigned value register;
- `operand`: unused.

Printed IR format:

```text
v3 = assign_index v0, v1, v2
```

Where:

- `v0` is the collection;
- `v1` is the index;
- `v2` is the value;
- `v3` receives the assigned value.

The IR compiler lowers `IndexAssignExpr` by compiling collection, index, and value in source order, then emitting `AssignIndex`.

## IR Interpreter Design

`IRInterpreter` executes `IROp::AssignIndex` by reusing the same validation rules as `executeIndex`, then mutating the shared array element vector.

The interpreter returns the assigned value in the destination register.

Mutation should copy the assigned `Value` into the array vector. This preserves normal value behavior while sharing arrays through their existing shared element storage.

## Bytecode Design

Add a new bytecode operation:

```cpp
BytecodeOp::AssignIndex
```

The bytecode instruction shape mirrors IR:

- `dest`: result register;
- `left`: collection register;
- `right`: index register;
- `arguments[0]`: assigned value register.

`BytecodeCompiler` lowers `IROp::AssignIndex` to `BytecodeOp::AssignIndex`.

`BytecodeVM` executes the operation with the same validation and mutation behavior as the IR interpreter.

`--run` and `--run-bytecode` must match for successful programs and runtime errors.

## Parser, AST, and Lowering Data Flow

Example:

```cd
xs[1] = 42;
```

Flow:

1. Parser parses `xs[1]` as `IndexExpr`.
2. Parser sees `=` and converts that `IndexExpr` into `IndexAssignExpr` with value expression `42`.
3. TypeChecker validates collection/index types and returns the value expression type.
4. IRCompiler emits `AssignIndex`.
5. BytecodeCompiler lowers `AssignIndex`.
6. Runtime mutates the array element and returns the value.

## Diagnostics

Parse diagnostics:

```text
Parse error at <line>:<column>: invalid assignment target
```

Type diagnostics:

```text
Type error at <line>:<column>: can only assign array elements
Type error at <line>:<column>: array index must be number
```

Runtime diagnostics:

```text
Runtime error: can only assign array elements
Runtime error: array index must be number
Runtime error: array index must be integer
Runtime error: array index out of range
```

Use the index expression's `[` token location for type diagnostics. Runtime diagnostics remain locationless.

## Testing Strategy

Add success coverage with both runtime paths:

```cd
let xs = [1, 2, 3];
xs[1] = 42;
print xs[1];

let ys = xs;
ys[0] = 9;
print xs[0];

print xs[2] = 7;
```

Expected output:

```text
42
9
7
```

Add nested mutation success coverage:

```cd
let inner = [1];
let outer = [inner];
outer[0][0] = 7;
print inner[0];
```

Expected output:

```text
7
```

Add type-error coverage:

```cd
let x = 1;
x[0] = 2;
```

```cd
let xs = [1];
xs["0"] = 2;
```

Add runtime-error coverage for dynamically unknown invalid cases:

```cd
fun id(x) { return x; }
id(1)[0] = 2;
```

```cd
fun id(x) { return x; }
let xs = [1];
xs[id("0")] = 2;
```

```cd
let xs = [1];
xs[0.5] = 2;
```

```cd
let xs = [1];
xs[9] = 2;
```

Add parse-error coverage for invalid targets if existing invalid assignment fixtures do not already cover these shapes:

```cd
(1 + 2) = 3;
foo() = 4;
```

## Documentation Updates

Update:

- `README.md` to describe `array[index] = value`, assignment expression result, and array reference mutation behavior;
- `docs/language-grammar.ebnf` because assignment targets change in implemented grammar;
- `docs/roadmap.md` to mark Phase 10B implemented after code lands;
- `AGENTS.md` current language semantics to mention mutable array element assignment.

## Non-Goals

This phase does not implement:

- `push` or `pop`;
- changing array length;
- array element static types;
- compound assignment;
- string mutation;
- record or field mutation;
- copy-on-write arrays.

## Risks and Mitigations

Risk: adding assignment targets could destabilize existing assignment parsing.

Mitigation: keep the parser structure as parse-expression-then-validate-target, and add parse-error fixtures for invalid targets.

Risk: IR and bytecode runtime checks could diverge.

Mitigation: implement the same validation sequence and add both `run.out` and `run_bytecode.out`, plus bytecode runtime-error expectations where the runner supports them.

Risk: mutable reference arrays may surprise users expecting value copies.

Mitigation: document aliasing behavior explicitly in README and tests.

Risk: `arguments[0]` in `AssignIndex` is less self-documenting than a dedicated field.

Mitigation: keep it localized to this operation and document the shape in `IR.hpp` comments or the implementation plan. Avoid larger IR restructuring in this phase.
