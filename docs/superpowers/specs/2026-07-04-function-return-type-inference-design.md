# Function Return Type Inference Design

## Goal

Add an internal, syntax-free return type inference layer for known function values. Calls to functions whose return type is statically known should produce that type in the type checker, allowing existing `let` inference and assignment checks to catch more errors.

Example:

```cd
fun one() {
  return 1;
}
let x = one();
x = "bad";
```

The final assignment should become a type error because `one()` is known to return `number`, so `x` is inferred as `number`.

## Background

The current type checker already tracks:

- static expression types through `StaticType`;
- lexical bindings in `TypeChecker::Binding`;
- function arity metadata for named functions, function expressions, and function-valued variables;
- inferred `let` types when initializer expressions have known types.

Calls currently preserve arity checks but return `Unknown`. Phase 9C extends the same internal metadata pattern with function return type information.

## User-Visible Behavior

No syntax changes are introduced.

Named functions and function expressions keep the existing forms:

```cd
fun name(parameter*) {
  declaration*
}

fun (parameter*) {
  declaration*
}
```

When a known function value has an inferred return type, calling it returns that type in the static checker:

```cd
fun greet() {
  return "hi";
}

let message = greet();  // message is string
message = 123;          // type error
```

Anonymous function values behave the same way:

```cd
let make = fun () {
  return 42;
};

let n = make(); // n is number
n = true;       // type error
```

Unknown return types remain dynamic and should not introduce new static rejections:

```cd
fun maybe(flag) {
  if flag {
    return 1;
  } else {
    return "x";
  }
}

let v = maybe(true); // v remains unknown
v = false;           // allowed by this phase
```

## Return Type Inference Rules

Each checked function body produces an inferred `StaticType` return type.

Rules:

1. `return expression;` contributes the static type of `expression`.
2. `return;` contributes `nil`.
3. A function with no explicit return contributes `nil`.
4. Multiple returns with the same known type infer that known type.
5. If any return contribution is `Unknown`, the function return type becomes `Unknown`.
6. If two known return contributions conflict, such as `number` and `string`, the function return type becomes `Unknown`.
7. This phase does not report a type error for conflicting return types.

This conservative join operation avoids committing to union types or function return annotations before the language has syntax for them.

## Metadata Model

Extend internal expression and binding metadata with return type information.

Conceptually:

```cpp
struct FunctionInfo {
    std::optional<std::size_t> arity;
    StaticType returnType;
};
```

The implementation may store this as separate fields to keep the existing code small, for example:

```cpp
struct CheckedExpression {
    StaticType type;
    std::optional<std::size_t> arity;
    StaticType returnType;
};

struct Binding {
    StaticType type;
    std::string resolvedName;
    std::optional<std::size_t> arity;
    StaticType returnType;
    std::size_t scopeDepth;
    std::size_t functionDepth;
};
```

`returnType` is meaningful only when `type == StaticType::Function`. For non-function values it should be `StaticType::Unknown`.

## Type Checker Flow

### Named Function Declarations

Named functions need self-reference for recursion. Continue declaring the function binding before checking the body.

Flow:

1. Declare the function binding with known arity and provisional `returnType = Unknown`.
2. Check the function body in a new scope.
3. Infer the body return type while checking return statements.
4. Update the already-declared binding's `returnType` after the body is checked.

Recursive calls inside the same function may still see `Unknown` return type during body checking. This is acceptable for Phase 9C and avoids fixed-point inference.

Example:

```cd
fun f(n) {
  if n == 0 {
    return 1;
  }
  return f(n - 1);
}
```

Because one return contribution is a recursive call with unknown return type during checking, `f` may infer `Unknown`. Full recursive return inference is out of scope.

### Function Expressions

Function expressions can be checked as self-contained function bodies.

Flow:

1. Check the function expression body in a new scope.
2. Infer its return type.
3. Return `CheckedExpression{Function, arity, returnType}`.

### Variable Reads

When reading a variable bound to a known function value, return expression info should include:

- `type = Function`;
- known `arity`, if any;
- known `returnType`, if any.

### Let Declarations

Unannotated `let` declarations preserve function metadata from initializer expression info:

```cd
let f = fun () {
  return 1;
};
```

The binding for `f` should store arity `0` and return type `number`.

Explicitly annotated `let` declarations still use only the existing simple annotation types. Since function type annotations do not exist, explicit annotations should not preserve function metadata unless the declared type is a function in the future.

### Assignment

Function assignment should continue to check known arity compatibility from Phase 9B.

After a valid assignment to a function-typed binding:

- if the assigned value is a function with known metadata, update both arity and return type;
- if the assigned value has unknown function metadata, clear stale metadata by setting arity to `nullopt` and return type to `Unknown`.

This preserves dynamic behavior for calls through variables reassigned from unknown-return function values.

### Calls

`checkCall()` should:

1. Check callee expression info.
2. Check arguments as before.
3. Reject calls to known non-functions as before.
4. Check known arity as in Phase 9B.
5. Return `CheckedExpression{callee.returnType, nullopt, Unknown}` when callee is a known function.
6. Return `CheckedExpression{Unknown, nullopt, Unknown}` otherwise.

Call expressions themselves are not function values unless a future phase supports returning known function values with metadata. If a function returns a function, the call result type may be `Function`, but arity and nested return metadata should remain unknown in Phase 9C.

## Return Tracking Design

Add a small function context stack to `TypeChecker` while checking function bodies.

Conceptual state:

```cpp
struct FunctionReturnContext {
    bool sawReturn = false;
    StaticType returnType = StaticType::Nil;
};

std::vector<FunctionReturnContext> returnContexts_;
```

When checking `return` statements:

- require `functionDepth_ > 0`, as today;
- compute contributed type (`expr` type or `nil`);
- merge it into the current return context;
- do not change runtime lowering.

A merge helper should encode the inference rules:

```cpp
StaticType mergeReturnTypes(StaticType current, StaticType next)
```

Suggested behavior:

- first return sets the type;
- same type keeps that type;
- `Unknown` on either side returns `Unknown`;
- differing known types return `Unknown`.

The no-return case is represented by `sawReturn == false`, then final function return type is `nil`.

## Diagnostics

New diagnostics are only indirect assignment/type compatibility errors caused by better call result typing.

Example expected diagnostic:

```text
Type error at 5:1: cannot assign string to `x` of type number
```

No new diagnostic category or format is introduced.

Conflicting return types do not produce diagnostics in this phase.

## Testing Strategy

Add type-error coverage:

- named function returning `number`, assigned through `let`, later assigned `string`;
- function expression returning `number`, assigned through `let`, later assigned `bool`;
- function variable assignment updates return metadata, so a later call result gets the new known type.

Add success coverage:

- named function returning `string`, then string concatenation through call result;
- function expression returning `number`, then numeric arithmetic through call result;
- no explicit return infers `nil`, so `let x = f(); x = nil;` remains valid;
- mixed return types degrade to `Unknown`, preserving dynamic assignment behavior.

Run both `--run` and `--run-bytecode` success paths where runtime output is relevant. Type-error fixtures are enough for static-only checks.

## Documentation Updates

Update:

- `README.md` to mention known function values carry arity and return type metadata for static checking, while parameter types, return annotations, and function type annotations remain unimplemented.
- `docs/roadmap.md` to mark Phase 9C as implemented after code lands.
- `AGENTS.md` current semantics to record known function return type inference.

No grammar documentation changes are needed because there is no syntax change.

## Non-Goals

This phase does not implement:

- parameter type inference;
- parameter type annotations;
- return type annotations;
- function type annotation syntax;
- union types;
- recursive fixed-point return inference;
- static errors for mixed return types;
- IR, bytecode, VM, or runtime function representation changes.

## Risks and Mitigations

Risk: recursive functions may not infer precise return types.

Mitigation: explicitly accept this limitation and keep recursive fixed-point inference for a later phase.

Risk: stale return metadata after function-valued assignment could produce false type errors.

Mitigation: clear return metadata whenever the assigned function value has unknown return metadata.

Risk: this could look like user-visible return type checking even though there is no annotation syntax.

Mitigation: document that conflicting returns degrade to `Unknown` instead of erroring, and that this is only best-effort internal inference.
