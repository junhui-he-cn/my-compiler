# Generic Function Type Abstraction Design

Date: 2026-07-13  
Status: approved for implementation planning

## Goal

Introduce a small, statically meaningful generic-function layer before adding
higher-order collection APIs, maps, or ranges. A generic function is checked
once and remains one runtime function; type arguments exist only in the C++
type-checking and module-interface layers.

The first slice intentionally supports type-parameter declaration and call-site
inference without explicit type arguments, constraints, overloads, or generic
containers beyond the existing structural array type `[T]`.

## User-visible syntax and scope

Named function declarations may declare comma-separated type parameters after
the function name:

```cd
fun identity<T>(value: T): T {
  return value;
}

print identity(42);       // 42
print identity("hello");  // hello
```

The parser reuses the existing `<` and `>` tokens. Calls do not accept explicit
type arguments in this slice, so `identity<number>(42)` is not valid syntax;
the existing call grammar remains unambiguous.

Type-parameter names are identifiers scoped to the declaring function's
parameter annotations, return annotation, and body annotations. They may occur
recursively inside array (`[T]`), function (`fun(T): T`), and nullable (`T?`)
annotations. A type parameter used outside its declaring generic function is an
unknown type error. Duplicate names in one declaration are type errors. Generic
methods and generic function expressions/lambdas are out of scope; ordinary
named functions and methods retain their existing behavior.

Nested function declarations do not capture an enclosing function's type
parameters in their own signatures. A nested declaration that needs a type
parameter must declare it explicitly; referencing an outer `T` from a nested
signature is rejected as an escaping type parameter.

Generic type parameters do not become runtime values. The IR compiler, bytecode
compiler, `.cdbc` format, and Rust VM continue to see one ordinary function
with the same arity and dynamic runtime values as before.

## Type representation

`TypeInfo` gains two pieces of metadata:

- a `TypeParameter` kind carrying its parameter name (for example `T`), and
- an ordered `genericParameters` list on function types (for example
  `fun<T>(T): T`).

Recursive type information keeps working: `[T]`, `T?`, and
`fun(T): [T]` retain the type variable in their child `TypeInfo` nodes.
`typeInfoName` and module-interface output print generic signatures in a stable
form. Existing structural array and nominal struct compatibility rules remain
unchanged for concrete types.

Inside a generic function body, two references to the same type parameter are
compatible with each other. A type parameter is not assumed to be a number,
string, array, or struct, so operations that require a concrete kind (numeric
operators, field access, and indexing) continue to reject an unconstrained
parameter.

## Inference and substitution

When a call targets a generic function with a complete signature, the type
checker:

1. Checks arity.
2. Checks each argument without treating a type variable as a concrete
   contextual type, preserving the argument's inferred `TypeInfo`.
3. Recursively unifies each parameter type with the corresponding argument type.
   A `T` occurrence records a concrete candidate; array, nullable, and function
   shapes recurse into their child types.
4. Ignores `Unknown` candidates for inference. A repeated parameter must receive
   mutually compatible concrete candidates; conflicting candidates produce a
   Type error naming the parameter and the two inferred types.
5. Requires every declared type parameter to have a candidate. A parameter that
   appears only in a return type, or whose only arguments are unknown, reports
   `cannot infer type parameter T` at the call.
6. Substitutes the inferred map through all parameter and return `TypeInfo`
   nodes, then runs the existing assignment-compatibility checks against the
   substituted parameter types.

`nil` is a concrete `nil` candidate when it appears as an argument. Nullable
parameters still use the existing nullable compatibility rules after
substitution. No special lower/upper-bound or least-common-supertype inference
is introduced; incompatible concrete candidates are an error rather than an
automatic union.

An unannotated `let` binding initialized from a generic function retains its
generic function signature, so an alias can be called and inferred later:

```cd
let copy = identity;
print copy(true);
```

Assigning a generic function directly to an explicitly monomorphic function
annotation is not part of this slice; such an assignment reports a type error
instead of silently specializing the value. Explicit type arguments are the
future escape hatch for APIs that need this distinction.

## Modules and names

Generic function `TypeInfo` travels through existing module value exports,
direct imports, namespace imports, and re-exports. A call through an imported
name or namespace uses the same inference/substitution helper as a local call.
Module interface text prints the generic function signature, but no new module
metadata section or linker behavior is required.

## Errors and diagnostics

Existing diagnostic categories and source snippets remain unchanged:

- Duplicate type-parameter names report a Type error at the duplicate name.
- A type parameter outside its function scope reports the existing unknown-type
  Type error.
- Conflicting inference reports a Type error at the call, for example
  `type parameter T inferred as number and string`.
- Missing inference reports `cannot infer type parameter T`.
- After substitution, ordinary argument mismatches continue to use the existing
  `argument N expects <type>, got <type>` wording.
- Runtime behavior and runtime diagnostics do not change because generics are
  erased before IR/bytecode execution.

## Testing strategy

Tests are vertical and begin with failing type-checker/parser coverage:

1. AST output covers `fun identity<T>(...)` and nested `[T]` annotations.
2. Success goldens exercise identity, array-head inference, repeated calls with
   different concrete types, unannotated generic aliases, and a generic
   function imported both directly and through a namespace.
3. Type-error goldens cover duplicate parameters, use of `T` outside scope,
   conflicting repeated inference, and an uninferred return-only parameter.
4. The artifact fixture proves generic declarations emit the same ordinary
   function bytecode shape and that C++ artifact output executes identically in
   the Rust VM; no VM code changes are expected.
5. Focused C++ unit/golden tests cover `TypeInfo` formatting, recursive
   substitution, and compatibility. Existing CTest, golden, artifact, Rust VM,
   and Cargo suites remain the completion gate.

## Non-goals and follow-up boundaries

- Explicit call-site type arguments (`f<number>(...)`).
- Generic methods, generic lambdas, generic structs, and a new `Array<T>`
  container syntax; existing `[T]` remains the only generic-shaped container.
- Type constraints, traits/protocols, variance, overload resolution, and
  higher-kinded types.
- Callback-based `map`, `filter`, and `reduce`; those remain deferred until
  generic function and collection inference has a separate design.
- Runtime specialization, monomorphization, new bytecode opcodes, or VM type
  tags.
