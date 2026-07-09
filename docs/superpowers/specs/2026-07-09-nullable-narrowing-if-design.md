# Nullable Narrowing in If Conditions Design

## Goal

Implement the first flow-sensitive nullable narrowing slice for `if` statements so a local variable of type `T?` can be treated as `T` in branches where a direct nil-check proves it is non-nil.

## Scope

Supported in this slice:

- `if (x != nil) { ... }` narrows `x: T?` to `T` in the then branch.
- `if (x == nil) { ... } else { ... }` narrows `x: T?` to `T` in the else branch.
- The nil literal may appear on either side: `nil != x` and `nil == x` are equivalent.
- Only simple variable expressions are narrowed.
- Narrowing is type-checker-only; it does not change AST, IR, bytecode, runtime values, or VM behavior.

Out of scope for this slice:

- `while` / loop narrowing.
- Narrowing through `&&`, `||`, `!`, early returns, or nested boolean expressions.
- Narrowing fields, array elements, member calls, function calls, or arbitrary expressions.
- Proving that `x == nil` makes `x` exactly `nil` in the then branch.
- Preserving narrowing after assignment or outside the narrowed branch.

## Semantics

When type checking an `if` statement condition, the checker also attempts to extract a direct nullable nil-check:

- `x != nil` or `nil != x` produces a non-nil narrowing for the then branch.
- `x == nil` or `nil == x` produces a non-nil narrowing for the else branch.

The variable must resolve to an existing local or global binding whose current static type is `Nullable(inner)`. Inside the narrowed branch, reads of that variable use `inner` as the static type. The underlying binding type remains unchanged outside the narrowed branch.

If the variable is assigned inside the narrowed branch, the assignment still updates the original binding type rules. This first slice does not track assignment invalidation within a branch; instead, it keeps the scoped narrowing simple and restores the original view after leaving the branch. Since assignments to a `T?` binding still require values compatible with `T?`, this remains sound for the current checker.

## Architecture

Add a narrow overlay stack to `TypeChecker`, separate from lexical binding scopes. Variable lookup still finds the original `Binding`, but expression reads consult the topmost narrowing overlay and return the narrowed type when present.

Key additions:

- A `Narrowing` record containing a resolved variable name and narrowed `TypeInfo`.
- A small helper to recognize direct `x == nil` / `x != nil` conditions.
- Scoped helpers to push/pop branch-local narrowing overlays around `checkStatement` for the then/else branches.

This avoids mutating the binding itself and keeps branch-local behavior easy to reason about.

## Diagnostics and tests

New success fixtures should demonstrate nullable values flowing into non-nullable contexts inside narrowed branches:

- `if (x != nil) { takesNumber(x); }`
- `if (x == nil) { ... } else { takesNumber(x); }`
- `if (nil != x) { ... }`

New type-error fixtures should demonstrate the boundaries:

- No narrowing outside the branch.
- No narrowing for fields such as `box.value != nil`.
- No narrowing for non-direct boolean compositions in this slice.

Existing runtime behavior and bytecode artifacts should not change. Full golden, bytecode artifact, and Rust VM parity suites remain the verification gate.

## Documentation

Update `README.md`, `AGENTS.md`, and `docs/roadmap.md` to state that the first nullable narrowing slice supports direct `if` nil-checks on local variables only.
