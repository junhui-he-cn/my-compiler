# C-Style For Loops Design

## Goal

Add C-style `for` loops as a practical iteration form that reuses existing declarations, expressions, block bodies, loop-control statements, and IR control-flow machinery.

## Syntax

The syntax is:

```cd
for [initializer]; [condition]; [increment] block
```

Examples:

```cd
for let i = 0; i < 3; i = i + 1 {
  print i;
}

for ; i < 10; i = i + 1 {
  print i;
}

for ;; {
  break;
}
```

## Non-goals

- No `for item in array` syntax in this slice.
- No comma-separated initializer or increment lists.
- No `++` or `--` operators.
- No compound assignment such as `+=`.
- No new runtime collection iteration protocol.

## Header Semantics

The initializer is optional. When present, it may be either:

- a `let` declaration without the trailing semicolon inside the parser helper, because the `for` header owns the first semicolon; or
- an expression used as an expression statement, also without an extra semicolon beyond the header separator.

The condition is optional. When omitted, it behaves as an always-true condition.

The increment is optional. When present, it is an expression evaluated after each completed loop body iteration and after `continue` jumps.

The body is always a block, matching existing `if` and `while` style.

## Scope Semantics

A `for` loop introduces a loop scope that contains the initializer, condition, increment, and body. A variable declared by the initializer is visible in the condition, body, and increment, but not after the loop.

Example:

```cd
for let i = 0; i < 3; i = i + 1 {
  print i;
}
print i; // type error
```

The body block still introduces its own nested block scope, so declarations inside the body remain local to the body block as they do today.

## Break and Continue

`break;` exits the nearest enclosing `for` or `while`.

`continue;` in a `for` loop jumps to the increment expression first, then returns to condition checking. If the increment is omitted, `continue;` jumps directly to condition checking.

Nested function bodies do not inherit loop-control permission from an enclosing `for`, matching existing `while` behavior.

## Type Checking

The type checker should treat `for` as a loop for loop-depth validation. It should check:

1. initializer, if present;
2. condition, if present;
3. increment, if present;
4. body.

The language currently uses truthiness for `if`, `while`, logical operators, and `!`, so this slice should not add strict boolean-only conditions.

## AST

Add a `ForStmt` with:

- `Token keyword`
- optional initializer statement
- optional condition expression
- optional increment expression
- body statement, expected to be a block

AST printing should make omitted pieces explicit enough for tests to be stable, for example:

```text
For
  Init ...
  Condition ...
  Increment ...
  Body ...
```

or compact equivalent. The exact format should follow existing AST printer style.

## Lowering and Runtime Behavior

IR lowering should compile `for` directly to control flow equivalent to:

```text
initializer
loop_start:
  condition or true
  jump_if_false loop_end
  body
continue_target:
  increment
  jump loop_start
loop_end:
```

The continue target for `for` is the increment block. The break target is the loop end. This may require extending the existing loop context to store separate break and continue labels, if it does not already.

No new runtime value, bytecode opcode, or Rust VM instruction is required if lowering reuses existing jumps and expression compilation.

## Tests

Add golden coverage for:

- basic counting loop;
- omitted initializer;
- omitted condition infinite loop with `break`;
- omitted increment;
- initializer scope does not escape;
- `continue` runs increment before checking condition;
- `break` exits a `for` loop;
- nested function inside a `for` cannot `break` or `continue` the outer loop;
- parse errors for malformed headers;
- bytecode artifact and Rust VM parity for representative `for` loops.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf`
- `README.md`
- `docs/roadmap.md`
- `AGENTS.md`

Docs should describe C-style `for` loops and explicitly leave `for-in` array iteration as future work.
