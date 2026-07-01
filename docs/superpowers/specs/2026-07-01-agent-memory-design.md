# Agent Memory File Design

## Goal

Create a project-level memory file for future Codex/AI-agent development. The file should help agents quickly understand the compiler demo's architecture, extension workflow, test expectations, and current semantics before adding grammar or backend features.

## Recommended Approach

Use a root-level `AGENTS.md` as the primary memory file.

Reasons:

- It is a common convention for agent-facing repository instructions.
- It is easy to discover from the repository root.
- It keeps instructions close to the code and avoids fragmenting a small project across too many documents.
- It can focus on operational guidance rather than duplicating all README content.

## File Scope

`AGENTS.md` should be concise but actionable. It should contain:

1. Project overview.
2. Architecture map for lexer, parser, AST, IR compiler, IR interpreter, values, CLI, and tests.
3. Build and verification commands.
4. Workflow for adding language syntax.
5. Workflow for adding IR/backend behavior.
6. Golden test conventions for success, runtime-error, and parse-error cases.
7. Documentation update rules.
8. Git and workspace hygiene.
9. Current language semantics and known limitations.
10. Roadmap hints for likely future compiler work.

## Current Semantics to Record

The memory file should explicitly remind agents that:

- The project uses C++17, CMake, CTest, and Python 3 golden test tooling.
- Type annotations on `let` declarations are currently syntax-only.
- Blocks group statements but do not introduce lexical scope yet.
- Assignment expressions update existing variables and evaluate to the assigned value.
- Assigning to an undefined variable is a runtime error.
- Parser and grammar precedence are documented in `docs/language-grammar.ebnf`.

## Test and Verification Guidance

The memory file should list these standard commands:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Agents should run the relevant subset while developing and the full set before claiming completion.

## Golden Test Conventions

Document these fixture shapes:

- Successful cases: `tests/golden/<case>/input.cd` plus one or more of `ast.out`, `ir.out`, `run.out`.
- Runtime errors: `tests/golden/runtime_errors/<case>.cd`, `<case>.run.err`, and `<case>.exit`.
- Parse errors: `tests/golden/parse_errors/<case>.cd`, `<case>.err`, and `<case>.exit`.

Also document golden refresh:

```sh
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

## Non-Goals

- Do not create a long human tutorial in this change.
- Do not replace `README.md` or `docs/language-grammar.ebnf`.
- Do not introduce new tooling or change build/test behavior.
- Do not change compiler semantics.

## Success Criteria

- `AGENTS.md` exists at the repository root.
- It is specific to this compiler project rather than generic agent advice.
- It gives future agents enough context to add grammar and backend features safely.
- It does not duplicate large sections of source code or become a second README.
