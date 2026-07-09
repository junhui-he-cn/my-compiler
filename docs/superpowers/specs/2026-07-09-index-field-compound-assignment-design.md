# Index and Field Compound Assignment Design

## Goal

Extend numeric compound assignment from variable-only targets to array index and struct field targets. This is Phase 15E after variable compound assignment and struct methods.

The first slice should support:

```cd
array[index] += value;
array[index] -= value;
array[index] *= value;
array[index] /= value;

object.field += value;
this.value += delta;
```

It should preserve current numeric-only compound-assignment semantics, existing mutation behavior, bytecode/Rust VM parity, and assignment expression result behavior.

## User-Facing Semantics

Compound assignment on index and field targets behaves like reading the target, applying a numeric binary operator, writing back the result, and evaluating to the assigned value.

Examples:

```cd
let xs = [1];
xs[0] += 4;
print xs[0];      // 5
print xs[0] *= 2; // 10
```

```cd
struct Counter { value: number }
let c: Counter = Counter { value: 1 };
c.value += 4;
print c.value; // 5
```

```cd
struct Counter { value: number }
impl Counter {
  fun inc(delta: number): number {
    this.value += delta;
    return this.value;
  }
}
```

Numeric-only means both the old target value and the right-hand value must be numbers. `+=` does not perform string concatenation in compound assignment, even though plain `+` supports strings.

## Evaluation Order

Evaluation order should be deterministic and side-effect safe:

- For `collection[index] += rhs`:
  1. Evaluate `collection` exactly once.
  2. Evaluate `index` exactly once.
  3. Read the old element value.
  4. Evaluate `rhs`.
  5. Compute and write the new value.
  6. Return the assigned value.

- For `object.field += rhs`:
  1. Evaluate `object` exactly once.
  2. Read the old field value.
  3. Evaluate `rhs`.
  4. Compute and write the new value.
  5. Return the assigned value.

This matches existing assignment target evaluation style and avoids duplicated receiver/collection side effects.

## Scope

Included:

- Parse compound assignment for `IndexExpr` and `FieldAccessExpr` targets.
- Add explicit AST nodes for index and field compound assignment.
- Type-check numeric-only rules for known element/field types and known RHS types.
- Reuse runtime numeric assertions for unknown old values and RHS values.
- Lower to existing IR operations: index/field read, numeric assert, binary op, assign index/field.
- Preserve current variable compound assignment behavior.
- Replace current parse-error fixtures for index/field compound assignment targets with success and type/runtime coverage.
- Add bytecode artifact and Rust VM parity tests.
- Update README, EBNF, roadmap, and AGENTS.

Excluded:

- Compound assignment on arbitrary expressions such as `(a + b) += 1`.
- Compound assignment on member calls, function calls, literals, or grouped non-target expressions.
- String concatenating `+=`.
- User-overloadable compound operators.
- New bytecode opcodes.
- `++` or `--`.

## AST

Keep existing variable compound assignment:

```cpp
struct CompoundAssignExpr final : Expr {
    Token name;
    Token op;
    ExprPtr value;
};
```

Add two new expression nodes:

```cpp
struct IndexCompoundAssignExpr final : Expr {
    ExprPtr collection;
    Token bracket;
    ExprPtr index;
    Token op;
    ExprPtr value;
};

struct FieldCompoundAssignExpr final : Expr {
    ExprPtr object;
    Token name;
    Token op;
    ExprPtr value;
};
```

Suggested AST print format:

```text
(+= (index xs 0) 1)
(+= (field person age) 1)
```

Use the actual operator lexeme for all four compound operators.

## Parser

Current parsing rejects compound assignment unless the target is a `VariableExpr`. Extend the target handling:

- `VariableExpr` -> existing `CompoundAssignExpr`.
- `IndexExpr` -> `IndexCompoundAssignExpr`.
- `FieldAccessExpr` -> `FieldCompoundAssignExpr`.
- All other targets -> same parse error: `invalid compound assignment target`.

Plain `=` assignment remains unchanged and continues to support variable, index, and field targets.

Assignment remains right-associative, so this is valid:

```cd
xs[0] += value = 1;
```

if the RHS assignment is otherwise valid.

## Type Checking

### Index compound assignment

For `collection[index] += value`:

- Type-check `collection`.
- Type-check `index`.
- Type-check `value`.
- If collection type is known and not array, report a type error.
- If index type is known and not number, report a type error.
- If collection element type is known and not number, report a type error.
- If value type is known and not number, report a type error.
- Result type is `number`.

If element type is unknown, runtime numeric assertion handles old element type. If RHS type is unknown, runtime numeric assertion handles RHS type.

Suggested diagnostics:

```text
Type error at <op>: compound assignment target must be number, got string
Type error at <op>: compound assignment value must be number, got string
Type error at <bracket>: can only assign array elements
Type error at <bracket>: array index must be number
```

### Field compound assignment

For `object.field += value`:

- Type-check `object`.
- Type-check `value`.
- If object type is known and not struct, report a type error.
- If object type is known named struct, look up the field statically.
- If known field type is not number, report a type error.
- If value type is known and not number, report a type error.
- Result type is `number`.

If object type is unknown or anonymous struct, runtime field lookup and runtime numeric assertion handle missing/non-number fields.

Suggested diagnostics:

```text
Type error at <op>: compound assignment target must be number, got string
Type error at <op>: compound assignment value must be number, got string
Type error at <field>: can only assign fields on structs
Type error at <field>: struct `Person` has no field `missing`
```

## Lowering

Reuse existing `IROp` operations and `assert_number` behavior.

### Index target lowering

Conceptual lowering:

```text
collectionReg = compile(collection)
indexReg = compile(index)
oldReg = emitIndex(collectionReg, indexReg)
checkedOld = emitAssertNumber(oldReg, "`+=` expects number target")
rhsReg = compile(value)
checkedRhs = emitAssertNumber(rhsReg, "`+=` expects number value")
result = emitBinary(compoundAssignmentOp(op), checkedOld, checkedRhs)
emitAssignIndex(collectionReg, indexReg, result)
return result
```

### Field target lowering

Conceptual lowering:

```text
objectReg = compile(object)
oldReg = emitField(objectReg, fieldName)
checkedOld = emitAssertNumber(oldReg, "`+=` expects number target")
rhsReg = compile(value)
checkedRhs = emitAssertNumber(rhsReg, "`+=` expects number value")
result = emitBinary(compoundAssignmentOp(op), checkedOld, checkedRhs)
emitAssignField(objectReg, fieldName, result)
return result
```

The returned value is the computed result. The assign operation can be emitted for its mutation side effect; if it also returns a value, that return value does not need to be used as long as it is semantically identical to `result`.

No bytecode or Rust VM opcode changes should be needed because existing IR lowering already supports index/field reads, assignments, asserts, and numeric binary operations.

## Tests

Success fixtures:

- Array index compound assignment for all four operators.
- Array index compound assignment result used directly in `print`.
- Struct field compound assignment for all four operators.
- Struct method using `this.value += delta`.
- Receiver/collection evaluation once where practical with a function returning an array/struct alias.

Type-error fixtures:

- Index target collection statically known non-array.
- Index target index statically known non-number.
- Index target known element type non-number.
- Index target RHS statically known non-number.
- Field target object statically known non-struct.
- Field target known field type non-number.
- Field target unknown field on named struct.
- Field target RHS statically known non-number.
- Invalid compound assignment target remains parse error for non-target expressions.

Runtime-error fixtures:

- Dynamic array element old value non-number.
- Dynamic field old value non-number.
- Dynamic non-array index target.
- Dynamic non-struct field target.

Bytecode/Rust VM coverage:

- At least one array index compound fixture.
- At least one struct field compound fixture.
- At least one `this.field += value` method fixture.

## Documentation

Update:

- `README.md`: document `array[index] += value` and `object.field += value` as numeric-only compound assignments.
- `docs/language-grammar.ebnf`: update compound assignment target description from variable-only to variable/index/field targets.
- `docs/roadmap.md`: mark Phase 15E implemented after completion.
- `AGENTS.md`: update current language semantics and limitations.

## Compatibility Notes

Existing parse-error fixtures for index/field compound assignment targets should be removed or converted because those targets become valid syntax:

- `tests/golden/parse_errors/compound_assignment_index_target.*`
- `tests/golden/parse_errors/compound_assignment_field_target.*`

Variable compound assignment remains unchanged. Plain assignment remains unchanged.
