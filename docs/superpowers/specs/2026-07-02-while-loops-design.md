# While Loops Design

## Goal

Implement Phase 4 from `docs/roadmap.md`: add `while` loops to support repeated control flow in the language.

The feature should integrate vertically with the lexer, parser, AST printer, type checker, IR compiler, IR interpreter, golden tests, grammar documentation, README, and roadmap.

## Current State

The language currently supports:

- `let` declarations with optional explicit type annotations
- `print` statements
- `if` / `else` with block bodies
- standalone blocks
- expression statements
- assignment expressions
- short-circuit logical operators `&&` and `||`

Control-flow truthiness is already established:

- `nil` is falsy.
- `false` is falsy.
- numbers, strings, and `true` are truthy.

The IR already has `Jump` and `JumpIfFalse`. The interpreter validates jump targets and can execute jumps to any instruction index within the program, including earlier instructions. The missing piece for loops is an IR construction helper that emits a jump to a known existing instruction index instead of only patching forward jumps.

## Chosen Syntax

Add `while` as a reserved keyword.

A `while` loop has this form:

```cd
while condition {
  declaration*
}
```

The body must be a block, matching the existing `if` syntax style. A single-statement body without braces is not supported.

Valid:

```cd
while i < 3 {
  print i;
}
```

Invalid:

```cd
while i < 3 print i;
```

## Grammar

Add `whileStmt` to the statement grammar:

```ebnf
statement   = printStmt
            | ifStmt
            | whileStmt
            | block
            | exprStmt ;

whileStmt   = "while", expression, block ;
```

The `expression` grammar is unchanged.

## Runtime Semantics

A `while` loop evaluates its condition before each iteration.

Execution steps:

1. Evaluate the condition expression.
2. If the condition is falsy, exit the loop.
3. If the condition is truthy, execute the body block.
4. After the body finishes, jump back and evaluate the condition again.

The condition uses the existing truthiness rules. It is not required to have static type `bool`.

Examples:

```cd
let i: number = 0;
while i < 3 {
  print i;
  i = i + 1;
}
```

prints:

```text
0
1
2
```

A loop whose condition starts falsy does not execute its body:

```cd
while false {
  print "skipped";
}
print "done";
```

prints:

```text
done
```

There is no infinite-loop protection in this phase. Programs with non-terminating loops keep running until externally interrupted.

## AST Design

Add a dedicated statement node:

```cpp
struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr condition, StmtPtr body);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr body;
};
```

Rationale:

- `while` is a statement-level control-flow construct, like `IfStmt`.
- A dedicated node keeps parser, type checker, and IR compiler logic explicit.
- Later `break` and `continue` support can target `WhileStmt` lowering without changing unrelated statement nodes.

AST printing should follow existing tree style. For:

```cd
while i < 3 {
  print i;
}
```

print:

```text
Program
  While (< i 3)
    Body
      Block
        Print i
```

## Type Checking and Scope

Add `WhileStmt` handling to `TypeChecker::checkStatement()`.

Rules:

- Check the condition with existing `checkExpression()`.
- Do not require condition type `bool`; truthiness is accepted for all runtime values.
- Check the body with existing `checkStatement()`.
- Because the parser requires a block body, the existing `BlockStmt` handling creates a lexical scope for the loop body.

Existing scope behavior should remain unchanged:

- Variables declared inside the loop body are not visible after the loop.
- Inner declarations may shadow outer variables.
- Assignment inside the body updates the nearest existing binding.
- Same-scope duplicate declarations are type errors.

Example type error:

```cd
while true {
  let x = 1;
}
print x;
```

reports:

```text
Type error: undefined variable `x`
```

The type checker is static and does not attempt control-flow termination analysis. It should still reject `print x;` after the loop even if the loop condition is `true`.

## IR Design

Reuse existing IR control-flow operations:

- `Jump`
- `JumpIfFalse`

Add a minimal IR builder helper for known-target jumps:

```cpp
void emitJumpTo(std::size_t target);
```

`IRProgram::instructionCount()` already exposes the current instruction index and can be used to mark the loop start.

Rationale:

- The interpreter already supports arbitrary valid jump targets.
- Backward jumps are a natural expression of loops in the existing register IR.
- No high-level `While` IR opcode is needed.

## IR Lowering

Lower:

```cd
while condition {
  body
}
```

as:

```text
loopStart = instructionCount()
condition = compile(condition)
exitJump = emitJumpIfFalse(condition)
compile(body)
emitJumpTo(loopStart)
patchJump(exitJump)
```

For:

```cd
let i = 0;
while i < 3 {
  print i;
  i = i + 1;
}
```

IR should have this shape:

```text
constant 0
store_var i#0
loopStart:
load_var i#0
constant 3
less
jump_if_false condition, afterLoop
... body ...
jump loopStart
afterLoop:
```

The exact register numbers and constant/name indexes are determined by the existing compiler order and should be captured in golden files.

## Error Model

Lexical errors:

- `while` becomes a keyword and is no longer available as an identifier.

Parse errors:

- Missing `{` after a `while` condition should report a stable parse error using the existing parser style:

```text
Parse error at line ..., column ...: expected `{` after while condition, found ...
```

Type errors:

- `while` itself does not introduce condition type errors.
- Condition and body expressions may still produce existing type errors, such as undefined variables or invalid assignments.
- Variables declared in the loop body remain scoped to the body block.

Runtime errors:

- Runtime errors while evaluating the condition abort execution normally.
- Runtime errors in an executed body abort execution normally.
- Runtime errors in a body that is skipped because the condition is falsy do not occur.

## Golden Test Coverage

Add successful fixtures covering:

1. Zero iterations:
   ```cd
   while false {
     print "skipped";
   }
   print "done";
   ```

2. Counting loop:
   ```cd
   let i: number = 0;
   while i < 3 {
     print i;
     i = i + 1;
   }
   ```

3. Loop body scope and shadowing:
   ```cd
   let x = "outer";
   let i = 0;
   while i < 1 {
     let x = "inner";
     print x;
     i = i + 1;
   }
   print x;
   ```

4. Nested control flow:
   ```cd
   let i = 0;
   while i < 3 {
     if i == 1 || i == 2 {
       print i;
     }
     i = i + 1;
   }
   ```

5. IR output with a backward `jump`.

Add parse-error fixture:

```cd
while true print 1;
```

Add type-error fixture:

```cd
while true {
  let x = 1;
}
print x;
```

## Documentation Updates

Update `docs/language-grammar.ebnf`:

- Add `whileStmt` to `statement`.
- Add the `whileStmt = "while", expression, block ;` production.

Update `README.md`:

- Add `while expression { declaration* }` to supported statements.
- Mention that `while` uses the same truthiness rules as `if`, `!`, `&&`, and `||`.

Update `docs/roadmap.md` after implementation:

- Mark Phase 4 as implemented.
- Document that the implemented body form is block-only and there is no `break`/`continue` yet.

`AGENTS.md` does not need changes unless implementation discovers a new workflow or architecture convention that future agents need to know.

## Non-Goals

- Do not add `break`.
- Do not add `continue`.
- Do not add `for` loops.
- Do not support unbraced single-statement loop bodies.
- Do not require boolean-only conditions.
- Do not add infinite-loop detection or iteration limits.
- Do not change truthiness rules.
- Do not change expression grammar.
- Do not change CLI modes.
