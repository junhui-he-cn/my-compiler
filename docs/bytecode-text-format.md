# Compiler Design ByteCode Text Format

This document describes the planned stable text artifact format for Compiler Design bytecode files.

The file extension is `.cdbc`, short for Compiler Design ByteCode.

This format is not the same as the current `--bytecode` debug print. The debug print is for humans inspecting compiler output. The `.cdbc` format is a future compiler/VM contract that must be stable, versioned, and parseable by the Rust VM.

## Phase Status

Phase 0 documents the format direction only. The C++ compiler does not emit `.cdbc` files yet, and the Rust VM does not parse or execute them yet.

## Header

Every file starts with a format identifier and version:

```text
cdbc 0.1
```

Future format changes must either remain backward-compatible with `0.1` or use a new version number.

## Sections

A `.cdbc` file is organized into explicit sections:

```text
cdbc 0.1

constants:
  c0 = number 1
  c1 = string "hello"

names:
  n0 = "x"

main registers=3:
  r0 = constant c0
  store_var n0, r0
  r1 = load_var n0
  print r1

function f0 name="add_one" arity=1 registers=4:
  r1 = constant c0
  r2 = add r0, r1
  return r2
```

The exact grammar will be finalized when the C++ emitter and Rust parser are implemented. The section names and reference prefixes are reserved by this plan.

## Value Encoding

Constants use explicit value tags:

```text
c0 = nil
c1 = number 1.25
c2 = bool true
c3 = string "escaped string"
```

Strings use double quotes and backslash escapes for at least `\\`, `\"`, `\n`, `\r`, and `\t`.

## References

References use stable prefixes:

- `cN`: constant index.
- `nN`: name index.
- `rN`: register index.
- `fN`: function index.

Indexes are zero-based decimal integers.

## Opcode Names

The planned opcode names are stable snake-case names:

```text
constant
make_function
array
move
load_var
store_var
assign_var
call
index
assign_index
len
print
return
negate
not
add
subtract
multiply
divide
equal
not_equal
greater
greater_equal
less
less_equal
jump
jump_if_false
jump_if_true
```

New opcodes must be added by updating this document, the C++ bytecode artifact emitter, and the Rust VM parser/executor together.

## Non-Goals for Phase 0

Phase 0 does not define a complete parser grammar, binary encoding, verifier, execution semantics, GC layout, task scheduler, or JIT metadata format. Those belong to later Rust VM phases.
