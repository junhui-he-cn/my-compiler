# Generic Struct Method Plan

**Goal:** bind generic struct receiver parameters in `impl` blocks and
specialize method signatures at known receiver call sites.

## Tasks

1. Extend `ImplStmt` and the grammar/parser/AST printer with an optional
   receiver type-parameter list.
2. Register generic receiver methods with a `Box<T>` receiver signature,
   inherit struct bounds in method bodies, and reject malformed headers or
   receiver/method parameter name collisions.
3. Specialize receiver type parameters before existing generic method-call
   inference, including imported and namespace-qualified method signatures.
4. Add local, module-interface, direct-import, namespace-import, re-export,
   and type-error golden coverage; update docs and repository behavior notes.
5. Run focused tests and the complete C++/golden/bytecode/Rust VM/Cargo suite,
   clean generated files, and commit the slice.

## Boundaries

- Do not add map APIs.
- Do not change runtime values, IR, bytecode, `.cdbc`, or `vm-rs` execution.
- Do not implement recursive structs, inheritance, overloads, or dynamic
  dispatch.
