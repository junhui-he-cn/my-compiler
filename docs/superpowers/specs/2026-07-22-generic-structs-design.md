# Generic Struct Type Design

Date: 2026-07-22

## Goal

Add the next generic container form after the existing built-in `map<K, V>`:
nominal generic structs with statically substituted field types.

```cd
struct Box<T> { value: T }

let inferred = Box { value: 1 };
let explicit: Box<number> = Box<number> { value: 2 };
print explicit.value;
```

## Static semantics

- Struct declarations may use the existing type-parameter syntax, including
  concrete bounds such as `struct Box<T: number> { value: T }`.
- Generic struct annotations require exactly one type argument per parameter,
  validate existing bounds, and are invariant in their type arguments.
- Constructors may infer type arguments from field values or from an expected
  `Box<...>` type. They may also provide all explicit type arguments before the
  field list, as in `Box<number> { ... }` and `lib.Box<number> { ... }`.
- Every declared field is still required exactly once and extra fields remain
  errors. Field expressions are checked against the substituted field type.
- Field reads, writes, compound assignment checks, and record-pattern payload
  checks use the instantiated field type.
- Generic struct runtime values remain the existing named struct values; type
  arguments are erased before IR lowering. `typeOf` therefore reports `Box`.
- Existing `impl` blocks on generic structs are rejected in this slice because
  the current method metadata has no receiver type-parameter binder. Generic
  methods on non-generic structs remain supported.
- Recursive struct fields, generic structs containing recursive references,
  generic struct inheritance, and generic container protocols remain out of
  scope.

## Modules

Struct generic parameters and bounds are preserved in module-interface output,
direct imports, namespace imports, and qualified constructors. Imported field
types are qualified in the same way as existing enum and function signatures.

## Non-goals

- No map API or map behavior change.
- No new IR, bytecode, artifact, or Rust VM instruction.
- No runtime specialization or generic struct type tags.

## Diagnostics and verification

Cover inferred and explicit constructors, expected-type inference, field reads
and writes, bounds, wrong type-argument arity, invariant assignment mismatch,
missing/extra fields, generic record patterns, namespace imports, module
interfaces, recursive-field rejection, and rejection of `impl` on a generic
struct. Existing non-generic struct outputs must remain unchanged.
