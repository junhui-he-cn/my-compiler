# Match Expression Design

## Goal

Add expression-form `match` to the existing named-enum pattern-matching
feature. The expression form reuses the current wildcard, binding, variant, and
nested payload patterns while allowing each arm to produce a value.

The supported form is:

```cd
let label = match result {
  Result.Ok(value) => "ok:" + str(value),
  Result.Err(message) => "err:" + message,
  Result.Empty => "empty",
};
```

## Scope and syntax

- A match expression is `match` followed by a scrutinee expression and a
  brace-delimited list of arms.
- Each arm is `pattern => expression`; arms are separated by commas and a
  trailing comma is optional.
- The arm expression is a single expression, not a statement block. Nested
  match expressions, calls, assignments, and collection/map literals remain
  available wherever ordinary expressions are available.
- The expression must be exhaustive over the scrutinee enum. `_` and binding
  patterns cover all variants, matching statement-form `match`.
- Pattern bindings exist only while checking and lowering their own arm.
- Guards, named payload fields, generic enums, nullable enum patterns, and
  expression arms containing statement blocks remain out of scope.

## Static checking

- The scrutinee must have a known named enum type.
- Existing pattern validation checks variant ownership, payload arity, nested
  payload types, and coverage.
- Every arm result must be compatible with the other arm results. If a match
  expression has an expected type from a declaration, parameter, or return,
  every arm must also be assignable to that expected type.
- A known result type is preserved when all arms agree; unknown arm types make
  the result unknown. Incompatible known arm types are type errors.
- The existing arm-local scope and resolved pattern names are reused, so a
  binding such as `value` is available in that arm's expression only.

## Runtime and backend

Lower the expression to the existing control-flow and variant operations:

1. Evaluate the scrutinee once.
2. Test arms in source order with `variant_tag` and nested `variant_field`
   operations.
3. Store successful pattern bindings, evaluate that arm's expression, and
   copy its result into the expression result register.
4. Jump over later arms after the first successful arm.

No new IR opcode, bytecode opcode, `.cdbc` syntax, or Rust VM runtime behavior
is required. Existing bytecode and VM paths therefore provide parity
automatically once the C++ IR compiler emits the control flow.

## Verification

Cover direct and nested expression matches, arm-local bindings, wildcard
coverage, expression use in declarations/returns/calls, AST/IR/bytecode
output, C++ execution, Rust VM parity, non-exhaustive matches, incompatible
arm result types, and binding-scope errors.
