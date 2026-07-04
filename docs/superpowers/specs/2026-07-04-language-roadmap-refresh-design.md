# Language Roadmap Refresh Design

Date: 2026-07-04

## Goal

Replace the current mixed compiler/backend roadmap with a language-focused development roadmap. Preserve the existing roadmap as an archive so prior backend and VM planning history remains available, but make `docs/roadmap.md` describe the next language-level phases rather than VM GC, task scheduling, or JIT work.

## Motivation

The current language already has lexical scope, basic annotations, logical operators, loops, diagnostics, functions, closures, lambdas, arrays, and a bytecode backend. The most useful next work is no longer backend infrastructure. The language itself is missing several user-visible capabilities: richer type information, mutable arrays and common builtins, loop control, data records, standard library functions, modules, and polish around diagnostics and grammar edge cases.

The refreshed roadmap should help future agents choose language slices without treating VM follow-ups as the default next step.

## Scope

In scope:

- Archive the current roadmap under a dated filename, for example `docs/roadmap-archive-2026-07-04.md`.
- Rewrite `docs/roadmap.md` as the active language roadmap.
- Keep the new roadmap aligned with implemented behavior from `README.md`, `AGENTS.md`, and `docs/language-grammar.ebnf`.
- Explicitly state that VM GC, task scheduling, and JIT are deferred backend tracks, not the current active recommendation.
- Define a recommended language phase order with goals, suggested syntax or behavior, likely touch points, and testing expectations.
- Update `AGENTS.md` only if needed to point future agents at the refreshed roadmap conventions.

Out of scope:

- Implementing any language feature in this phase.
- Removing backend or VM design docs.
- Changing the compiler, parser, IR, bytecode, tests, or README behavior documentation beyond roadmap references.
- Designing full detailed specs for every future language feature. Each phase should still get its own spec before implementation.

## Proposed Roadmap Structure

`docs/roadmap.md` should become a language roadmap with these sections:

1. **Current Implemented Baseline**
   - Summarize implemented statements and expressions.
   - Mention that IR and bytecode parity exists for supported features.
   - Link readers to README and grammar for exact current behavior.

2. **Guiding Principles**
   - Prefer vertical language slices across parser, AST, type checker, IR, interpreters/VM, docs, and goldens.
   - Keep runtime/bytecode parity for user-visible language features.
   - Do not document planned syntax as implemented.
   - Create a design spec and implementation plan per substantial phase.

3. **Recommended Language Phase Order**
   - Phase 9: Richer Type System
   - Phase 10: Array Mutation and Collection Builtins
   - Phase 11: Loop Control and For Loops
   - Phase 12: Records / Structs
   - Phase 13: Standard Builtins
   - Phase 14: Modules / Imports
   - Phase 15: Language Polish and Diagnostics

4. **Deferred Backend Track**
   - Keep VM GC, task scheduling, and JIT listed as deferred backend directions.
   - Make clear they are intentionally not the active near-term recommendation.

5. **Near-Term Recommendation**
   - Recommend Phase 9 or Phase 10 depending on desired focus.
   - Prefer Phase 9 if type soundness and future APIs are the priority.
   - Prefer Phase 10 if immediate language usability is the priority.

## Phase Details

### Phase 9: Richer Type System

Goal: turn the current annotation checker into a more useful static type layer.

Suggested features:

- Function types, such as `(number, number) -> number`, or a simpler internal-only function signature representation before user syntax.
- Array element types, such as `[number]`, with mixed arrays remaining possible only if an explicit dynamic/unknown type is chosen later.
- Basic inference for unannotated `let` declarations from initializer expressions.
- Static call checks for variables that are known functions.
- Clear behavior for `nil` compatibility.

Why early: array mutation, records, builtins, and modules all become easier to specify once function and collection types have a direction.

### Phase 10: Array Mutation and Collection Builtins

Goal: make arrays useful beyond read-only literals and indexing.

Suggested features:

- Index assignment: `xs[i] = value`.
- `len(xs)` builtin.
- `push(xs, value)` builtin or method-like syntax if records/methods are planned first.
- Runtime and static checks for non-array values, invalid indexes, and incompatible element types when known.

Why after or alongside Phase 9: mutation needs a type story for element compatibility, but `len` can be implemented earlier as a small builtin slice.

### Phase 11: Loop Control and For Loops

Goal: make iteration practical and structured.

Suggested features:

- `break;` exits the nearest loop.
- `continue;` jumps to the next nearest loop iteration.
- Type errors for `break` / `continue` outside loops.
- A later `for` form, either C-style or collection-oriented, after array iteration semantics are clearer.

Recommended split:

- Phase 11A: `break` / `continue` for `while`.
- Phase 11B: `for` loop syntax and lowering.

### Phase 12: Records / Structs

Goal: add named fields and simple aggregate data.

Possible approaches:

- Record literals first: `{ name: "Ada", age: 36 }` plus field access `person.name`.
- Named structs later: `struct Person { name: string, age: number }`.
- Keep methods and inheritance out of scope initially.

Why after types: records need field type tracking and useful diagnostics for missing fields.

### Phase 13: Standard Builtins

Goal: provide a small standard environment without introducing modules yet.

Suggested builtins:

- Numeric helpers: `floor`, `ceil`, `sqrt`.
- String helpers: `str`, `substr`, maybe `charAt`.
- Collection helpers: `len`, `push`, `pop` if not already completed.
- Debug helpers: `typeOf` if useful for mixed runtime values.

Each builtin should define behavior in both IR interpreter and bytecode VM paths, or be represented in shared runtime machinery so both paths stay aligned.

### Phase 14: Modules / Imports

Goal: allow programs to be split across files.

Suggested features:

- `import "path";` or `import name from "path";` after choosing a module model.
- Deterministic path resolution relative to the importing file.
- Cycle handling strategy.
- CLI behavior for multi-file source loading.

Why late: modules affect diagnostics, CLI, test fixtures, and name resolution across compilation units.

### Phase 15: Language Polish and Diagnostics

Goal: improve day-to-day ergonomics without changing major semantics.

Suggested features:

- Source snippets and carets for front-end diagnostics.
- More parse recovery and multi-error reporting.
- Grammar edge-case polish, such as parenthesized lambda expression-statement guidance or parser disambiguation if desired.
- Additional compound operators, such as `+=`, after assignment targets are generalized.
- Comments or doc comments if not already present.

## Recommended Next Choice

The active recommendation is to start with one of these two language tracks:

1. **Phase 9: Richer Type System** — best if future records, array mutation, and builtin APIs should have stronger static guarantees.
2. **Phase 10A: `len` and array usability** — best if the next step should produce a highly visible user-facing feature quickly, while leaving full mutation/type work for later.

The roadmap should present Phase 9 as the default recommendation, with Phase 10A as a reasonable pragmatic alternative.

## Documentation and Git Plan

Implementation of this roadmap refresh should be documentation-only:

- Copy current `docs/roadmap.md` to `docs/roadmap-archive-2026-07-04.md`.
- Replace `docs/roadmap.md` with the new language roadmap.
- Update `AGENTS.md` roadmap hints if they still imply the old VM-first recommendation.
- Run no compiler rebuild unless desired; at minimum run a quick repository status check and a markdown content review. Full project verification is optional because behavior does not change, but safe to run before final completion.

## Risks and Mitigations

- **Risk:** The new roadmap may imply planned syntax is implemented.
  - **Mitigation:** Keep planned phases clearly labeled as future work and point exact implemented syntax to README/grammar.

- **Risk:** Archiving the old roadmap hides backend plans.
  - **Mitigation:** Keep a deferred backend track in the active roadmap and preserve the full old file under an archive path.

- **Risk:** The phase order may overcommit to type system work before user-visible features.
  - **Mitigation:** Include Phase 10A as an alternate next slice for fast language usability.

## Success Criteria

- `docs/roadmap.md` no longer recommends VM GC/task/JIT as the default next phase.
- The old roadmap is preserved in a dated archive file.
- The active roadmap clearly lists current language gaps and future language phases.
- The roadmap is concise enough for future agents to use as a planning entry point.
- No compiler behavior changes are made in this phase.
