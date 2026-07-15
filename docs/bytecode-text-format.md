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

debug_sources:
  s0 path="examples/hello.cd" text="print 1;\n"

debug_locations:
  main 0 = s0:1:7
  main 1 = s0:1:1
```

The section names and reference prefixes are part of the canonical text format. Function `param` lines, when present, appear before instructions in a function section.

## Debug metadata

`debug_sources` and `debug_locations` are optional additive sections. The C++
compiler emits them for source-backed instructions, and the Rust VM uses them
to report runtime source locations, source lines, carets, and call stacks. Each
`debug_sources` entry is ordered by zero-based `sN` index and embeds the display
path plus original source text. Each location maps a section and instruction
index to `sN:line:column`, using one-based source coordinates:

```text
debug_sources:
  s0 path="lib.cd" text="fun fail() { return 1 / 0; }\n"

debug_locations:
  main 3 = s0:2:1
  function f0 2 = s0:1:21
```

`main` identifies the top-level body; `function fN` identifies a function
section. Locations are sparse, but every referenced source, function, and
instruction must exist. Source, function, and instruction references are
zero-based; line and column values are one-based and must be positive. Duplicate
mappings and out-of-range references are Rust parser errors. A metadata-free
`cdbc 0.1` artifact remains valid and executes with
legacy one-line runtime errors.

## Value Encoding

Constants use explicit value tags:

```text
c0 = nil
c1 = number 1.25
c2 = bool true
c3 = string "escaped string"
```

Strings use double quotes and backslash escapes for at least `\\`, `\"`, `\n`, `\r`, and `\t`.

String constants are UTF-8 text. The Rust VM's `len`, `substr`, and `charAt`
operations interpret string offsets as Unicode scalar-value positions and
never split a scalar's UTF-8 encoding. Grapheme segmentation and normalization
are language-level non-goals for this format version.

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
map
struct
variant
variant_tag
variant_field
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

Map construction preserves source order and uses explicit key/value register
pairs:

```text
rD = map [rKey0: rValue0, rKey1: rValue1, ...]
```

The Rust parser rejects malformed entries that do not contain a `: ` pair
separator. Map lookup and assignment reuse the existing `index` and
`assign_index` instructions.

Struct and field instructions use name-table references for field names:

```text
rD = struct {nName: rValue, ...}
rD = struct nType {nName: rValue, ...}
rD = field rObject, nName
rD = assign_field rObject, nName, rValue
```

The optional `nType` name-table reference records a named struct runtime type name for `typeOf`. Anonymous bytecode struct instructions omit it and continue to report `"struct"` when executed by the VM.

`assign_field` mutates an existing struct field and stores the assigned value in `rD`; assigning to a missing field is a runtime error.

Enum variants use two name-table references and an ordered payload register list:

```text
rD = variant nEnum.nVariant [rPayload0, rPayload1, ...]
rD = variant_tag rValue nEnum.nVariant
rD = variant_field rValue payloadIndex
```

`variant_tag` returns a boolean and is false for non-matching values.
`variant_field` reads a positional payload and raises a runtime error for
non-variant values or an out-of-range payload index.

Generic enum type arguments are compile-time metadata and are erased from
these runtime instructions; the emitted enum name and payload layout remain
the same as for non-generic enums.

Native stdlib calls use a name-table reference for the function name:

```text
rD = native_call nName [rArg0, rArg1, ...]
```

`native_call` invokes a registered VM native stdlib function by name-table reference; in this version `push`, `pop`, `floor`, `ceil`, `sqrt`, `str`, `substr`, `charAt`, `typeOf`, `contains`, `slice`, `copy`, and `concat` are supported.

The `range` native is also supported with one to three numeric arguments. Its
result is consumed by the existing `len`, `index`, and `assert_array`
instructions; `assert_array` accepts both arrays and ranges for compatibility
with existing artifacts.

New opcodes must be added by updating this document, the C++ bytecode artifact emitter, and the Rust VM parser/formatter and executor together.

## Non-Goals for This Phase

This format does not define binary encoding, verifier internals, GC layout, task scheduler, or JIT metadata format. Those belong to later Rust VM phases.
