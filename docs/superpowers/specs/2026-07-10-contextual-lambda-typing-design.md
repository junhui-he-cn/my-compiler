# Contextual Lambda Typing Design

## Goal

Add contextual typing for anonymous function expressions when they appear in a position that already has an expected function type. This lets unannotated lambda parameters and returns use nearby type annotations without adding global inference.

Example:

```cd
let increment: fun(number): number = fun (x) {
  return x + 1;
};
print increment(2);
```

`x` is checked as `number` because the `let` annotation provides `fun(number): number`.

## Non-Goals

- Do not infer lambda parameter types from the lambda body.
- Do not infer a function signature from arbitrary call sites without an expected type.
- Do not add generic function types, overload resolution, or bidirectional inference beyond the existing `expectedType` path.
- Do not change IR, bytecode, `.cdbc`, or Rust VM execution. Runtime representation of function values is unchanged.
- Do not add additional nullable narrowing work as part of this slice.

## Current Behavior

`TypeChecker::checkExpressionInfo(const Expr&, const TypeInfo*)` already accepts an optional expected type and passes it into many expression positions, including typed `let` initializers, function returns, assignment right-hand sides, call arguments with known function signatures, array elements with an expected element type, and struct fields.

Function expressions currently ignore that expected type. Unannotated lambda parameters are checked as `unknown`, and an unannotated lambda return type is inferred only from its body. That means a lambda can fail to type-check even when its surrounding context carries a precise function signature.

## Proposed Semantics

When a `FunctionExpr` is checked with an expected type that is a function type with a complete signature:

1. The lambda arity must match the expected function arity.
2. Each unannotated lambda parameter receives the corresponding expected parameter type.
3. Each explicitly annotated lambda parameter keeps its annotation, and the annotation must be compatible with the corresponding expected parameter type. A conflict is a type error at the parameter name.
4. The lambda body is checked with the expected return type. Existing return checking and implicit-nil-return checking apply.
5. The resulting lambda expression type is a function type using the contextual parameter types and contextual return type, refined only by existing body checking behavior.

When a `FunctionExpr` is checked without an expected type, or with an expected type that is not a function type with a complete signature, current behavior remains unchanged: unannotated parameters are `unknown`, annotated parameters use their annotations, and return type is inferred or validated from explicit annotations.

A function type is considered usable as contextual input only when `kind == StaticType::Function`, it has `returnType`, and `hasFunctionSignature(type)` is true.

## Contexts Covered

The feature applies anywhere the existing checker already passes an expected type into `checkExpressionInfo`, including:

- `let name: fun(...): ... = fun (...) { ... };`
- assignment to a variable with a known function type;
- call arguments when the callee has a known function signature;
- `return fun (...) { ... };` in a function with an annotated function return type;
- array literals checked against an expected array-of-function type;
- named struct literal fields whose field type is a function type;
- index and field assignment right-hand sides when the target has a known function type.

This design intentionally does not add new expected-type propagation paths beyond the ones already present.

## Error Behavior

### Arity mismatch

If contextual function arity and lambda arity differ, report the same style as function calls:

```text
Type error at <line>:<column>: expected N parameters but got M
```

The diagnostic should point at the `fun` token or the nearest stable lambda token.

### Explicit parameter conflict

For an explicitly annotated lambda parameter, if the annotation is not compatible with the contextual parameter type, report:

```text
Type error at <line>:<column>: parameter `name` expects <expected>, got <actual>
```

The diagnostic should point at the parameter name.

### Return conflict

If an explicit lambda return annotation conflicts with the contextual return type, report a type error at the lambda return annotation token. Return statement conflicts inside the body use existing `recordReturn` / `checkFunctionBody` diagnostics, for example:

```text
Type error at <line>:<column>: cannot return number from function returning string
```

Implicit fallthrough in a lambda with a contextual non-nil return type should continue to use the existing `function `<label>` may return nil but is annotated <type>` diagnostic.

## Architecture

### TypeChecker API

Change the private function-expression checker from:

```cpp
CheckedExpression checkFunctionExpression(const FunctionExpr& expression);
```

to accept contextual type information:

```cpp
CheckedExpression checkFunctionExpression(const FunctionExpr& expression, const TypeInfo* expectedType);
```

Then update `checkExpressionInfo` so `FunctionExpr` nodes call the new overload with the current `expectedType`.

### Context Extraction

Add a small helper in `TypeChecker` to extract contextual function signatures:

```cpp
const TypeInfo* contextualFunctionType(const TypeInfo* expectedType) const;
```

It returns `expectedType` only when it is a function type with a complete signature. Otherwise it returns `nullptr`.

### Parameter Type Selection

`checkFunctionExpression` builds `declaredParameterTypes` by iterating lambda parameters:

- If a parameter has an annotation, resolve it as today.
- Else if a contextual function type exists, use the corresponding contextual parameter type.
- Else use `unknownType()` as today.

After selecting the parameter type, if contextual parameter type exists and the selected type is incompatible with it, report the explicit-annotation conflict. Because unannotated parameters copy the contextual type, this conflict only normally affects annotated parameters.

### Return Type Selection

For the lambda expected return type:

- Explicit lambda return annotation, if present, is resolved as today.
- Contextual return type, if present, is also enforced.
- If both are present, they must be compatible before checking the body.
- The body is checked with the effective expected return type when one exists.

The effective return type is the explicit return annotation when present, otherwise the contextual return type when present, otherwise no expected return type.

The resulting lambda type uses the parameter types selected above and the return type returned by `checkFunctionBody`.

## Testing Plan

Add golden success fixtures for:

1. Typed `let` contextual lambda parameters.
2. Call argument contextual lambda parameters.
3. Returning a lambda from a function with annotated function return type.
4. Struct field or array element contextual lambda typing using an expected function type.
5. Assignment to an existing binding with a function type.

Add type-error fixtures for:

1. Contextual lambda arity mismatch.
2. Explicit lambda parameter annotation conflict with context.
3. Lambda return value conflict with contextual return type.
4. Lambda fallthrough when contextual return type is non-nil.

Run focused golden and Rust VM tests for the new fixtures, then the full project verification set.

## Documentation

Update `README.md` to state that function expression parameters and returns can be contextually typed from an expected function type in annotated contexts, while global parameter inference remains unsupported.

Update `docs/roadmap.md` after implementation by removing the completed M1 contextual-lambda item.
