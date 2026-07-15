# Generic Enums Implementation Plan

1. Extend enum AST/parser/type annotations and static type metadata with
   generic declaration and instantiation arguments.
2. Add checker substitution/inference for enum declarations, constructors,
   recursive payloads, nullable enum matches, and patterns.
3. Thread generic enum metadata through module interfaces/imports while
   keeping runtime names and bytecode erased.
4. Update grammar, README, AGENTS, roadmap, goldens, artifacts, and Rust VM
   parity fixtures.
5. Run the complete repository verification suite, then commit and push.
