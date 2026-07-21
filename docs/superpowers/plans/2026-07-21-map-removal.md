# Map Removal Implementation Plan

## Tasks

- [x] Add red success, type-error, runtime-error, Rust unit, and bytecode
  artifact fixtures for `remove` and `map.remove`.
- [x] Register `remove`, add static map/key/result checks, and mark `remove` as
  an unshadowed builtin member call.
- [x] Lower both forms through the existing `native_call` path.
- [x] Implement ordered shared-entry removal and diagnostics in the Rust VM.
- [x] Generate/review AST, IR, bytecode, `.cdbc`, and Rust VM outputs.
- [x] Update `README.md`, `docs/language-grammar.ebnf` only if grammar text
  needs a builtin mention, `docs/roadmap.md`, `AGENTS.md`, and
  `docs/bytecode-text-format.md`.
- [ ] Run the focused and full verification commands, remove Python caches, and
  inspect `git diff --check` and final branch status.
