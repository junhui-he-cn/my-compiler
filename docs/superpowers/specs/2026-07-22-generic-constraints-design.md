# Generic Type Parameter Constraints Design

Date: 2026-07-22

## Goal

Add a small, statically checked bound syntax for the generic declarations that
already exist in the language. A declaration may write `T: type` in its type
parameter list, and every explicit or inferred instantiation must satisfy that
bound.

Examples:

```cd
fun identity<T: number>(value: T): T { return value; }
fun<T: number>(value: T): T { return value; }
enum Box<T: number> { Value(T) }
```

## Scope

- Named functions, named-struct methods, anonymous function expressions, and
  generic enum declarations accept constrained type parameters.
- Bounds are ordinary existing type annotations, including primitive,
  nullable, array, map, struct, and instantiated enum types.
- A bound must resolve to a concrete type and may not contain a type parameter.
  Higher-kinded bounds, unions, protocol/trait bounds, and inferred bounds are
  out of scope.
- Explicit type arguments and inferred arguments use the same compatibility
  rule: `T: Bound` accepts an actual type when `compatible(Bound, actual)` is
  true. An outer type parameter is accepted when its own concrete bound
  satisfies the required bound.
- Generic callback specialization for existing array helpers reuses the same
  constraint validation.
- Generic parameters and their bounds remain type-checker metadata and are
  erased before IR lowering; no new IR, bytecode, or VM behavior is required.

## Representation

AST type parameters carry an optional `TypeAnnotation` bound. `TypeInfo`
function signatures, enum declarations, method signatures, and module
interfaces carry bounds in declaration order alongside their generic names.
Type-parameter values retain their own concrete bound so nested generic calls
can validate an outer parameter without losing information.

## Diagnostics

Constraint failures are static type errors at the call or constructor token:

```text
type error: type parameter T must satisfy number, got string
```

Malformed syntax remains a parse error. A bound containing a type parameter is
rejected as an unsupported non-concrete constraint.

## Non-goals

- No new map API or collection operation.
- No generic structs or new generic container syntax.
- No operator overloading or automatic treatment of a constrained `T` as a
  primitive inside the generic body; bounds constrain instantiation only.
- No changes to runtime values, native calls, bytecode artifacts, or the Rust
  VM.

## Verification

Cover inferred and explicit function calls, generic lambdas, methods, generic
enum constructors, constrained array callbacks, nested outer type parameters,
invalid concrete arguments, non-concrete bounds, imported method metadata, AST
printing, and module-interface output. Existing unconstrained generic fixtures
must remain byte-for-byte stable.
