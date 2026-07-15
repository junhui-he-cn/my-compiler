# Nullable Enum Patterns Implementation Plan

1. Add focused success and type-error fixtures for nullable enum `match`.
2. Extend `TypeChecker::checkPattern`, statement matching, and match
   expressions with nullable unwrapping, `nil` coverage, and guarded coverage
   rules.
3. Confirm the existing IR, bytecode, and Rust VM paths execute `nil` patterns
   without an opcode or artifact-format change.
4. Update grammar, README, AGENTS, roadmap, and module-interface coverage.
5. Refresh goldens/artifacts, run the complete repository verification suite,
   then commit and push the focused slice.
