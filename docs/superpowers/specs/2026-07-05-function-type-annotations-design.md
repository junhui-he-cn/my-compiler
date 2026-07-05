# Function Type Annotations Design

## Goal

Add first-class function type annotations to the static type checker so variables, parameters, and return values can declare callable signatures.

This phase extends the current function parameter and return annotation work without changing runtime function values, IR, bytecode, or Rust VM execution semantics.

## User-Visible Syntax

Function type annotations use the existing `fun` keyword:

```cd
fun(number, string): bool
```

They may appear anywhere current type annotations are accepted:

```cd
let pred: fun(number): bool = fun (x: number): bool {
  return x > 0;
};

fun apply(f: fun(number): number, x: number): number {
  return f(x);
}

fun makeAdder(): fun(number): number {
  return fun (x: number): number {
    return x + 1;
  };
}
```

Empty parameter lists are valid:

```cd
let thunk: fun(): number = fun (): number {
  return 1;
};
```

Supported non-function leaf type names remain:

- `number`
- `bool`
- `string`
- `nil`

Nested function types are valid where useful:

```cd
let factory: fun(): fun(number): number = fun (): fun(number): number {
  return fun (x: number): number {
    return x + 1;
  };
};
```

## Scope

This phase includes:

- Parsing function type annotations in `let` declarations.
- Parsing function type annotations in function parameter annotations.
- Parsing function type annotations in function return annotations.
- AST storage and printing for simple and function type annotations.
- Static function signature compatibility for annotated initialization, assignment, calls, parameters, and returns.
- Type-error and parse-error golden coverage.
- README, grammar, and roadmap updates.

This phase does not include:

- Array element types.
- Generic functions.
- Union or nullable type syntax.
- Type aliases.
- Function overloads.
- Runtime enforcement of signatures.
- IR, bytecode, `.cdbc`, or Rust VM changes.

## Grammar

Replace the current `typeName = identifier` annotation grammar with recursive type expressions:

```ebnf
typeExpr      = simpleType
              | functionType ;

simpleType    = identifier ;

functionType  = "fun", "(", [ typeArguments ], ")", ":", typeExpr ;

typeArguments = typeExpr,
                { ",", typeExpr } ;
```

Then update annotation sites to use `typeExpr`:

```ebnf
funDecl      = "fun", identifier,
               "(", [ parameters ], ")",
               [ ":", typeExpr ],
               block ;

parameter    = identifier,
               [ ":", typeExpr ] ;

letDecl      = "let", identifier,
               [ ":", typeExpr ],
               "=", expression, ";" ;

functionExpr = "fun", "(", [ parameters ], ")",
               [ ":", typeExpr ],
               block ;
```

The parser should keep rejecting unknown simple type names in the type checker, preserving existing diagnostic ownership for unknown annotation names.

## AST Shape and Printing

Introduce a type annotation representation instead of storing raw type-name tokens directly on declarations and parameters.

Suggested shape:

```cpp
struct TypeAnnotation {
    enum class Kind {
        Simple,
        Function,
    };

    Kind kind;
    Token token;
    std::vector<TypeAnnotation> parameterTypes;
    std::unique_ptr<TypeAnnotation> returnType;
};
```

Implementation may use a copyable recursive representation if that fits existing AST ownership better.

Update:

- `LetStmt::typeName` to `std::optional<TypeAnnotation>`.
- `Parameter::typeName` to `std::optional<TypeAnnotation>`.
- `FunctionStmt::returnTypeName` to `std::optional<TypeAnnotation>`.
- `FunctionExpr::returnTypeName` to `std::optional<TypeAnnotation>`.

AST printing should preserve current simple annotation output and print function annotations compactly:

```text
Let pred: fun(number): bool = (fun (x: number): bool (return (> x 0)))
Fun apply(f: fun(number): number, x: number): number
```

Exact indentation should continue to match the current AST printer style.

## Static Type Model

The current checker stores function metadata as `StaticType::Function` plus optional arity and return type. This phase should replace the ad hoc pair with a function signature value that can carry parameter types and return type.

Suggested internal model:

```cpp
struct StaticFunctionType {
    std::vector<StaticTypeInfo> parameterTypes;
    std::unique_ptr<StaticTypeInfo> returnType;
};

struct StaticTypeInfo {
    StaticType kind;
    std::optional<StaticFunctionType> function;
};
```

`StaticType::Function` remains useful as the top-level kind. Function metadata is meaningful only when `kind == StaticType::Function`.

Unknown remains a compatibility escape hatch:

- An unknown whole expression type is compatible with any expected type.
- A function parameter or return position whose type is unknown is compatible with known types.
- A function signature with missing metadata keeps existing arity-only behavior when arity is known.

## Compatibility Rules

### Simple values

Keep existing compatibility for simple known types:

- Same known simple types are compatible.
- Unknown expected or unknown actual is compatible.
- Different known simple types are incompatible.

### Function values

Two known function signatures are compatible when:

- Both are functions.
- Their arities match.
- Each corresponding parameter type is compatible.
- Their return types are compatible.

This first implementation may use invariant compatibility for known parameter and return types. That means `fun(number): number` is not compatible with `fun(string): number`, and `fun(number): string` is not compatible with `fun(number): number`.

### Initialization and assignment

Annotated `let` declarations check initializer compatibility against the declared type:

```cd
let f: fun(number): number = fun (x: number): number {
  return x + 1;
};
```

Reassignment of a function-typed variable checks the assigned value against the variable's known function signature:

```cd
let f: fun(number): number = fun (x: number): number { return x; };
f = fun (s: string): number { return 1; }; // type error
```

### Calls

If the callee has a known function signature:

- Check argument count.
- Check each argument expression against the corresponding parameter type.
- The call expression type is the signature return type.

```cd
fun apply(f: fun(number): number): number {
  return f(41);
}
```

Calling a value known not to be a function remains a type error.

### Function bodies and returns

Function declarations and function expressions should continue to use their parameter annotations as local binding types.

If the function has a function-typed return annotation, explicit and implicit returns are checked against that type:

```cd
fun make(): fun(number): number {
  return fun (x: number): number { return x + 1; };
}
```

Existing conservative implicit-`nil` behavior remains. A function annotated as returning a function type may not fall through implicitly.

## Diagnostics

Use the existing stable diagnostic format:

```text
Type error at <line>:<column>: <message>
```

Suggested messages:

- Unknown leaf annotation:

```text
Type error at 1:12: unknown type `foo`
```

- Function parameter mismatch on assignment:

```text
Type error at 2:1: cannot assign function with parameter type string to `f` of type function with parameter type number
```

- Function return mismatch on assignment:

```text
Type error at 2:1: cannot assign function returning string to `f` of type function returning number
```

- Call argument mismatch:

```text
Type error at 1:7: argument 1 expects number, got string
```

Exact wording may vary, but each stable behavior should be covered by type-error goldens.

## Parser Error Coverage

Add parse-error fixtures for malformed function type annotations, including:

- Missing `)` in a function type parameter list.
- Missing `:` return separator after a function type parameter list.
- Missing return type after `:`.

These should remain parse errors rather than type errors when the annotation shape is syntactically incomplete.

## Runtime and Backend Impact

No runtime behavior changes are required.

The IR compiler, IR interpreter, bytecode compiler, bytecode text emitter, and Rust VM should continue receiving the same AST-level executable program after type checking succeeds. Existing runtime parity tests should continue passing unchanged.

## Tests

Add success golden fixtures for:

- `let` binding with a function type annotation.
- Function parameter annotated with a function type and called inside the body.
- Function returning a function type.
- Nested function type annotation.

Add type-error fixtures for:

- Initializer function parameter type mismatch.
- Initializer function return type mismatch.
- Assignment function parameter type mismatch.
- Assignment function return type mismatch.
- Calling a function-typed parameter with the wrong argument type.
- Returning a non-function from a function annotated with a function return type.

Run the standard project verification set before completion.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` to document recursive `typeExpr` grammar.
- `README.md` to document function type annotations and remaining type-system limits.
- `docs/roadmap.md` to mark this Phase 9 slice implemented after code and tests land.
- `AGENTS.md` current language notes if the user-visible semantics summary changes.
