# Match Guard Implementation Plan

## 1. Establish tests

- Add statement and expression guard success fixtures, including false-guard
  fall-through and pattern bindings used by guards.
- Add type-error coverage for a guarded-only non-exhaustive match and an
  undefined guard binding.
- Add a parse-error fixture for a missing guard expression when practical.

## 2. Extend AST and parser

- Add optional guard expressions to statement and expression match arms.
- Parse `if expression` between each pattern and `=>` while preserving the
  existing statement-form syntax.
- Print guards in AST output and update grammar documentation.

## 3. Check and lower guards

- Check guards inside the arm-local pattern scope.
- Exclude guarded arms from exhaustive coverage.
- Lower guard failures onto the same next-arm failure path as pattern failures.
- Keep match-expression result typing and statement fall-through analysis
  sound in the presence of guards.

## 4. Verify and finish

- Add a focused `.cdbc` artifact and Rust VM golden coverage.
- Update `README.md`, `AGENTS.md`, and `docs/roadmap.md`.
- Run focused and full verification, clean Python caches, commit, and push.
