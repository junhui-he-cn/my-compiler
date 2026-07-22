# Array `flatMap` Implementation Plan

**Goal:** Add `flatMap(array, callback)` and `array.flatMap(callback)` with
one-level flattening, source snapshots, and C++ compiler/Rust VM parity.

**Architecture:** Register `flatMap` as a shadowable native function and route
both forms through the existing `native_call` path. Reuse the array callback
type-checking conventions, then execute callback results as arrays in the Rust
VM without adding opcodes.

## Tasks

- [x] Add focused success, shadowing, static-error, and runtime-error fixtures.
- [x] Register and statically type `flatMap` for function and member forms.
- [x] Lower member calls and execute one-level flattening in the Rust VM.
- [x] Add Rust unit coverage and generate compiler/artifact goldens.
- [x] Update `README.md`, `AGENTS.md`, `docs/roadmap.md`, and the bytecode
      native list.
- [x] Run the complete repository verification suite and review `git diff`.
