# Generic Lambdas Implementation Plan

## Goal

Support generic anonymous function expressions with the existing generic call
inference and explicit type-argument machinery.

## Tasks

- [x] Add success and type-error source fixtures for generic lambda syntax,
  inference, explicit calls, and invalid assignments.
- [x] Extend `FunctionExpr`, its AST printer, and `Parser::functionExpression`
  with optional type parameters; keep `fun<T>(...)` on the expression path.
- [x] Update `TypeChecker::checkFunctionExpression` to scope, resolve, check,
  and return generic lambda signatures, including nested escape validation.
- [x] Generate and review AST, IR, and run goldens; confirm no bytecode or VM
  opcode changes are needed.
- [x] Update `README.md`, `docs/language-grammar.ebnf`, and `docs/roadmap.md`.
- [x] Run focused tests, the complete C++/Python/Rust verification gate, and
  remove generated Python caches.

## Expected backend invariant

Generic lambda type parameters must not appear in IR, `.cdbc` artifacts, or
Rust VM values. They are erased by the time `IRCompiler` lowers the checked AST.
