# Match Expression Implementation Plan

## 1. Establish the contract and red tests

- Record the syntax, typing, lowering, and out-of-scope rules in the design
  spec.
- Add a match-expression success fixture and type-error fixtures for
  non-exhaustive and incompatible-arm cases.
- Confirm the success fixture is rejected before parser support exists.

## 2. Extend the AST, parser, and printer

- Add `MatchExpr` and `MatchExprArm` nodes carrying the existing pattern nodes.
- Parse `match` as a primary expression with comma-separated expression arms
  and an optional trailing comma.
- Preserve source spans and add stable AST output.

## 3. Add static checking

- Reuse scrutinee validation, `checkPattern`, coverage tracking, and arm-local
  scopes from statement-form `match`.
- Check arm expressions against an optional expected type and merge compatible
  arm result types.
- Record useful diagnostics for missing variants, incompatible arm results, and
  out-of-scope binding use.

## 4. Lower and verify execution

- Add expression lowering that shares pattern control flow and copies the
  selected arm value into one result register.
- Confirm no bytecode or Rust VM changes are needed, then add the `.cdbc`
  artifact fixture and Rust VM golden coverage.

## 5. Update documentation and finish

- Update `README.md`, `AGENTS.md`, `docs/language-grammar.ebnf`, and the active
  roadmap to describe the implemented expression form and remaining pattern
  matching work.
- Run focused tests, then the full CTest, golden, artifact, Rust VM, Cargo, and
  diff checks; clean Python caches.
- Commit and push the focused slice, then report branch and verification state.
