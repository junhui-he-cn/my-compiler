# OR Patterns Implementation Plan

1. Add failing success, type-error, and parse-error fixtures for OR patterns,
   including shared bindings and nested alternatives.
2. Add the single-pipe token, AST node, printer, and recursive pattern parser
   while preserving `||` expression parsing.
3. Extend type-checker binding unification and union coverage, then lower OR
   alternatives to existing IR control flow with shared capture registers.
4. Update grammar, README, AGENTS semantics, roadmap, golden outputs, and
   bytecode artifact coverage.
5. Run the complete verification suite, clean generated caches, commit, and
   push the focused slice.
