# Function Expression Statement Design

## Goal

Implement Phase 15B as a small language-polish slice: allow anonymous function expressions that begin with `fun` to appear directly as expression statements.

Example:

```cd
fun (x) {
  return x + 1;
};
print "ok";
```

This should parse as an expression statement containing a function value, followed by a print statement. It improves consistency with the existing rule that functions are values and expression statements are allowed.

## Current Problem

The parser currently treats any declaration starting with `fun` as a named function declaration:

```cpp
if (match(TokenType::Fun)) {
    return functionDeclaration();
}
```

That means this valid-looking anonymous function expression statement:

```cd
fun (x) { return x; };
```

is parsed as a declaration and fails because `functionDeclaration()` expects an identifier after `fun`.

Anonymous function expressions already work in expression positions such as `let` initializers, calls, and parenthesized expressions. The issue is only declaration/statement dispatch at the start of a statement.

## User-Facing Semantics

Support direct anonymous function expression statements anywhere declarations are currently allowed:

```cd
fun (x) { return x; };

{
  fun (y) { return y; };
}

if true {
  fun () { return nil; };
}
```

The expression statement evaluates to a function value and discards it, matching the existing behavior for other expression statements such as:

```cd
1 + 2;
"unused";
```

Named function declarations keep their current behavior:

```cd
fun add(a, b) {
  return a + b;
}
```

The parser should distinguish the two forms with one-token lookahead after `fun`:

- `fun identifier ...` is a named function declaration.
- `fun ( ...` is an anonymous function expression and should be parsed by the expression-statement path.

Malformed `fun` starts should produce a clear parse diagnostic. For example:

```cd
fun ;
```

should report a parse error like:

```text
Parse error at 1:5: expected function name after `fun` declaration or `(` for function expression, found Semicolon `;`
  fun ;
      ^
```

The exact first-line text should be fixed in the implementation plan and goldens.

## Architecture

Make the smallest parser change possible.

Add a helper on `Parser`:

```cpp
bool checkNext(TokenType type) const;
```

Use it in `Parser::declaration()`:

```cpp
if (check(TokenType::Fun) && checkNext(TokenType::Identifier)) {
    advance();
    return functionDeclaration();
}
if (check(TokenType::Fun) && !checkNext(TokenType::LeftParen)) {
    throw ParseError(peek(), "expected function name after `fun` declaration or `(` for function expression");
}
```

Then let `fun (` fall through to `statement()` and `expressionStatement()`. `primary()` already matches `TokenType::Fun` and calls `functionExpression()`, so no AST, IR, bytecode, interpreter, or VM changes are needed.

This approach keeps named function parsing unchanged and avoids introducing an AST node for declarations-vs-expressions.

## AST / IR / Runtime Behavior

No new AST types are required.

A bare anonymous function expression statement should print as an existing `ExpressionStmt` with an existing `FunctionExpr`, for example:

```text
Program
  Expr (fun (x)
    Return (+ x 1))
  Print "ok"
```

IR/bytecode generation should naturally lower the function expression, discard the result, and continue. Runtime behavior should match existing expression statements: no output unless the function body is called elsewhere.

## Error Handling

Expected parse errors:

- `fun ;` reports a clear dispatch error at the token after `fun`.
- Existing malformed anonymous function expression cases, such as missing body after `fun (x)`, should continue to use the existing function-expression diagnostics.
- Existing malformed named function declarations, such as missing function name when using `fun` followed by neither identifier nor `(`, should use the new clearer dispatch error.

Located parse errors should include the source snippet/caret from Phase 15A.

## Testing Strategy

Use TDD. Add failing tests before implementation.

Success golden fixtures:

1. Top-level anonymous function expression statement:

```cd
fun (x) {
  return x + 1;
};
print "ok";
```

Expected:

```text
ok
```

Also include `ast.out` to prove it is an expression statement and no named declaration is created.

2. Block-level anonymous function expression statement:

```cd
{
  fun () {
    return 1;
  };
}
print "done";
```

Expected run output:

```text
done
```

Parse-error fixture:

```cd
fun ;
```

Expected first line:

```text
Parse error at 1:5: expected function name after `fun` declaration or `(` for function expression, found Semicolon `;`
```

The fixture should include the full source snippet and caret.

Regression coverage:

- Existing named function declaration fixtures continue to pass.
- Existing lambda fixtures in `let` initializers and immediate calls continue to pass.
- Bytecode/Rust VM parity should be covered by at least one success fixture through existing golden/Rust VM runners if it includes `bytecode.out` or artifact coverage. For this polish slice, a normal `run.out` golden is sufficient unless implementation unexpectedly touches codegen.

## Documentation

Update:

- `README.md`: mention that anonymous function expressions can appear directly as expression statements, not only in parenthesized or initializer positions.
- `docs/language-grammar.ebnf`: no grammar shape change is required because `exprStmt = expression, ";"` and `primary = functionExpr | ...` already describe this behavior. Add a short comment if helpful, but avoid documenting parser-dispatch internals.
- `docs/roadmap.md`: mark Phase 15B implemented after completion.
- `AGENTS.md`: update current language semantics to mention bare anonymous function expression statements are supported.

## Out of Scope

- Changing named function declaration syntax.
- Requiring lambdas to be parenthesized.
- Warning about unused function values.
- Adding dead-code or unused-expression analysis.
- Changing AST/IR/runtime representation for function expressions.
- Adding methods or closures beyond existing function-expression behavior.

## Future Work

A later language-polish phase can add warnings or lints for unused expression statements, including discarded function values. That should be a separate diagnostics/linting design because the language currently accepts many effect-free expression statements.
