# Generic Struct Type Plan

**Goal:** implement nominal generic structs as a static-only container form,
with field substitution and erased runtime representation.

## Tasks

1. Add type parameters to struct declarations and explicit type arguments to
   struct constructor AST nodes; update parser, AST printing, and grammar.
2. Extend `TypeInfo`, struct declarations, module interfaces, and namespace
   qualification to carry generic struct arguments and bounds.
3. Resolve generic struct annotations and constructors, infer arguments from
   fields/expected types, validate bounds, and substitute field types in
   constructor, access, assignment, and record-pattern checks.
4. Preserve direct/namespace import metadata and reject `impl` blocks on generic
   structs until receiver type-parameter binding is designed.
5. Add success, parse-error, and type-error goldens plus module-interface and
   unit coverage; update README, AGENTS, roadmap, and grammar.
6. Run the complete C++/golden/bytecode/Rust VM/Cargo verification suite and
   commit the slice.

## Boundaries

- Do not add map APIs.
- Do not change runtime values, IR, bytecode, `.cdbc`, or `vm-rs` execution.
- Do not implement recursive structs, protocols, inheritance, or generic
  receiver methods in this slice.
