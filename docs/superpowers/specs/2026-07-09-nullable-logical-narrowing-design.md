# Nullable Logical Narrowing Design

## Goal

Extend the existing nullable narrowing slice from direct `if` nil checks to common logical combinations in `if` conditions, without introducing a full control-flow graph or changing runtime behavior.

## User-visible behavior

The type checker should narrow simple nullable variables in branches when boolean logic proves them non-nil:

- `if (x != nil && otherCondition) { ... }` narrows `x` to non-nullable inside the `then` branch.
- `if (nil != x && otherCondition) { ... }` has the same behavior.
- `if (x == nil || otherCondition) { ... } else { ... }` narrows `x` to non-nullable inside the `else` branch.
- `if (nil == x || otherCondition) { ... } else { ... }` has the same behavior.

The first slice intentionally supports only one proven simple-variable narrowing per branch. It does not narrow fields, indexes, arbitrary expressions, or post-branch flow.

## Scope

In scope:

- Type-checker-only changes to `ifNarrowing` logic.
- Success and type-error golden fixtures that capture logical `&&` / `||` behavior.
- Documentation updates in `README.md` and `docs/roadmap.md`.

Out of scope:

- Runtime, IR, bytecode, or Rust VM changes.
- Narrowing through loops or after an `if` statement completes.
- Multiple simultaneous narrowed variables from one condition.
- Field or index narrowing such as `person.name != nil` or `items[i] != nil`.
- Full short-circuit-sensitive expression dataflow inside condition operands.

## Design

The current implementation computes an `IfNarrowing` with optional `then` and `else` narrowings. This slice keeps that interface and teaches it to combine narrowings for logical binary expressions.

Rules:

- Direct comparisons keep their current behavior:
  - `x != nil` narrows `x` in `then`.
  - `x == nil` narrows `x` in `else`.
- For `a && b`, the `then` branch is narrowed by the first available `then` narrowing from `a`, otherwise from `b`. The `else` branch is left unchanged for this slice because `!(a && b)` does not prove either operand false individually.
- For `a || b`, the `else` branch is narrowed by the first available `else` narrowing from `a`, otherwise from `b`. The `then` branch is left unchanged for this slice because `a || b` does not prove either operand true individually.

This conservative rule avoids unsound narrowing while making the most common guard-style conditions useful.

## Error handling

No new diagnostic shape is needed. Programs that still use a nullable value where a non-nullable value is required continue to produce the existing type compatibility errors.

## Testing

Use TDD:

1. Convert the existing logical nullable unsupported fixture into a successful fixture that demonstrates `&&` narrowing.
2. Add a success fixture for `||` else-branch narrowing.
3. Add or keep type-error coverage showing unsupported field/index narrowing and unsupported post-branch narrowing remain rejected.
4. Run the golden runner before implementation to observe the new success fixture fail, then implement the minimal type-checker logic and refresh only the intended goldens with `--case`.

## Documentation

Update the nullable/type-system docs to mention direct nil checks and conservative logical `&&` / `||` combinations for simple variables only.
