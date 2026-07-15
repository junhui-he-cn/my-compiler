# Generic Lambda Design

## Goal

Allow anonymous function expressions to declare their own inferred type
parameters, so a generic function value can be created without introducing a
named top-level declaration.

The first supported form is:

```cd
let identity = fun<T>(value: T): T {
  return value;
};

print identity<number>(42);
print identity("hello");
```

## Scope

- A function expression may use the same ordered type-parameter list as a
  named function: `fun<T, U>(...)`.
- Parameter and return annotations in the expression may refer to those local
  type parameters, including nested array, map, nullable, and function shapes
  already supported by `TypeInfo`.
- The existing generic call checker performs explicit substitution or
  argument-based inference when the lambda is called through an unannotated
  alias.
- Duplicate type parameters use the existing stable type diagnostic.
- Generic lambda values preserve their generic signatures through unannotated
  `let` bindings and assignments.
- Generic lambdas are not coerced to monomorphic function annotations. The
  existing rejection for assigning a generic function to a monomorphic
  function type remains in effect.
- Constraints, generic function-type annotations, and generic callback
  collection APIs remain separate follow-up slices.

## Syntax and AST

Change `FunctionExpr` to carry `std::vector<Token> typeParameters`. The parser
accepts an optional type-parameter list immediately after `fun`, before the
parameter list. AST output prints it as `(fun<T>(...))`, matching named
function and method printing conventions.

At statement-declaration dispatch, `fun<T>(...) { ... };` must continue down
the expression path rather than being reported as a malformed named function
declaration.

## Type checking

`checkFunctionExpression` opens a local type-parameter scope before resolving
parameter and return annotations, checks the body while that scope is active,
and returns a `TypeInfo::Function` carrying the ordered generic parameter
names. Existing `checkFunctionCall` then handles inference, explicit type
arguments, argument compatibility, and substituted return types without any
backend changes.

Nested generic lambdas use the same type-parameter escape rule as nested named
functions: a nested function may not expose an enclosing generic parameter in
its own function signature.

## Backend and modules

No IR, bytecode, Rust VM, module-interface, or runtime representation changes
are needed. Generic parameters are compile-time metadata and are erased before
IR lowering. A generic lambda's closure behavior remains identical to an
ordinary lambda.

## Verification

Add success coverage for inferred calls, explicit calls, array-shaped
parameters, zero-argument generic returns, and a standalone generic lambda
expression. Add type-error coverage for duplicate parameters, conflicting
inference, missing inference, explicit argument mismatch, monomorphic
assignment, and nested type-parameter escape.
