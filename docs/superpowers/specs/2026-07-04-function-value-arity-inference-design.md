# Function Value Arity Inference Design

Date: 2026-07-04

## Goal

Implement Phase 9B: carry simple function arity metadata through function-valued expressions and variable bindings, so calls through variables and direct lambda calls can be checked statically when the callee's arity is known.

This phase extends the existing `Binding::arity` mechanism. It does not add parameter types, return types, function type annotation syntax, or new runtime function representations.

## Motivation

Named functions already record arity and support static argument-count checks when called directly:

```cd
fun add(a, b) {
  return a + b;
}

print add(1); // Type error today
```

Function values introduced by closures and function expressions are less precise:

```cd
let f = fun (x) {
  return x;
};

print f(); // Currently reaches runtime arity checking
```

After Phase 9A, unannotated `let` bindings preserve known static types such as `function`, but they still do not preserve the function's arity. Phase 9B closes that gap while keeping the type system small.

## Scope

In scope:

- Infer arity for direct function expressions.
- Preserve arity when a known function variable is assigned to another binding.
- Check calls through variables with known function arity.
- Check direct calls of function expressions with known arity.
- Preserve runtime arity checks for unknown arity cases.
- Reject assignment between function bindings when both sides have known, different arities.
- Keep same-arity function assignment valid.
- Update docs to describe function arity inference boundaries.
- Add golden tests for success and type-error behavior.

Out of scope:

- Function parameter type inference.
- Function return type inference.
- User-visible function type syntax.
- Function overloads, optional parameters, rest parameters, or default arguments.
- Storing arity in `StaticType` as a full function type.
- Changing IR, bytecode, VM, runtime `FunctionValue`, or runtime arity diagnostics.
- Changing parser or grammar.

## User-Visible Semantics

### Lambda assigned to a variable

A direct function expression initializer gives the binding known function arity:

```cd
let f = fun (x) {
  return x;
};

print f(1); // ok
print f();  // Type error: expected 1 arguments but got 0
```

### Named function assigned to a variable

A variable initialized from a named function inherits that function's known arity:

```cd
fun add(a, b) {
  return a + b;
}

let f = add;
print f(1, 2); // ok
print f(1);    // Type error: expected 2 arguments but got 1
```

### Direct lambda calls

Direct calls of function expressions are checked statically:

```cd
print (fun (x) {
  return x;
})(1); // ok

print (fun (x) {
  return x;
})(); // Type error
```

### Assignment between function values

When both target and assigned value have known function arities, arities must match:

```cd
let f = fun (x) {
  return x;
};

f = fun (y) {
  return y + 1;
}; // ok, both arity 1

f = fun (x, y) {
  return x + y;
}; // Type error, arity 2 assigned to arity 1 binding
```

Suggested diagnostic:

```text
Type error at <line>:<column>: cannot assign function with <actual> parameters to `<name>` of type function with <expected> parameters
```

### Unknown arity preservation

If an expression is a function but the checker cannot know its arity, static arity checks should not guess:

```cd
fun id(x) {
  return x;
}

let f = fun (x) {
  return x;
};

f = id(f); // call result type and arity are unknown in this phase
```

After assigning an unknown-arity function value to `f`, the checker should not keep stale arity metadata for `f`. Later calls through `f` should defer arity validation to runtime unless a later assignment reestablishes known arity.

This conservative behavior avoids false static diagnostics.

## Internal Model

Keep `StaticType::Function` unchanged. Track arity as metadata alongside bindings and expression results.

The current `Binding` already has:

```cpp
std::optional<std::size_t> arity;
```

This phase needs expression checking to return both:

- static type
- optional known function arity

Recommended internal helper:

```cpp
struct CheckedExpression {
    StaticType type;
    std::optional<std::size_t> arity;
};
```

Then:

- literals return known non-function types with no arity.
- variable reads return the binding type and binding arity.
- function expressions return `StaticType::Function` with `parameters.size()` arity.
- named function declarations already create `Binding{StaticType::Function, arity = parameters.size()}`.
- calls return `StaticType::Unknown` with no arity.
- indexing remains `StaticType::Unknown` with no arity.
- arrays return `StaticType::Array` with no arity.
- non-function expressions return no arity.

To reduce churn, existing `checkExpression()` can remain as a wrapper that returns only `StaticType`, while a new private helper such as `checkExpressionWithInfo()` returns `CheckedExpression` for places that need arity. Alternatively, `checkExpression()` itself can be changed to return `CheckedExpression`, but that touches more code.

## Type Checking Rules

### Let declarations

For an unannotated `let`, store both initializer type and known arity:

```cd
let f = fun (x) { return x; };
```

The binding should be:

- type: `Function`
- arity: `1`

For explicit annotations, no function annotation syntax exists yet, so only built-in annotations remain supported. Arity metadata is only meaningful when the declared binding type is `Function`, which cannot currently be written as an annotation. Existing annotation behavior remains unchanged.

### Assignments

Assignment should continue checking static type compatibility. Add function arity checking when both sides are known functions and both arities are known.

Cases:

- target `Function` arity 1, value `Function` arity 1: ok; target arity remains 1.
- target `Function` arity 1, value `Function` arity 2: type error.
- target `Function` arity 1, value `Function` unknown arity: ok; target arity becomes unknown.
- target `Function` unknown arity, value `Function` arity 2: ok; target arity becomes 2.
- target `Unknown`, value `Function` arity 2: ok; target type may remain unknown under current assignment behavior, but the expression result can be function with arity 2 if the implementation already propagates value type for unknown targets. Do not broaden scope if this case requires restructuring.

The important safety rule is: never keep stale known arity after assigning an unknown-arity function value.

### Calls

Call checking should validate argument count when callee arity is known, regardless of whether the callee expression is a variable or a direct function expression:

```cd
let f = fun (x) { return x; };
f(); // arity known from variable binding

(fun (x) { return x; })(); // arity known from callee expression
```

If callee arity is unknown, keep current behavior and let runtime arity checks handle it.

The existing direct named-function variable check should still work and can be replaced by the more general expression-info arity check.

## Diagnostics

Reuse the existing arity mismatch wording for calls:

```text
Type error at <line>:<column>: expected <expected> arguments but got <actual>
```

Use the call's closing paren token location, matching current named function arity diagnostics.

For incompatible function arity assignment, use the assignment target token location and stable wording:

```text
Type error at <line>:<column>: cannot assign function with <actual> parameters to `<name>` of type function with <expected> parameters
```

Keep parse/type/runtime diagnostic categories distinct.

## Testing Strategy

Add failing golden tests before implementation.

Type-error fixtures:

- `lambda_variable_wrong_arity.cd`

  ```cd
  let f = fun (x) {
    return x;
  };
  print f();
  ```

- `lambda_direct_wrong_arity.cd`

  ```cd
  print (fun (x) {
    return x;
  })(1, 2);
  ```

- `function_value_wrong_arity.cd`

  ```cd
  fun add(a, b) {
    return a + b;
  }
  let f = add;
  print f(1);
  ```

- `function_assignment_arity_mismatch.cd`

  ```cd
  let f = fun (x) {
    return x;
  };
  f = fun (x, y) {
    return x + y;
  };
  ```

Success fixtures:

- `function_value_arity_success/`
  - calls lambda variable with correct arity
  - calls named function through variable with correct arity
  - assigns same-arity lambda and calls it
  - include `run.out` and `run_bytecode.out`

- `function_value_unknown_arity_assignment/`
  - assigns a function value through a call result so static arity becomes unknown
  - includes a call that still succeeds at runtime
  - include `run.out` and `run_bytecode.out`

Regression checks:

- Existing direct named function wrong-arity fixture must still pass.
- Existing lambda runtime behavior must still pass.
- Full golden suite should pass.

## Documentation Updates

Update after implementation:

- `README.md`
  - Note that known function values carry arity for static argument-count checks.
  - Still state that function parameter types, function return types, and function type annotations are not implemented.

- `docs/roadmap.md`
  - Mark Phase 9B function value arity inference as implemented/in progress.
  - Keep function parameter/return type inference and function type annotations as future work.

- `AGENTS.md`
  - Update current language semantics to mention known function values carry arity for type checking.

No grammar update is needed.

## Risks and Mitigations

- **Risk:** Stale arity after assigning an unknown-arity function value could cause false type errors.
  - **Mitigation:** Clear target binding arity when assigning a function value with unknown arity.

- **Risk:** Changing expression checking from `StaticType` to richer metadata could create churn.
  - **Mitigation:** Prefer an internal `CheckedExpression` helper and keep `checkExpression()` as a compatibility wrapper if that keeps the patch smaller.

- **Risk:** Direct lambda call checking might double-check the lambda body or alter resolution side effects.
  - **Mitigation:** Ensure callee expression is checked once per call, and reuse the result for both callee type and arity.

- **Risk:** Runtime arity checks disappear for cases that should remain dynamic.
  - **Mitigation:** Keep unknown-arity call fixtures and only statically reject known arity mismatches.

## Success Criteria

- Calls through variables initialized from function expressions are statically arity-checked.
- Calls through variables initialized from named functions are statically arity-checked.
- Direct function expression calls are statically arity-checked.
- Same-arity function assignment succeeds.
- Known different-arity function assignment fails with a type error.
- Unknown-arity function assignment does not keep stale arity metadata.
- No parser, grammar, IR, bytecode, VM, or runtime value representation changes are required.
- Existing named function direct arity checks still pass.
- README, roadmap, and AGENTS describe the new arity inference boundary accurately.
