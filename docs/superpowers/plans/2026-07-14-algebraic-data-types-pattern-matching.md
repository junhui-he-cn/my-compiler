# Algebraic Data Types and Pattern Matching Implementation Plan

## 1. Grammar, AST, and fixtures

- Add `enum`, `match`, and `=>` lexical forms.
- Add enum declarations, match arms, and recursive pattern AST nodes with
  stable AST printing.
- Update the grammar and add successful, parse-error, and type-error fixtures.
- Add a runtime fixture that exercises unit, payload, and nested patterns.

## 2. Type checker and module metadata

- Add enum type information and variant payload declarations.
- Predeclare enum names alongside structs, resolve recursive enum annotations,
  and check variant constructors.
- Check patterns, arm-local bindings, duplicate variants, and exhaustiveness.
- Preserve resolved names for variant constructors and pattern bindings.
- Export/import enum declarations and include them in module-interface output.

## 3. Runtime value and IR

- Add C++ and Rust enum/variant runtime values with structural equality and
  stable formatting.
- Add IR construction/tag/field operations and lower match arms to jumps.
- Keep enum declarations compile-time-only.

## 4. Bytecode and Rust VM

- Add bytecode operations and stable `.cdbc` text syntax.
- Update the Rust artifact parser/formatter and execute all three operations.
- Add bytecode artifact coverage and Rust VM parity coverage.

## 5. Documentation and verification

- Update `README.md`, `docs/language-grammar.ebnf`, and `docs/roadmap.md` to
  describe only implemented behavior.
- Refresh focused goldens, run CMake/CTest, all golden runners, artifact tests,
  Rust VM tests, Cargo tests, and `git diff --check`.
- Remove generated Python caches and keep the commit focused.
