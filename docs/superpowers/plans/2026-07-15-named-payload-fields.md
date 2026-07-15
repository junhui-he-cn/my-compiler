# Named Enum Payload Fields Implementation Plan

## 1. Establish tests

- Add a success fixture with named declarations, reordered nested patterns,
  positional constructor compatibility, and module-interface output.
- Add type-error fixtures for unknown, duplicate, incomplete, mixed, and
  positional-variant named patterns.
- Add a focused artifact fixture and Rust VM run output.

## 2. Thread payload names through metadata

- Extend enum declaration AST and checked enum variant types with aligned names.
- Preserve names through module symbols, namespace qualification, re-exports,
  and module-interface text.
- Keep existing positional declarations and aggregate test construction valid.

## 3. Parse and print named forms

- Parse optional `name:` prefixes in enum payload declarations and variant
  pattern arguments.
- Print names in AST output and update the implemented EBNF.

## 4. Check and lower patterns

- Validate declaration style and pattern field names/completeness.
- Record resolved payload indexes alongside existing resolved variant names.
- Emit `variant_field` with those indexes for both statement and expression
  matches.

## 5. Verify and finish

- Update `README.md`, `AGENTS.md`, and `docs/roadmap.md`.
- Run focused and full CMake, golden, artifact, Rust VM, Cargo, and diff checks;
  clean Python caches; commit and push.
