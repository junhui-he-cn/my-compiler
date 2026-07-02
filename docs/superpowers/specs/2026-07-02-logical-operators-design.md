# Logical Operators Design

## Goal

Implement Phase 3 from `docs/roadmap.md`: add short-circuit logical expression operators to the language using C-style operator spellings:

```cd
expr || expr
expr && expr
```

The operators should integrate with existing truthiness semantics, expression precedence, AST printing, type checking, IR generation, IR interpretation, documentation, and golden tests.

## Current State

The language currently supports literals, variables, grouping, unary operators, binary arithmetic/comparison/equality operators, and assignment expressions. `if` conditions and unary `!` use runtime truthiness through `isTruthy()`:

- `nil` is falsy.
- `false` is falsy.
- numbers, strings, and `true` are truthy.

The parser precedence stack currently goes from assignment directly to equality:

```text
expression -> assignment -> equality -> comparison -> term -> factor -> unary -> primary
```

The IR already supports forward control flow with `Jump` and `JumpIfFalse`, which is enough for `if` statements but not ideal for expression-level `||` results.

## Chosen Syntax

Add two logical operators:

- `&&` for logical and
- `||` for logical or

Do not add `and` or `or` keywords. Do not add single-character `&` or `|` operators. A lone `&` or `|` remains invalid source and should be reported by the lexer as an unexpected character.

## Grammar and Precedence

Insert two precedence levels between assignment and equality:

```ebnf
expression  = assignment ;

assignment  = identifier, "=", assignment
            | logicalOr ;

logicalOr   = logicalAnd,
              { "||", logicalAnd } ;

logicalAnd  = equality,
              { "&&", equality } ;

equality    = comparison,
              { ( "==" | "!=" ), comparison } ;
```

Precedence and associativity:

- Assignment remains the lowest-precedence expression form and remains right-associative.
- `||` is lower precedence than `&&`.
- `&&` is lower precedence than equality.
- `&&` and `||` are left-associative.

Example:

```cd
a || b && c == d
```

parses as:

```text
a || (b && (c == d))
```

## Runtime Semantics

Logical operators use existing truthiness and return operand values, not forced booleans.

For `left || right`:

1. Evaluate `left`.
2. If `left` is truthy, the expression result is the left value and `right` is not evaluated.
3. Otherwise evaluate `right`; the expression result is the right value.

For `left && right`:

1. Evaluate `left`.
2. If `left` is falsy, the expression result is the left value and `right` is not evaluated.
3. Otherwise evaluate `right`; the expression result is the right value.

Examples:

```cd
print "fallback" || "x"; // fallback
print nil || "x";        // x
print "x" && 42;         // 42
print false && (1 / 0);  // false, no division error
print true || (1 / 0);   // true, no division error
```

## AST Design

Add a dedicated expression node instead of reusing `BinaryExpr`:

```cpp
struct LogicalExpr final : Expr {
    LogicalExpr(ExprPtr left, Token op, ExprPtr right);
    void print(std::ostream& out) const override;

    ExprPtr left;
    Token op;
    ExprPtr right;
};
```

Rationale:

- Logical expressions have different evaluation semantics from ordinary binary expressions because the right side may not execute.
- A dedicated node keeps parser, type checker, and IR compiler intent explicit.
- Future features with side effects, functions, or calls will rely on this distinction being clear.

AST printing should use the existing compact prefix style. For example:

```cd
print true || false && nil;
```

should print as:

```text
(print (|| true (&& false nil)))
```

inside the normal program tree format.

## Type Checking

Add `LogicalExpr` handling to `TypeChecker::checkExpression()`.

Rules:

- Check both operands statically so variable resolution, duplicate declaration checks, undefined-variable errors, and assignment type checks still happen before IR compilation.
- Accept any operand static type because runtime semantics are based on truthiness.
- If the left and right static types are the same known type, the result type is that type.
- Otherwise the result type is `unknown`.
- If either side is `unknown`, the result type is `unknown`.

This matches the current partial type-checking design: explicit annotation errors are caught when statically knowable, while truthiness-oriented constructs remain flexible.

## IR Design

Add two small IR capabilities:

- `Copy`: copy a value from an existing register into a destination register.
- `JumpIfTrue`: jump to an instruction target when a condition register is truthy.

Rationale:

- Logical expressions need a stable result register whose value may come from either operand.
- `Copy` allows the compiler to initialize that result register with the left value and overwrite it only when the right side is evaluated.
- `JumpIfTrue` expresses `||` directly and avoids generating an extra `not` instruction solely to reuse `JumpIfFalse`.
- Both operations are small, general-purpose IR additions that may be useful for later control-flow features.

Expected printed IR names:

- `copy`
- `jump_if_true`

`JumpIfTrue` should follow the same target validation and printing style as `JumpIfFalse`.

## IR Lowering

`left || right` lowers to:

```text
leftReg = compile(left)
result = copy leftReg
jump_if_true result, end
rightReg = compile(right)
result = copy rightReg
end:
```

`left && right` lowers to:

```text
leftReg = compile(left)
result = copy leftReg
jump_if_false result, end
rightReg = compile(right)
result = copy rightReg
end:
```

The right operand must only be compiled into instructions after the conditional jump, so runtime execution skips those instructions when short-circuiting.

Nested logical expressions can lower recursively using the same expression compiler path.

## Error Model

Lexical errors:

- A single `&` should remain an unexpected character error.
- A single `|` should remain an unexpected character error.

Parse errors:

- Missing right operand after `&&` or `||` should produce the existing parser's stable `expected expression` style error.

Type errors:

- Logical operators themselves should not introduce operand type errors.
- Operand expressions may still produce existing type errors, such as undefined variables or invalid assignments, during type checking.

Runtime errors:

- Runtime errors in a short-circuited right operand must not occur.
- Runtime errors in an evaluated operand should behave normally.

## Golden Test Coverage

Add successful fixtures covering:

1. Precedence: `true || false && false` evaluates as `true || (false && false)`.
2. `||` short-circuit: a truthy left operand skips a right operand that would fail, such as `1 / 0`.
3. `&&` short-circuit: a falsy left operand skips a right operand that would fail.
4. Operand-value results: truthy/falsy operands return original values rather than forced booleans.
5. Assignment side effects: a short-circuited right-side assignment does not run, while a non-short-circuited one does.
6. AST output includes `LogicalExpr` prefix forms.
7. IR output includes `copy`, `jump_if_true`, and `jump_if_false` for logical expressions.

Add parse-error fixtures covering:

1. `print true && ;`
2. `print false || ;`

Do not add lone `&` or `|` golden fixtures in this phase because the current golden runner has parse-error, type-error, and runtime-error categories but no lexer-error category. The lexer behavior should remain unchanged: lone `&` or `|` raises the existing unexpected-character error.

## Documentation Updates

Update `docs/language-grammar.ebnf` to include the new precedence levels and operators.

Update `README.md` to list `&&` and `||` as supported expressions and describe short-circuit truthiness/value-return semantics concisely.

Update `docs/roadmap.md` after implementation to mark Phase 3 as implemented.

`AGENTS.md` does not need workflow changes unless implementation discovers a new convention worth preserving.

## Non-Goals

- Do not add keyword aliases `and` or `or`.
- Do not add single-character logical or bitwise operators `&` or `|`.
- Do not require operands to be booleans.
- Do not force logical expression results to booleans.
- Do not add full type inference.
- Do not add loops, functions, arrays, or new value types.
- Do not change existing truthiness rules.
- Do not change CLI modes.
