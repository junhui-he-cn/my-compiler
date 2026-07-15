# Named Struct Record Patterns Implementation Plan

1. Add failing success, type-error, and parse-error fixtures for local and
   qualified named record patterns.
2. Add record-pattern AST nodes and parser support without changing map or
   struct-constructor expression parsing.
3. Extend type checking for named fields, nested bindings, nullable structs,
   and record-pattern exhaustiveness; lower fields through existing IR.
4. Update grammar, README, AGENTS semantics, roadmap, golden outputs, module
   coverage, and bytecode artifacts.
5. Run the complete verification suite, clean generated caches, commit, and
   push the focused slice.
