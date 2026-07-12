# Parser Recovery and Multiple Diagnostics Design

## Goal

Add a first parse-only multiple-diagnostics slice. When source contains multiple
recoverable syntax errors, the compiler should report them in one run instead of
stopping at the first parse error.

This phase is intentionally limited to parser diagnostics. Lexer, import, type,
compile, runtime, IR, bytecode, and Rust VM behavior should remain unchanged.

## User-Facing Behavior

Multiple parse diagnostics are enabled by default. No new CLI flag is added.

For a single parse error, output should keep the existing shape:

```text
Parse error at 1:7: expected expression
  print ;
        ^
```

For multiple parse errors, print diagnostics in source order. Each diagnostic
uses the existing formatter, and diagnostics are concatenated without an extra
blank line:

```text
Parse error at 1:7: expected expression
  print ;
        ^
Parse error at 2:11: expected `]` after index, found Semicolon `;`
  print xs[0;
            ^
```

The process still exits with status `1`. If any parse errors are collected, the
compiler does not continue to type checking, IR lowering, bytecode lowering, or
execution artifact generation.

## Scope

Include:

- parse-only recovery and aggregation;
- top-level and block-level declaration/statement recovery;
- file-aware parse diagnostics for direct files, multi-file inputs, and imported
  files;
- golden coverage for multiple parse errors in one file and inside a block.

Exclude:

- lexer multi-diagnostics;
- type-checker multi-diagnostics;
- import-error aggregation;
- runtime-error aggregation;
- parser error nodes in the AST;
- grammar changes;
- speculative expression-level repair beyond statement/declaration recovery;
- new CLI flags such as `--all-errors`.

## Parser Architecture

Keep `ParseError` as the unit diagnostic thrown by existing parsing helpers.
Add an aggregate parse exception type, likely in `include/Parser.hpp`:

```cpp
class ParseErrorList final : public std::exception {
public:
    explicit ParseErrorList(std::vector<ParseError> errors);
    const std::vector<ParseError>& errors() const;
    const char* what() const noexcept override;
};
```

`Parser::parse()` should become a recovery loop:

1. call a helper that parses one declaration;
2. if it succeeds, append the statement;
3. if it throws `ParseError`, store the error and call `synchronize()`;
4. keep parsing until EOF;
5. if any errors were collected, throw `ParseErrorList`;
6. otherwise return the complete `Program`.

`blockStatements()` should use the same recovery pattern so function bodies,
blocks, and loop/branch bodies can report more than one syntax error before the
closing brace.

Existing syntax functions such as `consume()`, `assignment()`, `primary()`, and
specific declaration/statement parsers can continue throwing `ParseError`.
Recovery is owned by declaration/statement boundaries rather than by every
expression parser.

## Synchronization Strategy

Add `Parser::synchronize()` with conservative statement-boundary recovery.

Suggested behavior:

1. If the current token is `;`, consume it.
2. Advance until EOF or a likely recovery point.
3. Stop before consuming `}` so the enclosing block parser can close normally.
4. Stop at likely declaration/statement starts:
   - `let`
   - `fun`
   - `struct`
   - `impl`
   - `import`
   - `export`
   - `print`
   - `if`
   - `while`
   - `for`
   - `break`
   - `continue`
   - `return`
   - `{`
   - `}`

This is deliberately conservative. It should recover well from independent
statement errors such as missing expressions, missing semicolons, and invalid
assignment targets, but it does not need to recover field-by-field inside struct
field lists or parameter-by-parameter inside function signatures in this slice.

## Frontend and Diagnostic Flow

Lexer behavior is unchanged: lexing still fails at the first lexer diagnostic.

For parser errors:

- stdin parsing can throw `ParseErrorList` directly;
- direct single-file and multi-file parsing should preserve current source-line
  and caret behavior;
- imported-file parsing should preserve file path, source line, and caret for
  each parse diagnostic.

`FrontendSession` currently maps file-backed parse errors to
`FileDiagnosticError`. Extend that mapping to aggregate parse errors. One
possible implementation is a file-aware aggregate type that stores a vector of
`FileDiagnosticError` values for imported/direct files and a vector of
`ParseError` values for stdin/pathless parsing.

`main.cpp` should catch the aggregate parse error path before the generic
`DiagnosticError` path, format each diagnostic with the existing functions, and
write them to stderr separated by a single newline between diagnostic strings.
Exit status remains `1`.

## Golden Runner Behavior

`tests/run_golden_tests.py` already supports full snippet `.err` files for parse
errors. Keep that behavior. Existing one-line expected parse diagnostics should
continue to be accepted for single-error fixtures through the current
first-line/snippet compatibility rule.

For new multi-diagnostic fixtures, use full `.err` files with every diagnostic
and snippet. Multi-diagnostic expectations should match stderr exactly after the
existing checkout-path normalization.

## Tests

Add parse-error fixtures under `tests/golden/parse_errors/`:

1. independent top-level statement errors, for example:

   ```cd
   print ;
   let x = ;
   print 1;
   ```

   Expected: two parse diagnostics, no stdout, exit `1`.

2. block-level recovery, for example:

   ```cd
   fun bad() {
     print ;
     let x = ;
     return 1;
   }
   ```

   Expected: two parse diagnostics inside the function body, no stdout, exit
   `1`.

3. direct multi-file parse diagnostics, if current test layout allows it through
   a focused Python CLI test, where errors in different direct input files both
   report their own file paths.

4. imported-file parse diagnostics, through a focused Python CLI test or golden
   fixture, where multiple errors in the imported file all report that imported
   file path.

Existing parse-error fixtures should not need output changes unless the new
recovery intentionally finds additional errors in an existing fixture. Prefer
adding new multi-diagnostic fixtures over broad refreshing of existing goldens.

## Documentation

Update `README.md` diagnostics text to mention that parse diagnostics can report
multiple syntax errors in one run.

Do not update `docs/language-grammar.ebnf`, because grammar does not change.

## Success Criteria

- Multiple independent parse errors in one file are reported in a single run.
- Multiple parse errors inside a block are reported in a single run.
- Single parse-error output remains compatible with existing fixtures.
- File-backed diagnostics keep correct paths, source lines, and carets for every
  parse diagnostic.
- Type, import, runtime, IR, bytecode, and Rust VM behavior remain unchanged.
- Full verification passes.
