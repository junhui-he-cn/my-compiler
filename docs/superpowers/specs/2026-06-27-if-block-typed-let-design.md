# If, Block, and Typed Let Design

Date: 2026-06-27

## Goal

Extend the current language with:

- `if` / `else` statements.
- `{ ... }` block statements.
- Optional type annotations on `let` declarations.

The implementation should support parsing, AST printing, IR generation, IR interpretation, golden tests, and documentation updates. Type annotations are syntax-only in this change: they are stored in the AST and shown in AST output, but they do not perform static or runtime type checking.

## Grammar

Target grammar:

```ebnf
program     = { declaration } ;

declaration = letDecl
            | statement ;

statement   = printStmt
            | ifStmt
            | block
            | exprStmt ;

letDecl     = "let", identifier,
              [ ":", typeName ],
              "=", expression, ";" ;

ifStmt      = "if", expression, block,
              [ "else", block ] ;

block       = "{", { declaration }, "}" ;

printStmt   = "print", expression, ";" ;

exprStmt    = expression, ";" ;

expression  = equality ;

equality    = comparison,
              { ( "==" | "!=" ), comparison } ;

comparison  = term,
              { ( "<" | "<=" | ">" | ">=" ), term } ;

term        = factor,
              { ( "+" | "-" ), factor } ;

factor      = unary,
              { ( "*" | "/" ), unary } ;

unary       = ( "!" | "-" ), unary
            | primary ;

primary     = "false"
            | "true"
            | "nil"
            | number
            | string
            | identifier
            | "(", expression, ")" ;

typeName    = identifier ;
```

## Lexer Changes

Add token types:

```cpp
LeftBrace,
RightBrace,
Colon,
If,
Else,
```

Lexical spelling:

```text
{ -> LeftBrace
} -> RightBrace
: -> Colon
if -> If
else -> Else
```

The existing identifier keyword lookup should recognize `if` and `else` as reserved words.

## AST Changes

Add block and if statement nodes:

```cpp
struct BlockStmt final : Stmt {
    explicit BlockStmt(std::vector<StmtPtr> statements);
    void print(std::ostream& out, int indent) const override;

    std::vector<StmtPtr> statements;
};

struct IfStmt final : Stmt {
    IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
};
```

`thenBranch` and `elseBranch` are stored as `StmtPtr`, but the parser requires both to be blocks for this grammar. `elseBranch` may be null when no `else` is present.

Extend `LetStmt` with an optional type name:

```cpp
struct LetStmt final : Stmt {
    LetStmt(Token name, std::optional<Token> typeName, ExprPtr initializer);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::optional<Token> typeName;
    ExprPtr initializer;
};
```

Type annotations are preserved for syntax and AST output only. They are not used by the IR compiler or interpreter in this change.

## Parser Changes

`let` declarations become:

```text
let name [: typeName] = expression;
```

Unlike the previous implementation, an initializer is required. This makes the grammar explicit and matches the new typed declaration form.

Statement parsing gains:

```text
if expression block [else block]
block
```

`block` parses zero or more declarations until `}`. Because block contents are declarations, `let` declarations are allowed inside blocks.

Parser errors should remain explicit:

- Missing variable name after `let`.
- Missing type name after `:`.
- Missing `=` after a `let` declaration.
- Missing `{` after an `if` condition.
- Missing `}` after a block.
- Missing `;` after print/expression statements.

## IR Changes

Add control-flow operations:

```cpp
enum class IROp {
    ...
    Jump,
    JumpIfFalse,
};
```

Instruction conventions:

```text
Jump:        operand = target instruction index
JumpIfFalse: left = condition register, operand = target instruction index
```

Add IRProgram helpers:

```cpp
std::size_t emitJump();
std::size_t emitJumpIfFalse(IRRegister condition);
void patchJump(std::size_t jumpInstruction);
```

`patchJump` sets the jump target to the current instruction count.

IR printing should show jump targets clearly:

```text
0001  jump_if_false v0, 0004
0003  jump 0006
```

## IRCompiler Changes

Compile `BlockStmt` by compiling each child statement in order.

Compile `IfStmt` with patched jumps:

Without `else`:

```text
condition = compile(condition)
jumpIfFalse = emitJumpIfFalse(condition)
compile then block
patchJump(jumpIfFalse)
```

With `else`:

```text
condition = compile(condition)
jumpIfFalse = emitJumpIfFalse(condition)
compile then block
jumpOverElse = emitJump()
patchJump(jumpIfFalse)
compile else block
patchJump(jumpOverElse)
```

`LetStmt::typeName` is ignored by IR generation.

## IRInterpreter Changes

Change execution from a `for` loop to an instruction pointer loop:

```cpp
std::size_t ip = 0;
while (ip < instructions.size()) {
    ...
}
```

Most instructions increment `ip` by one after executing.

Jump behavior:

```text
Jump:
    ip = operand

JumpIfFalse:
    if (!isTruthy(registers[left])) ip = operand
    else ++ip
```

Jump targets must be checked for range. A target equal to `instructions.size()` is valid and means execution continues past the end of the program.

`JumpIfFalse` uses the existing truthiness rule:

- `nil` is false.
- `false` is false.
- Everything else is true.

## Block Scope Semantics

Blocks are statement grouping constructs only. They do not create lexical scopes in this change.

Example:

```text
if true {
  let x = 1;
}
print x;
```

Expected output:

```text
1
```

This preserves the current single global variable table behavior and avoids adding environment stacks, shadowing rules, or variable lifetime rules in the same change.

## Tests

Add golden cases.

### `tests/golden/if_else/input.cd`

```text
let ok = true;
if ok {
  print "then";
} else {
  print "else";
}

if false {
  print "bad";
} else {
  print "fallback";
}
```

Expected runtime output:

```text
then
fallback
```

### `tests/golden/block/input.cd`

```text
{
  let x = 10;
  print x;
}
print x;
```

Expected runtime output:

```text
10
10
```

This documents that blocks do not introduce lexical scope.

### `tests/golden/typed_let/input.cd`

```text
let answer: number = 42;
let label: string = "answer";
print label;
print answer;
```

Expected runtime output:

```text
answer
42
```

Each successful golden case should include:

```text
ast.out
ir.out
run.out
```

The existing golden runner should be used to generate and verify outputs.

## Documentation Updates

Update `docs/language-grammar.ebnf` to the target grammar.

Update `README.md` to mention:

- `if expression { ... } else { ... }` statements.
- Block statements.
- Typed let declarations: `let name: type = expression;`.
- Type annotations are currently syntax-only and are not checked.
- Blocks do not introduce lexical scope yet.

## Non-Goals

This change does not add:

- Static type checking.
- Runtime type checking for typed `let` declarations.
- Lexical scopes for blocks.
- Variable shadowing rules.
- `while`, `for`, functions, `return`, `break`, or `continue`.
- SSA or control-flow graph construction.

The new jump IR is a linear control-flow mechanism for executing `if/else` and is a foundation for later control-flow features.
