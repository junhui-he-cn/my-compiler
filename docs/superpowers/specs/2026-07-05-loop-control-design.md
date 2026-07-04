# Phase 11A Loop Control Design

## Goal

Add `break;` and `continue;` statements for existing `while` loops.

This phase should make current loops practical without adding `for` syntax yet:

```cd
let i = 0;
while i < 10 {
  i = i + 1;
  if i == 3 {
    continue;
  }
  if i == 8 {
    break;
  }
  print i;
}
```

Expected output:

```text
1
2
4
5
6
7
```

## User-Visible Semantics

- `break;` exits the nearest enclosing `while` loop.
- `continue;` skips the rest of the nearest enclosing `while` body and starts the next condition check.
- Nested loops target only the nearest loop.
- `break;` and `continue;` are statements only. They do not produce values and cannot appear inside expressions.
- Using `break;` or `continue;` outside a loop is a type error with the keyword source location.
- Function declarations and function expressions introduce independent control-flow scopes. A `break;` or `continue;` inside a nested function cannot target a loop outside that function body.

Example invalid program:

```cd
while true {
  fun f() {
    break;
  }
}
```

The `break;` is outside any loop in the nested function's own control-flow scope, so it is a type error.

## Grammar

Add two statement forms:

```ebnf
statement = printStmt
          | ifStmt
          | whileStmt
          | breakStmt
          | continueStmt
          | returnStmt
          | block
          | exprStmt ;

breakStmt    = "break", ";" ;
continueStmt = "continue", ";" ;
```

Add `break` and `continue` as reserved keywords. Existing identifiers named `break` or `continue` will become invalid as identifiers after this phase.

## AST and Printing

Add two statement nodes:

- `BreakStmt { Token keyword; }`
- `ContinueStmt { Token keyword; }`

AST output should follow existing compact statement style:

```text
(break)
(continue)
```

For example:

```cd
while true {
  break;
}
```

prints as a `while` whose block contains `(break)`.

## Type Checking

Track loop depth while checking statements.

- Entering a `while` body increments loop depth for that body and restores it afterward.
- `break;` and `continue;` require loop depth greater than zero.
- When checking any function body, reset loop depth to zero for that function's independent control-flow scope, then restore the outer depth after the function body is checked.
- Existing return-type inference and function arity tracking must continue to work unchanged.

Diagnostics should use existing type-error shape, for example:

```text
Type error at 1:1: `break` can only be used inside a loop
Type error at 1:1: `continue` can only be used inside a loop
```

Exact wording may vary slightly, but should be stable and covered by type-error goldens.

## IR Lowering

No new IR opcodes are needed. Lower loop control into existing `Jump` instructions.

For each `while` lowering, maintain a loop context:

- `continueTarget`: the instruction index at the start of the condition check.
- `breakJumps`: placeholder `Jump` instruction indexes emitted for `break;` statements in this loop.

Lowering outline:

1. Record `loopStart = instructionCount()` before compiling the condition.
2. Compile condition and emit `JumpIfFalse` placeholder for normal exit.
3. Push loop context with `continueTarget = loopStart` and empty `breakJumps`.
4. Compile body.
5. Emit `JumpTo(loopStart)` for normal loop continuation.
6. Patch normal `JumpIfFalse` to the loop exit.
7. Patch all `breakJumps` for this context to the same loop exit.
8. Pop loop context.

Lower `continue;` as `JumpTo(current.continueTarget)`.

Lower `break;` as placeholder `Jump` appended to `current.breakJumps`; it is patched after the loop exit location is known.

The TypeChecker prevents loop-control statements outside loops before IR compilation. The IRCompiler may still report a compile error if it sees one without an active loop context as a defensive check.

## Bytecode and Rust VM

No bytecode opcode or `.cdbc` format changes are required because loop control lowers to existing IR `Jump` / bytecode `Jump` instructions.

The C++ bytecode compiler should continue translating IR jumps as before. Rust VM execution should work without VM semantic changes, but Rust VM parity tests should include at least one loop-control fixture to prove artifact execution matches `--run`.

## Tests

Add or update golden coverage:

### Success fixtures

1. `loop_break`
   - Verifies `break;` exits a `while` early.
   - Include `ast.out`, `ir.out`, `bytecode.out`, and `run.out` if practical.

2. `loop_continue`
   - Verifies `continue;` skips the rest of the current iteration and re-checks the condition.
   - Include `run.out`; add IR/bytecode output if concise enough.

3. `loop_control_nested`
   - Verifies nearest-loop targeting for nested loops.
   - Include `run.out`.

Ensure Rust VM parity covers at least one loop-control success fixture, either through `tests/run_rust_vm_tests.py --goldens` allowlist or an explicit `tests/bytecode_artifacts/` case.

### Type-error fixtures

1. `break_outside_loop.cd`
2. `continue_outside_loop.cd`
3. `break_inside_function_in_loop.cd`

Each should verify stderr and exit code with existing type-error fixture conventions.

### Parse-error fixtures

Optional, only if parser behavior needs explicit coverage:

- `break` without semicolon.
- `continue` without semicolon.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` for new statement forms and keywords.
- `README.md` to replace “not implemented yet” with the implemented semantics.
- `docs/roadmap.md` to mark Phase 11A implemented after the implementation lands.
- `AGENTS.md` current language semantics if needed.

## Non-Goals

This phase does not include:

- `for` loops.
- Labeled break or labeled continue.
- `break expression;` or loop expressions.
- `do while` loops.
- New IR or bytecode opcodes.
- Any VM scheduling or task behavior.

## Acceptance Criteria

- `break;` and `continue;` lex and parse as statements.
- AST printing includes stable `(break)` and `(continue)` nodes.
- Loop-control statements outside loops are type errors with source locations.
- Nested function bodies cannot break or continue an enclosing non-function loop.
- IR lowering uses existing jumps and handles nested loops correctly.
- `--run`, `--bytecode`, `.cdbc` artifact tests, and Rust VM execution agree for supported loop-control programs.
- README and grammar docs match the implemented language.
