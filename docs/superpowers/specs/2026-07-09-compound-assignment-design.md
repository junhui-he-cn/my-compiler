# Variable Compound Assignment Design

## Goal

Add a focused first slice of compound assignment operators for variables:

```cd
x += 1;
x -= 1;
x *= 2;
x /= 2;
```

This phase is Phase 15D in the language roadmap. It improves everyday ergonomics without broadening assignment targets yet.

## User-Facing Semantics

Supported operators:

```text
+=
-=
*=
/=
```

Supported target in this slice:

```cd
name += expression
name -= expression
name *= expression
name /= expression
```

The target must be an existing variable binding. Compound assignment is an expression and evaluates to the assigned value, matching existing plain assignment behavior.

Examples:

```cd
let x = 1;
print x += 2; // 3
print x;      // 3

let y = 10;
y /= 2;
print y;      // 5
```

`x += y` is semantically equivalent to:

```cd
x = x + y
```

except the variable target is resolved once by the compiler. The right-hand side expression is evaluated once.

## Scope

In scope:

- Lexing `+=`, `-=`, `*=`, and `/=`.
- Parsing compound assignment at the same precedence and associativity level as plain assignment.
- Supporting variable targets only.
- Static checks for known non-number variable or value types.
- IR lowering to load variable, apply numeric binary operation, assign variable, and return assigned value.
- C++ `--run`, bytecode, and Rust VM parity through existing binary and assignment operations.
- Success, parse-error, type-error, runtime-error, docs, and golden coverage.

Out of scope:

- `array[index] += value`.
- `object.field += value`.
- String concatenation through `+=`.
- Logical compound operators.
- Increment/decrement operators such as `++` or `--`.
- Assignment-combination operators for arrays or structs.

## Type Rules

Compound assignment is numeric-only in this first slice.

Static rules:

- The variable must already be declared and visible.
- If the variable type is known and not `number`, type checking fails.
- If the right-hand side type is known and not `number`, type checking fails.
- The expression result type is `number`.
- The assigned variable keeps its existing static type.

Suggested diagnostics:

```text
Type error at 1:3: `+=` expects number variable, got string
Type error at 1:8: `+=` expects number value, got bool
Type error at 1:1: undefined variable `x`
```

For unknown types, runtime checks from existing numeric binary IR operations enforce valid values. For example:

```cd
fun id(x) { return x; }
let n = 1;
n += id("x");
```

should raise the existing runtime error for invalid `add` operands.

## Parsing and AST

Add token types:

```cpp
PlusEqual
MinusEqual
StarEqual
SlashEqual
```

The parser should recognize them in `assignment()` after parsing the left expression. Valid target for this slice is only `VariableExpr`.

Add AST node:

```cpp
struct CompoundAssignExpr final : Expr {
    CompoundAssignExpr(Token name, Token op, ExprPtr value);

    Token name;
    Token op;
    ExprPtr value;
};
```

Printer output should be stable and compact, for example:

```text
(+= x 1)
```

Invalid targets should produce parse errors similar to existing assignment target diagnostics:

```cd
(xs[0]) += 1; // parse error in this slice
person.age += 1; // parse error in this slice
```

Suggested message:

```text
Parse error at 1:8: invalid compound assignment target
```

## Type Checker Design

Extend `ResolvedNames` with compound assignment name tracking, parallel to plain `AssignExpr`:

```cpp
const std::string& compoundAssignmentName(const CompoundAssignExpr& expression) const;
void recordCompoundAssignment(const CompoundAssignExpr& expression, std::string name);
```

Type checking should:

1. Resolve the variable name through existing lexical scopes.
2. Report undefined variable using the variable token if not found.
3. Record the resolved name for IR lowering.
4. Check the right-hand side expression.
5. If the binding type is known and not number, report an operator-specific type error at the operator token.
6. If the right-hand side type is known and not number, report an operator-specific type error at the operator token or right-hand side stable diagnostic location.
7. Return `number`.

Shadowing and module/import rules follow existing variable resolution.

## IR Lowering Design

No new IR operation is needed.

Lower:

```cd
x += expr
```

to:

```text
vOld = load_var x#N
vRhs = <compile expr>
vNew = add vOld, vRhs
assign_var x#N, vNew
result = copy vNew
```

The expression result should be the assigned value. Existing `emitAssignVar` does not produce a value, so the lowering can return `vNew` after emitting assignment.

Operator mapping:

- `+=` -> `IROp::Add`
- `-=` -> `IROp::Subtract`
- `*=` -> `IROp::Multiply`
- `/=` -> `IROp::Divide`

Existing bytecode and Rust VM support these operations already, so no bytecode format change is required.

## Testing Strategy

### Success Goldens

Add fixtures such as:

- `compound_assignment_basic`

```cd
let x = 1;
print x += 2;
print x -= 1;
print x *= 5;
print x /= 2;
print x;
```

Expected:

```text
3
2
10
5
5
```

- `compound_assignment_expression_result`

```cd
let x = 1;
let y = (x += 4) * 2;
print x;
print y;
```

Expected:

```text
5
10
```

Include `ast.out`, `ir.out`, `bytecode.out`, and `run.out` where useful. Add representative success fixtures to Rust VM parity.

### Parse Errors

Add fixtures for unsupported targets:

```cd
let xs = [1];
xs[0] += 1;
```

```cd
let s = { value: 1 };
s.value += 1;
```

Expected parse error:

```text
Parse error at <line>:<column>: invalid compound assignment target
```

### Type Errors

Add fixtures for:

- Undefined variable: `x += 1;`.
- Known non-number variable: `let s = "x"; s += 1;`.
- Known non-number value: `let n = 1; n += true;`.
- Typed variable mismatch if needed: `let n: number = 1; n += "x";`.

### Runtime Errors

Add dynamic invalid RHS fixture:

```cd
fun id(x) { return x; }
let n = 1;
n += id("x");
```

Expected runtime error should match existing `add` invalid operand behavior.

Add division by zero fixture if not already covered for compound assignment:

```cd
let n = 1;
n /= 0;
```

Expected:

```text
Runtime error: division by zero
```

## Documentation

Update:

- `docs/language-grammar.ebnf` assignment grammar and operator token reference.
- `README.md` expression/assignment section.
- `docs/roadmap.md` Phase 15D status after implementation.
- `AGENTS.md` current language semantics.

## Future Work

Future slices may add:

- Compound assignment for array index targets.
- Compound assignment for struct field targets.
- String `+=` once string concatenation policy is explicitly designed.
- `++` / `--` if still desired after compound assignment exists.
