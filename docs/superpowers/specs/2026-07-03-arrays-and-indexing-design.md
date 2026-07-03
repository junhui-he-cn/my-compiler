# Arrays and Indexing Design

Date: 2026-07-03

## Goal

Implement Phase 7 arrays and indexing as the language's first compound data type. This phase adds array literals and read-only indexing while deferring element assignment, array methods, length builtins, and array type annotations.

## Scope

In scope:

- Array literals: `[expr, expr, ...]` and empty arrays `[]`.
- Index expressions: `collection[index]`.
- Chained suffix expressions with calls and indexing, such as `makeArray()[0]`, `nested[0][1]`, and `f()[0]`.
- Mixed element types at runtime.
- Runtime bounds/type validation for indexing.
- Golden coverage for AST, IR, run output, parse errors, type errors, and runtime errors.
- Documentation updates for grammar, README, roadmap, and project memory.

Out of scope:

- Element assignment such as `xs[0] = 42`.
- Mutation methods such as `push`.
- Length builtins such as `len(xs)`.
- Array type annotations or generic types such as `array<number>`.
- Static element type inference.

## Language Semantics

Array literals evaluate their elements from left to right and produce an array value:

```text
let xs = [1, "two", true, nil];
print xs[1];
```

Expected output:

```text
two
```

Arrays may contain any runtime values currently supported by the language, including numbers, strings, booleans, nil, functions, and other arrays. Mixed element types are valid.

Index expressions read an element from an array:

```text
let xs = [10, 20, 30];
print xs[0];
print xs[1 + 1];
```

Expected output:

```text
10
30
```

Indexing is a postfix expression at the same precedence level as calls. The parser accepts chains such as:

```text
makeArray()[0]
nested[0][1]
f()[0]
```

Index validation:

- The indexed value must be an array.
- The index value must be a number.
- The numeric index must be an integer.
- The index must be within `[0, array.size)`.

Runtime diagnostics use these messages:

```text
Runtime error: can only index arrays
Runtime error: array index must be number
Runtime error: array index must be integer
Runtime error: array index out of range
```

Static type checking should catch statically known invalid index uses with these messages:

```text
Type error at <line>:<column>: can only index arrays
Type error at <line>:<column>: array index must be number
```

Array printing uses the existing `valueToString()` convention for elements and wraps them in brackets:

```text
print [1, "two", true, nil];
```

Expected output:

```text
[1, two, true, nil]
```

String elements are not additionally quoted because the existing print behavior for strings prints their raw value.

## Lexer, Parser, and AST

Add tokens:

- `LeftBracket` for `[`
- `RightBracket` for `]`

Comma already exists and is reused for separating array elements.

Add AST expressions:

```cpp
struct ArrayExpr final : Expr {
    explicit ArrayExpr(std::vector<ExprPtr> elements);
    void print(std::ostream& out) const override;

    std::vector<ExprPtr> elements;
};

struct IndexExpr final : Expr {
    IndexExpr(ExprPtr collection, Token bracket, ExprPtr index);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
};
```

AST printing:

```text
(array)
(array 1 2 3)
(index xs 0)
(index (call makeArray) 0)
```

Parsing changes:

- `primary()` parses array literals when it sees `[`.
- The current `call()` suffix loop should be generalized to parse both call and index suffixes:
  - `expr(args)`
  - `expr[index]`
- Trailing commas are not supported in this phase. `[1,]` should fail with `Parse error at ...: expected expression`.
- Missing closing bracket in literals should report `expected \`]\` after array elements`.
- Missing closing bracket in indexes should report `expected \`]\` after index`.

## Type Checker

Add `StaticType::Array` and return `"array"` from `staticTypeName()`.

Type-checking array literals:

- Check each element expression for existing errors.
- Do not require all elements to have the same static type.
- Return `StaticType::Array`.

Type-checking index expressions:

- Check the collection expression.
- Check the index expression.
- If collection type is known and not `Array`, throw `TypeError(bracket, "can only index arrays")`.
- If index type is known and not `Number`, throw `TypeError(bracket, "array index must be number")`.
- Return `StaticType::Unknown`, because element type inference is out of scope.

## IR Design

Add IR operations:

```cpp
Array,
Index,
```

Recommended placement:

- `Array` near `Constant`/`MakeFunction`.
- `Index` near `Call`.

`IRInstruction::arguments` already exists and should be reused for array elements.

Add IRProgram helpers:

```cpp
IRRegister emitArray(std::vector<IRRegister> elements);
IRRegister emitIndex(IRRegister collection, IRRegister index);
```

Printing:

```text
v3 = array [v0, v1, v2]
v2 = index v0, v1
```

AST-to-IR lowering:

- Array literal: compile elements left to right, then emit `Array` with element registers.
- Index expression: compile collection, compile index, then emit `Index`.

## Runtime Value and Interpreter

Add array runtime support to `Value`:

```cpp
struct ArrayValue {
    std::size_t identity = 0;
    std::shared_ptr<std::vector<Value>> elements;
};
```

Add `Value::Type::Array`, a `Value::array(ArrayValue)` factory, and `asArray()` accessor.

Use `std::shared_ptr<std::vector<Value>>` to avoid deep copies and to support nested arrays/functions without special lifetime handling. Arrays are immutable in this phase from the language surface, so sharing the vector is safe.

Array equality should be identity-based:

```cpp
left.asArray().identity == right.asArray().identity
```

The interpreter should maintain a monotonically increasing `nextArrayIdentity_`, similar to function identity.

Executing `IROp::Array`:

- Read each element register in order.
- Store values in a new shared vector.
- Write `Value::array(ArrayValue{nextArrayIdentity_++, elements})` to the destination register.

Executing `IROp::Index`:

- Read collection and index registers.
- If collection is not `Array`, throw `IRRuntimeError("can only index arrays")`.
- If index is not `Number`, throw `IRRuntimeError("array index must be number")`.
- If index is not an integer, throw `IRRuntimeError("array index must be integer")`.
- If index is negative or out of range, throw `IRRuntimeError("array index out of range")`.
- Write the selected element value to the destination register.

Use a strict integer check. A practical implementation can use `std::trunc(index) == index` after verifying the value is finite enough for this demo. The existing language does not define infinities or NaN literals.

Array string formatting:

```cpp
[<element 0>, <element 1>, ...]
```

where each element uses `valueToString()` recursively.

## Tests

Add successful golden fixtures:

- `array_literal_print`: prints `[1, two, true, nil]`.
- `array_index_basic`: indexes `0` and `1 + 1`.
- `array_empty`: prints `[]`.
- `array_nested`: reads `xs[1][0]` from nested arrays.
- `array_function_value`: stores a function or closure in an array, retrieves it, and calls it.

Add parse-error fixtures:

- `array_literal_missing_bracket.cd`: `[1, 2;` should report missing `]` after array elements.
- `array_index_missing_bracket.cd`: `xs[0;` should report missing `]` after index.
- `array_trailing_comma.cd`: `[1,];` should report expected expression.

Add type-error fixtures:

- `index_non_array.cd`: `print 1[0];` should report `can only index arrays`.
- `index_non_number.cd`: `let xs = [1]; print xs["0"];` should report `array index must be number`.

Add runtime-error fixtures:

- `array_index_out_of_range.cd`.
- `array_index_non_integer.cd`.
- `array_dynamic_non_array.cd`, using an unknown function return value to bypass static checking.
- `array_dynamic_non_number_index.cd`, using an unknown function return value as the index to bypass static checking.

Keep existing 94 golden cases passing.

## Documentation

Update after implementation:

- `docs/language-grammar.ebnf`: add array literal and index suffix productions.
- `README.md`: document array literals, indexing, mixed elements, and current limitations.
- `docs/roadmap.md`: mark Phase 7 arrays and indexing as implemented; note mutation, length, and methods as future work.
- `AGENTS.md`: record array runtime and type-checking semantics.

## Verification

Run the full project verification before completion:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

## Risks and Mitigations

- **Parser ambiguity with calls and indexing:** handle both in the same postfix loop to preserve precedence and chaining.
- **Static vs runtime diagnostics:** statically known invalid collection/index types should be type errors; values flowing from unknown expressions should remain runtime errors.
- **Array identity:** use per-array identity for equality to avoid expensive or recursive deep equality.
- **String formatting ambiguity:** follow existing print conventions and do not quote strings inside arrays.
- **Unwanted mutation semantics:** do not add element assignment or push-like behavior in this phase.

## Approval Status

Approved design choices:

- Phase 7 scope is immutable-length array literals plus read indexing only.
- Mixed element types are allowed.
- Calls and indexing share postfix precedence and can chain.
- Runtime index errors use explicit array/index diagnostics.
- Lambda/function expressions remain future Phase 6C work and are not part of this phase.
