# Match Guard Design

## Goal

Allow existing enum match arms to refine a pattern with an expression guard.
Guards apply to both statement and expression forms:

```cd
match result {
  Result.Ok(value) if value > 0 => { print value; }
  Result.Ok(_) => { print 0; }
  Result.Empty => { print 0; }
}

let label = match result {
  Result.Ok(value) if value > 0 => "positive",
  Result.Ok(_) => "zero",
  Result.Empty => "empty",
};
```

## Scope and semantics

- A guard appears between a pattern and `=>`: `pattern if expression =>`.
- The pattern is tested first and its bindings are available while evaluating
  the guard and the arm body/value.
- Guards use the language's existing truthiness rules. A true guard selects the
  arm; a false guard continues matching later arms in source order.
- Guards are evaluated at most once for the first arm whose pattern matches.
- Statement arms still contain blocks; expression arms still contain one
  expression and comma separators.
- No new runtime value or bytecode opcode is introduced.

## Exhaustiveness and typing

- A guarded arm never contributes to exhaustive coverage because its condition
  may be false at runtime. An unguarded variant, wildcard, or binding arm must
  cover every possible value.
- Guard expressions are checked in the arm-local pattern scope. Their type is
  otherwise unrestricted, matching `if` conditions and existing truthiness.
- Expression arm result compatibility and all existing pattern checks remain
  unchanged.
- A match statement containing only guarded arms may fall through when every
  guard is false; functions with annotated returns must not rely on such a
  match to prove a return on every path.

## Lowering

After pattern tests and binding stores, lower the guard to `jump_if_false` and
add that jump to the arm's failure path. The existing arm-end jump remains
unchanged. This preserves single evaluation, source order, and the current
variant tag/field bytecode format.

## Verification

Cover statement and expression guards, bindings visible in guards, false-guard
fall-through, guarded wildcard behavior, exhaustive fallback arms, guard
truthiness, non-exhaustive guarded matches, guard binding-scope errors, AST/IR/
bytecode output, and Rust VM parity.
