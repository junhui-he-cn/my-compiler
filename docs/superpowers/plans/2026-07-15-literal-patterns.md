# Primitive Literal Patterns Implementation Plan

1. Add success and type-error fixtures for primitive, nullable primitive, and
   nested enum literal patterns.
2. Extend type-checker pattern validation and exhaustive coverage tracking for
   primitive match domains while preserving enum and guard behavior.
3. Update grammar, README, AGENTS semantics, roadmap, golden outputs, bytecode
   artifacts, and Rust VM parity coverage.
4. Run the complete verification suite, remove generated Python caches, commit
   the focused slice, and push it to `origin/master`.
