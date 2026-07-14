# Generic Method Type Abstraction Design

Date: 2026-07-14
Status: approved for implementation planning

## Goal

Extend the existing named-struct method slice with generic methods that use the
same inference and explicit type-argument rules as generic named functions.
Generic method type arguments are compile-time metadata only; methods remain
ordinary runtime functions receiving the receiver as their first argument.

## User-visible syntax and scope

Methods in an `impl` block may declare comma-separated type parameters after
the method name:

```cd
struct Box { value: number }

impl Box {
  fun echo<T>(value: T): T {
    return value;
  }
}

let box: Box = Box { value: 0 };
print box.echo(42);          // inferred number
print box.echo<string>("x"); // explicit string
```

The existing type-annotation forms remain available inside generic method
signatures, including arrays, maps, nullable types, function types, and named
or qualified struct/enum types. A method type parameter is scoped to that
method's parameter annotations, return annotation, and body annotations.
Duplicate names in one method declaration are type errors. A type parameter is
not a runtime value and cannot be used in a declaration outside the method.

The slice covers local methods, methods reached through direct imports,
namespace imports, and re-exported structs. Those paths already share method
metadata and call lowering, so they must preserve the same generic signature.
Generic lambdas, generic structs, constraints, method overloading, and taking a
method as a standalone function value remain out of scope.

## Static semantics

Generic method calls use the existing `MemberCallExpr.typeArguments` syntax.
The receiver is checked first using the existing named-struct method lookup;
only explicit arguments after the receiver are passed to generic call
checking. For a method with parameter types `P`, return type `R`, and type
parameters `T`, the checker builds the equivalent erased signature
`fun<T>(P): R`, then reuses the generic call helper:

1. Check method arity.
2. Check each argument without treating an unsubstituted type parameter as a
   concrete context.
3. Infer or explicitly bind every method type parameter recursively through
   array, map, nullable, and function shapes.
4. Substitute the method parameters and return type.
5. Apply the existing compatibility and argument diagnostic rules.

Inference is per call. Repeated occurrences of a type parameter must infer
mutually compatible concrete types. Unknown arguments do not produce a
candidate, and a parameter with no candidate reports `cannot infer type
parameter T`. Explicit type arguments replace inference and must provide one
argument for every declared method type parameter.

Generic method bodies are checked while the method type-parameter scope is
active. An unconstrained type parameter remains opaque: numeric operators,
indexing, and field access continue to reject it through the existing static
kind checks. Return checking compares the inferred body type with the
substituted or generic annotation exactly as it does for generic functions.

## Type and module metadata

`MethodDecl`, `MethodInfo`, and `MethodSignature` carry ordered generic
parameter names. `ModuleInterfaceMethod` carries the same names so interface
text prints `method echo<T>(T): T`. Direct imports, namespace qualification,
and re-exports copy the metadata without specializing it.

The IR compiler, bytecode compiler, `.cdbc` format, and Rust VM do not receive
the type arguments. The method's implicit receiver and ordinary runtime
arguments retain their current order and arity.

## Diagnostics

Use the existing Type diagnostic category and call-site locations:

- duplicate method type parameters report `duplicate type parameter \`T\``;
- conflicting inference reports `type parameter T inferred as number and
  string`;
- missing inference reports `cannot infer type parameter T`;
- a non-generic method with explicit arguments reports `function is not
  generic`;
- wrong explicit-argument counts and post-substitution argument mismatches
  use the existing generic function messages.

## Testing strategy

Add AST and runtime goldens for local generic methods, including array-shaped
parameters, inferred calls with different concrete types, and explicit
return-only type parameters. Add a module fixture that calls an exported
generic method directly and through a namespace, and verify its module
interface text.

Add type-error fixtures for duplicate parameters, conflicting inference,
missing inference, explicit argument mismatch, and explicit arguments on an
ordinary method. Add a bytecode artifact fixture proving method calls remain
ordinary calls and retain Rust VM parity. Existing struct method, import,
re-export, golden, artifact, and Cargo suites remain completion gates.

## Non-goals and follow-up boundaries

- Generic function expressions or lambdas.
- Generic struct declarations or generic enum declarations.
- Constraints, traits/protocols, variance, overloads, or specialization.
- Method values, dynamic dispatch, inheritance, and static methods.
- New IR operations, bytecode opcodes, artifact sections, or VM type tags.
