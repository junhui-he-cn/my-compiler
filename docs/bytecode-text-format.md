# Compiler Design ByteCode Text Format

This document describes the stable text artifact format for Compiler Design bytecode files.

The file extension is `.cdbc`, short for Compiler Design ByteCode.

This format is not the same as the current `--bytecode` debug print. The debug print is for humans inspecting compiler output. The `.cdbc` format is the compiler/VM contract: stable, versioned, and parseable by the Rust VM.

## Phase Status

This format is the text artifact contract at the compiler/VM boundary. The C++ compiler can emit `.cdbc` files with `--emit-bytecode`, and the Rust VM can parse, canonicalize, and execute them with `dump` and `run`.

```sh
compiler_design --emit-bytecode output.cdbc input.cd
compiler-design-vm dump output.cdbc
```

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

The section names and reference prefixes are part of the canonical text format. Function `param` lines, when present, appear before instructions in a function section.

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

The opcode names are stable snake-case names:

```text
constant
make_function
array
struct
move
load_var
store_var
assign_var
call
native_call
index
assign_index
field
assign_field
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

Struct and field instructions use name-table references for field names:

```text
rD = struct {nName: rValue, ...}
rD = field rObject, nName
rD = assign_field rObject, nName, rValue
```

`assign_field` mutates an existing struct field and stores the assigned value in `rD`; assigning to a missing field is a runtime error.

Native stdlib calls use a name-table reference for the function name:

```text
rD = native_call nName [rArg0, rArg1, ...]
```

`native_call` invokes a registered VM native stdlib function by name-table reference; in this version `push` and `pop` are supported.

New opcodes must be added by updating this document, the C++ bytecode artifact emitter, and the Rust VM parser/formatter and executor together.

## Non-Goals for This Phase

This format does not define binary encoding, verifier internals, GC layout, task scheduler, or JIT metadata format. Those belong to later Rust VM phases.
