# Runtime Source Diagnostics and Call Stacks Design

## Goal

Make Rust VM runtime failures report the originating source location, source
line, caret, and language-level call stack. Preserve that information from
parsed source through AST, IR, C++ bytecode, emitted `.cdbc`, Rust parsing, and
execution. A standalone VM must diagnose an emitted artifact without relying on
the original source files remaining on disk.

## Scope

This slice covers runtime failures raised while executing compiler-emitted
bytecode, including invalid dynamic operations, native-stdlib failures, and
errors reached through nested or closure calls. It covers stdin, one or more
direct files, and imported files.

It does not add a C++ bytecode interpreter, source-level stepping, exception
handling, debugger commands, multi-error runtime reporting, or source-map
compression. Parse, type, compile, and import diagnostics retain their current
format and behavior.

## User-visible diagnostics

When an error has debug metadata, the VM writes the following stable shape to
stderr and exits with code 1:

```text
Runtime error at path/to/file.cd:3:14: division by zero
  return value / 0;
               ^
Call stack:
  at divide (path/to/file.cd:3:14)
  at main (path/to/file.cd:7:7)
```

For stdin, the location is rendered as `<stdin>:line:column`. The first stack
frame is the instruction that failed. Each following frame is the call-site
instruction in its caller. Frames are ordered innermost to outermost and use
the compiled function name; the top-level frame is named `main`. Runtime errors
raised by native helpers are attributed to their `native_call` instruction.

Source snippets come from text embedded in the artifact. The VM prints the
single source line and a caret aligned to the stored one-based column. It never
re-reads the path from the host filesystem. Locations whose line or column is
invalid, and manually written artifacts that omit a location, fall back safely
to the existing first line:

```text
Runtime error: division by zero
```

Such artifacts may still show a `Call stack:` section only for frames with a
valid location; a fully metadata-free legacy artifact retains the old one-line
diagnostic exactly.

## Source provenance

`Token` gains an optional source identity in addition to its existing one-based
line and column. The front-end assigns an identity to each input unit. For
module loads, that identity belongs to the parsed module; for direct multi-file
input, the frontend maps the existing combined parse coordinates back to the
particular direct input before IR generation. Stdin is a single source named
`<stdin>`.

Expressions and statements receive a source span anchored at their syntactic
start token. The parser assigns the span while constructing each concrete AST
node. Compiler-generated epilogue instructions (the implicit `return nil`) use
the declaring function's `fun` token; module-generated or otherwise synthetic
instructions without a meaningful origin intentionally have no span.

The `Program` retains a source-file table containing display path and original
source text. `ModuleStmt` continues to own source text for module handling, but
the program-level table is the single source of truth consumed by IR lowering.
Direct multi-file programs retain a table entry per command-line input even
when they compile in one shared top-level scope.

## Compiler and bytecode metadata

Introduce a shared C++ `SourceSpan` value with a source-file index, line, and
column. `IRInstruction` and `BytecodeInstruction` each carry an optional span.
`IRCompiler` sets the current span immediately before lowering a source AST
node, so every emitted instruction that implements that node receives the same
anchor. Nested expression lowering temporarily replaces and restores the
current span, letting an arithmetic, call, index, field, or assertion failure
point to its own source expression rather than the surrounding statement.

`IRProgram` carries the source-file table. `BytecodeCompiler` preserves both
the table and every instruction span. Existing human-readable `--ir` and
`--bytecode` output stays unchanged: source locations are debug data, not a
golden-visible assembly syntax change.

## `.cdbc` debug format

The existing instruction grammar remains unchanged. Add an optional section
after all function bodies:

```text
debug_sources:
  s0 path="path/to/file.cd" text="let x = 1;\\n"
  s1 path="<stdin>" text="print 1 / 0;\\n"

debug_locations:
  main 0 = s0:1:9
  function f0 3 = s0:4:12
```

`debug_sources` is omitted when the compiler has no source table. Entries are
ordered by source-file index and use the existing quoted-string escaping rules.
`debug_locations` is omitted when no instruction has a span. Each mapping is
ordered by execution section then instruction index; `main` names the top-level
body and `function fN` names a function table entry. A mapping must reference a
defined source and an existing instruction in its section. Duplicate mappings,
out-of-range indices, malformed source references, and invalid zero line or
column values are parse errors for the Rust artifact parser.

The Rust formatter emits these optional sections canonically and parses legacy
`cdbc 0.1` artifacts that lack them. This is an additive format extension
without changing the header version because old artifacts remain valid and
new debug sections do not change instruction semantics.

## Rust VM runtime model

Replace the message-only runtime error payload with a message plus optional
failure location and a vector of stack frames. A frame contains a function name
and optional call-site location. `execute_body` associates any error escaping
an instruction with that instruction's location if it lacks one. `call_function`
wraps an escaping callee error by appending the caller's `call` instruction
location and the caller function name. Main is represented as the outermost
frame when an error escapes top-level execution.

The VM never overwrites a more precise location attached by an inner operation.
For example, an `index` error keeps the index expression's span, and an error
from `sqrt` is given the native call's span before the caller frames are added.
Errors raised while validating malformed bytecode continue to execute through
the same wrapper and acquire a location when the current instruction has one.

The display implementation selects the embedded `DebugSource` from the
location, extracts its line without panicking, renders the caret, and then
renders the call stack. It has no dependency on the C++ compiler or source
filesystem.

## Error handling and compatibility

- A compiler-produced program always emits debug sources and mappings for
  source-backed instructions.
- A hand-authored artifact may omit both debug sections and remains executable.
- Debug metadata errors are rejected by the Rust parser before VM execution.
- Runtime failure remains stderr-only; successful execution output is unchanged.
- Existing runtime-error goldens intentionally change only after their new
  location and stack output is reviewed.

## Tests

### C++ compiler and artifact tests

Add fixtures that emit bytecode for a direct file, a two-file direct program,
and an imported module. Assert canonical debug-source entries and source
mappings while confirming the existing instruction text remains stable.

### Rust parser and formatter tests

Add unit tests that parse and canonically format both populated debug sections,
reject invalid source/instruction references and zero coordinates, and preserve
a legacy artifact with neither section.

### Rust VM tests

Add focused unit tests for a failing main instruction, a native-call failure, a
two-function call chain, a closure call chain, missing debug metadata, and
invalid source-line lookup. Assert the exact diagnostic text, including frame
order and caret alignment.

### End-to-end goldens

Add runtime-error fixtures for direct files, nested functions, and imports.
Run them through `tests/run_rust_vm_tests.py --goldens` so emitted artifacts and
the standalone Rust VM are tested together. Refresh each affected existing
runtime-error expected file deliberately, then run the complete project
verification suite specified in `AGENTS.md`.

## Documentation

Update `docs/bytecode-text-format.md` with the optional debug sections and
validation rules. Update `README.md` to describe runtime source diagnostics and
call stacks. Update `docs/roadmap.md` by marking the runtime-diagnostics step
complete only after the feature and full verification suite are complete.
