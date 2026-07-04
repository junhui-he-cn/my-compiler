# Compiler Design VM

`compiler-design-vm` is the planned standalone Rust bytecode VM for Compiler Design.

Phase 0 status: this crate is a project skeleton and CLI placeholder. It does not parse `.cdbc` files and does not execute bytecode yet.

## Current Commands

```sh
cargo run --manifest-path vm-rs/Cargo.toml -- --help
```

## Planned Commands

```sh
compiler-design-vm run program.cdbc
compiler-design-vm dump program.cdbc
```

`run` will execute a stable Compiler Design ByteCode artifact. `dump` will parse and print a normalized view of that artifact for debugging and golden tests. These commands are part of future phases.

## Future Module Boundaries

- `format`: `.cdbc` parser and serializer.
- `bytecode`: bytecode structures and validation.
- `value`: runtime values.
- `vm`: executor.
- `heap`: GC-aware heap ownership and root scanning.
- `scheduler`: task scheduling, instruction budgets, and yield points.
- `jit`: JIT metadata and native-code experiments.
