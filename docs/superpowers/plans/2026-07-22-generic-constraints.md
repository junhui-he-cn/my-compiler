# Generic Type Parameter Constraints Plan

**Goal:** enforce concrete bounds on existing generic functions, methods,
lambdas, enums, and array callbacks without changing runtime lowering.

**Design:** add optional bounds to AST type parameters, preserve aligned bounds
in `TypeInfo`, enum/method metadata, and module interfaces, and validate both
explicit and inferred substitutions through one TypeChecker helper.

## Tasks

1. Extend `TypeParameter` AST data and parser syntax from `<T>` to
   `<T: type>`, preserving current output for unconstrained declarations.
2. Carry bounds through AST printing, language grammar, function signatures,
   enum declarations, method metadata, imported namespaces, and module-interface
   emission.
3. Add TypeChecker scope metadata for a type parameter's concrete bound and
   reject bounds that contain type parameters.
4. Validate generic substitutions in ordinary calls, enum constructors, and
   array callback specialization. Allow an outer type parameter when its own
   bound satisfies the required bound.
5. Add success and type-error fixtures for calls, lambdas, methods, enums,
   callbacks, nested bounds, and imported module interfaces.
6. Update `README.md`, `AGENTS.md`, and `docs/roadmap.md`; run the complete
   C++/golden/bytecode/Rust VM/Cargo verification suite and commit the slice.

## Explicit boundaries

- Do not add map APIs.
- Do not add generic structs, constraint operators, protocol types, or new
  runtime instructions.
- Do not make constrained type parameters usable as primitive operands inside
  generic bodies; only instantiation compatibility changes in this slice.
