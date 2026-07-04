# Language Roadmap Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Archive the old mixed roadmap and replace `docs/roadmap.md` with a language-focused roadmap that makes language features, not VM backend work, the active next direction.

**Architecture:** This is a documentation-only change. Preserve the old roadmap verbatim in an archive file, rewrite the active roadmap around current language gaps and future language phases, and adjust AGENTS roadmap hints so future agents follow the new planning direction.

**Tech Stack:** Markdown documentation, git, existing CMake/Python verification commands.

---

## File Structure

Create:

- `docs/roadmap-archive-2026-07-04.md` — verbatim archived copy of the current roadmap before the refresh.

Modify:

- `docs/roadmap.md` — active language-focused roadmap.
- `AGENTS.md` — roadmap hints updated to prioritize language phases and keep backend VM work explicitly deferred.

Reference:

- `docs/superpowers/specs/2026-07-04-language-roadmap-refresh-design.md`
- `README.md`
- `docs/language-grammar.ebnf`

Do not modify compiler source, tests, grammar, or README in this phase.

---

### Task 1: Archive the Existing Roadmap

**Files:**
- Create: `docs/roadmap-archive-2026-07-04.md`

- [ ] **Step 1: Confirm the active roadmap still contains the old VM-first recommendation**

Run:

```bash
grep -n "For backend continuity, start with Phase 8B GC groundwork" docs/roadmap.md
```

Expected output includes one matching line from the current `docs/roadmap.md` near the final recommendation. This confirms the archive source is the old roadmap.

- [ ] **Step 2: Create the archive copy**

Run:

```bash
cp docs/roadmap.md docs/roadmap-archive-2026-07-04.md
```

- [ ] **Step 3: Verify the archive is identical to the current roadmap**

Run:

```bash
diff -u docs/roadmap.md docs/roadmap-archive-2026-07-04.md
```

Expected: no output and exit code 0.

- [ ] **Step 4: Commit the archive**

Run:

```bash
git add docs/roadmap-archive-2026-07-04.md
git commit -m "docs: archive previous roadmap"
```

---

### Task 2: Rewrite the Active Roadmap Around Language Features

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Replace `docs/roadmap.md` with the refreshed roadmap**

Overwrite `docs/roadmap.md` with this content:

```markdown
# Compiler Demo Language Roadmap

This roadmap is the active planning entry point for user-visible language development. The previous mixed compiler/backend roadmap is preserved in `docs/roadmap-archive-2026-07-04.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable, but they are a deferred backend track. The current near-term direction is to improve the language itself.

## Current Implemented Baseline

The language currently supports:

- Statements: `let`, `print`, `if`/`else`, `while`, `fun`, `return`, blocks, and expression statements.
- Expressions: literals, arrays, indexing, variables, calls, function expressions, grouping, unary operators, binary/logical operators, and assignment expressions.
- Lexical scopes resolved during type checking.
- Explicit `let` annotations for `number`, `bool`, `string`, and `nil`.
- Named functions, anonymous function expressions, recursion, returns, and by-reference closures.
- Array literals and read-only indexing.
- IR interpreter and bytecode VM execution paths that should match for implemented language features.

For exact implemented grammar and user behavior, see `docs/language-grammar.ebnf` and `README.md`.

## Guiding Principles

- Prefer vertical language slices that update parser, AST, type checker, IR, bytecode lowering, interpreters/VM, docs, and goldens together when behavior crosses layers.
- Keep `--run` and `--run-bytecode` behavior aligned for every supported user-visible feature.
- Keep planned syntax out of `README.md` and `docs/language-grammar.ebnf` until implemented.
- Write a focused design spec and implementation plan for each substantial phase before changing compiler behavior.
- Preserve parse errors, type errors, compile errors, and runtime errors as distinct test categories.

## Recommended Language Phase Order

```text
9. Richer type system
10. Array mutation and collection builtins
11. Loop control and for loops
12. Records / structs
13. Standard builtins
14. Modules / imports
15. Language polish and diagnostics
```

## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested features:

- Internal function signatures for named functions and function expressions.
- User-visible function type annotations, after choosing syntax.
- Array element types, after deciding whether mixed arrays remain a dynamic escape hatch.
- Basic inference for unannotated `let` declarations from initializer expressions.
- Static call checks for variables that are known functions.
- A clear compatibility rule for `nil`.

Likely touch points:

- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp`
- `include/Ast.hpp` and `src/Ast.cpp` if type syntax grows new AST nodes
- `include/Parser.hpp` and `src/Parser.cpp` for user-visible type syntax
- type-error golden fixtures
- `README.md`
- `docs/language-grammar.ebnf`

Recommended first slice: infer simple unannotated `let` types and preserve those types through assignment checks without introducing new type annotation syntax.

## Phase 10: Array Mutation and Collection Builtins

Goal: make arrays useful beyond read-only literals and indexing.

Suggested features:

- `len(xs)` for arrays and strings, if string length should be included.
- Index assignment: `xs[i] = value`.
- `push(xs, value)` and possibly `pop(xs)` after choosing mutable array semantics.
- Runtime checks for non-array values, invalid indexes, and bounds.
- Static checks for known non-array values and known invalid index types.

Likely touch points:

- assignment target parsing and AST representation
- `Value` array representation if arrays become mutable
- IR operations for index assignment or builtin calls
- IR interpreter and bytecode VM behavior
- runtime-error and type-error fixtures
- success fixtures with `run.out` and `run_bytecode.out`

Recommended split:

- Phase 10A: `len` builtin as a small usability slice.
- Phase 10B: index assignment.
- Phase 10C: `push` / `pop` mutation helpers.

## Phase 11: Loop Control and For Loops

Goal: make iteration practical and structured.

Suggested features:

- `break;` exits the nearest loop.
- `continue;` starts the next nearest loop iteration.
- Type errors for `break` and `continue` outside loops.
- A later `for` form after array iteration and mutation semantics are clearer.

Likely touch points:

- `include/Token.hpp` and `src/Lexer.cpp` for new keywords.
- `include/Ast.hpp` and `src/Ast.cpp` for break/continue statements.
- `include/Parser.hpp` and `src/Parser.cpp` for statement parsing.
- `src/TypeChecker.cpp` for loop-depth validation.
- IR control-flow lowering and bytecode parity.
- parse/type/run golden fixtures.

Recommended split:

- Phase 11A: `break` / `continue` for existing `while` loops.
- Phase 11B: `for` loop syntax and lowering.

## Phase 12: Records / Structs

Goal: add named fields and simple aggregate data.

Possible approaches:

- Record literals first: `{ name: "Ada", age: 36 }`.
- Field access: `person.name`.
- Field assignment after mutation rules are clear: `person.age = 37`.
- Named structs later: `struct Person { name: string, age: number }`.

Keep methods, inheritance, and protocols out of the first records slice.

Likely touch points:

- lexer/parser support for field syntax and dot access
- AST expression nodes for record literals and field access
- runtime value representation for records
- type checker field tracking
- IR and bytecode operations for field reads/writes
- docs and golden fixtures

## Phase 13: Standard Builtins

Goal: provide a small standard environment without introducing modules yet.

Suggested builtins:

- Numeric helpers: `floor`, `ceil`, `sqrt`.
- String helpers: `str`, `substr`, `charAt`.
- Collection helpers: `len`, `push`, `pop` if not completed in Phase 10.
- Debug helper: `typeOf` if useful for mixed runtime values.

Each builtin should define behavior for both the IR interpreter and bytecode VM paths, preferably through shared runtime machinery so semantics stay aligned.

## Phase 14: Modules / Imports

Goal: allow programs to be split across files.

Suggested features:

- `import "path";` or `import name from "path";` after selecting a module model.
- Deterministic path resolution relative to the importing file.
- Clear cycle handling.
- CLI behavior for multi-file source loading.
- Golden fixtures that include secondary source files.

Why late: modules affect diagnostics, CLI source management, test layout, and name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Suggested features:

- Source snippets and carets for front-end diagnostics.
- More parse recovery and multi-error reporting.
- Clear handling for lambda expression statements that begin with `fun`, either by documenting parenthesized form or changing parser disambiguation.
- Compound assignment operators such as `+=`, after assignment targets are generalized.
- Comments or doc comments if they are still missing.

## Deferred Backend Track

The bytecode VM already exists and provides extension points for backend research. These directions are deferred while the active roadmap focuses on language features:

- GC groundwork: VM heap ownership, root scanning, and value reachability.
- Task scheduling: schedulable VM threads, instruction budgets, yield points, and blocked states.
- JIT exploration: bytecode metadata, hot function detection, and native-code experiments.

Before starting any backend track, create a dedicated backend design spec and implementation plan rather than mixing it into this language roadmap.

## Near-Term Recommendation

Start with **Phase 9: Richer Type System** if the priority is stronger foundations for records, mutable arrays, and builtin APIs.

Choose **Phase 10A: `len` builtin** instead if the priority is a small, immediately visible usability improvement before deeper type-system work.
```

- [ ] **Step 2: Verify the old VM-first recommendation is no longer active**

Run:

```bash
! grep -n "For backend continuity, start with Phase 8B GC groundwork" docs/roadmap.md
```

Expected: no matching output and exit code 0.

- [ ] **Step 3: Verify the active roadmap points to the archive**

Run:

```bash
grep -n "docs/roadmap-archive-2026-07-04.md" docs/roadmap.md
```

Expected: at least one matching line.

---

### Task 3: Update Agent Roadmap Hints

**Files:**
- Modify: `AGENTS.md`

- [ ] **Step 1: Replace the Roadmap Hints paragraph**

In `AGENTS.md`, replace the existing paragraph under `## Roadmap Hints` with:

```markdown
The active roadmap in `docs/roadmap.md` is now language-focused. Likely future language work includes richer type inference/checking, array mutation and collection builtins, loop control, records/structs, standard builtins, modules/imports, and diagnostic polish. Bytecode VM follow-ups such as GC, task scheduling, and JIT exploration are deferred backend tracks; start them only from a dedicated backend design spec and implementation plan. When adding language features, prefer vertical slices that update parser, AST, type checker, IR, bytecode, interpreters/VM, docs, and goldens together.
```

- [ ] **Step 2: Verify AGENTS no longer presents VM work as the default next step**

Run:

```bash
! grep -n "Likely future work includes bytecode VM follow-ups" AGENTS.md
```

Expected: no matching output and exit code 0.

- [ ] **Step 3: Verify AGENTS references the active roadmap**

Run:

```bash
grep -n "The active roadmap in `docs/roadmap.md` is now language-focused" AGENTS.md
```

Expected: one matching line.

---

### Task 4: Documentation Verification and Commit

**Files:**
- Verify: `docs/roadmap.md`
- Verify: `docs/roadmap-archive-2026-07-04.md`
- Verify: `AGENTS.md`

- [ ] **Step 1: Review the final documentation diff**

Run:

```bash
git diff -- docs/roadmap.md docs/roadmap-archive-2026-07-04.md AGENTS.md
```

Expected:

- `docs/roadmap-archive-2026-07-04.md` is a new copy of the old roadmap.
- `docs/roadmap.md` is rewritten as the active language roadmap.
- `AGENTS.md` roadmap hints point to the language roadmap and defer backend work.
- No compiler source, test, grammar, or README behavior docs are changed.

- [ ] **Step 2: Scan for unresolved placeholders in changed docs**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
patterns = ["T" + "BD", "FIX" + "ME", "PLACE" + "HOLDER"]
for path in [Path("docs/roadmap.md"), Path("docs/roadmap-archive-2026-07-04.md"), Path("AGENTS.md")]:
    for lineno, line in enumerate(path.read_text().splitlines(), start=1):
        if any(pattern in line for pattern in patterns):
            print(f"{path}:{lineno}:{line}")
PY
```

Expected: no output.

- [ ] **Step 3: Run full project verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

- CMake configure and build succeed.
- `ctest` reports all tests passed.
- Golden tests report all checks passed.
- Golden runner selftests report OK.

- [ ] **Step 4: Commit the active roadmap refresh**

Run:

```bash
git add docs/roadmap.md AGENTS.md
git commit -m "docs: refresh language roadmap"
```

---

## Self-Review Checklist

- Spec coverage:
  - Archive old roadmap: Task 1.
  - Rewrite active roadmap: Task 2.
  - Defer VM GC/task/JIT backend track: Task 2.
  - Update AGENTS hints: Task 3.
  - No compiler behavior changes: Tasks 1-4 only touch docs.
- Verification:
  - Grep checks confirm old VM-first recommendation is not active.
  - Full project verification is included before the final documentation commit.
- Commit structure:
  - One commit preserves the old roadmap archive.
  - One commit refreshes the active roadmap and agent hints.
