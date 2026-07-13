# Array Collection Helpers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add shadowable `contains`, `slice`, `copy`, and `concat` array helpers, plus unshadowed member-call sugar, through the existing `native_call` compiler and Rust VM path.

**Architecture:** Extend the native stdlib registry and `TypeChecker` with the four fixed-arity APIs, reuse `mergeArrayElementTypes` for `concat`, and lower member calls to the same native names as function calls. The Rust VM allocates fresh top-level arrays through `VM::make_array`, clones element handles for shallow-copy semantics, and reuses `Value::runtime_equals` for membership checks; no syntax, AST, IR opcode, bytecode opcode, or `.cdbc` format changes are required.

**Tech Stack:** C++17 compiler/type checker/IR lowering, Python golden and artifact tests, `.cdbc` bytecode, Rust VM and Rust unit tests.

---

## File Structure

- Modify `include/NativeStdlib.hpp`: add the four native function kinds.
- Modify `src/NativeStdlib.cpp`: register names and fixed arities.
- Modify `src/TypeChecker.cpp`: add static argument checks, result types, member-call behavior, and builtin-member conflicts.
- Modify `src/IRCompiler.cpp`: lower the four member-call forms through existing `native_call` IR.
- Modify `vm-rs/src/vm.rs`: dispatch and execute the four functions, allocate fresh arrays, and add focused unit tests.
- Create `tests/golden/collection_helpers/`: public behavior, shallow-copy, shadowing, IR, bytecode, and run goldens.
- Create `tests/golden/collection_helpers_interface/`: stable static return-type coverage through `--module-interface`.
- Create `tests/golden/type_errors/collection_helper_*`: known-type and member-name diagnostics.
- Create `tests/golden/runtime_errors/collection_helper_*`: dynamic argument and slice-bound diagnostics.
- Create `tests/bytecode_artifacts/collection_helpers/`: stable `.cdbc` native-call artifact and Rust execution parity.
- Modify `README.md`, `docs/roadmap.md`, and `AGENTS.md`: document the completed language slice and advance the roadmap.

No new production file is needed. Keep type rules in the existing native-call dispatch and runtime behavior in the existing VM native dispatch; do not create a second builtin registry or a collection abstraction layer.

---

### Task 1: Add the RED end-to-end behavior fixture

**Files:**
- Create: `tests/golden/collection_helpers/input.cd`
- Create: `tests/golden/collection_helpers/run.out`

- [ ] **Step 1: Add the source fixture**

Create `tests/golden/collection_helpers/input.cd`:

```cd
struct Box { value: number }

let xs: [number] = [1, 2, 3];
print contains(xs, 2);
print xs.contains(4);

let shared = [9];
let nested = [shared];
print nested.contains(shared);
print nested.contains([9]);

let box = Box { value: 1 };
print [box].contains(box);
print [box].contains(Box { value: 1 });

let part = slice(xs, 1, 2);
print part;
part[0] = 20;
print xs;
print xs.slice(3, 0);

let cloned = copy(xs);
cloned[0] = 10;
print xs;
print cloned;

let inner = [1];
let outer = [inner];
let outerCopy = outer.copy();
outerCopy[0].push(2);
print outer;

print concat(xs, [4, 5]);
print xs.concat([]);

let copy = fun (value) { return value; };
print copy(7);
print xs.copy();
```

- [ ] **Step 2: Add the expected Rust VM output**

Create `tests/golden/collection_helpers/run.out`:

```text
true
false
true
false
true
false
[2, 3]
[1, 2, 3]
[]
[1, 2, 3]
[10, 2, 3]
[[1, 2]]
[1, 2, 3, 4, 5]
[1, 2, 3]
7
[1, 2, 3]
```

- [ ] **Step 3: Build the current compiler**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: both commands exit 0.

- [ ] **Step 4: Verify the fixture is RED**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_helpers
```

Expected: FAIL during bytecode emission with a Type error for undefined `contains`, `slice`, `copy`, or `concat`.

- [ ] **Step 5: Commit the RED fixture**

```bash
git add tests/golden/collection_helpers/input.cd tests/golden/collection_helpers/run.out
git commit -m "test: add array collection helper behavior"
```

---

### Task 2: Add registry, static typing, member calls, and C++ lowering

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`
- Modify: `src/TypeChecker.cpp`
- Modify: `src/IRCompiler.cpp`
- Create: `tests/golden/collection_helpers_interface/input.cd`
- Create: `tests/golden/collection_helpers_interface/lib.cd`
- Create: `tests/golden/collection_helpers_interface/module-interface.out`
- Create: `tests/golden/type_errors/collection_helper_contains_value_mismatch.cd`
- Create: `tests/golden/type_errors/collection_helper_contains_value_mismatch.err`
- Create: `tests/golden/type_errors/collection_helper_contains_value_mismatch.exit`
- Create: `tests/golden/type_errors/collection_helper_slice_non_number_start.cd`
- Create: `tests/golden/type_errors/collection_helper_slice_non_number_start.err`
- Create: `tests/golden/type_errors/collection_helper_slice_non_number_start.exit`
- Create: `tests/golden/type_errors/collection_helper_concat_non_array_right.cd`
- Create: `tests/golden/type_errors/collection_helper_concat_non_array_right.err`
- Create: `tests/golden/type_errors/collection_helper_concat_non_array_right.exit`
- Create: `tests/golden/type_errors/collection_helper_copy_bad_receiver.cd`
- Create: `tests/golden/type_errors/collection_helper_copy_bad_receiver.err`
- Create: `tests/golden/type_errors/collection_helper_copy_bad_receiver.exit`
- Create: `tests/golden/type_errors/collection_helper_slice_member_wrong_arity.cd`
- Create: `tests/golden/type_errors/collection_helper_slice_member_wrong_arity.err`
- Create: `tests/golden/type_errors/collection_helper_slice_member_wrong_arity.exit`
- Create: `tests/golden/type_errors/collection_helper_copy_wrong_arity.cd`
- Create: `tests/golden/type_errors/collection_helper_copy_wrong_arity.err`
- Create: `tests/golden/type_errors/collection_helper_copy_wrong_arity.exit`
- Create: `tests/golden/type_errors/collection_helper_method_name_conflict.cd`
- Create: `tests/golden/type_errors/collection_helper_method_name_conflict.err`
- Create: `tests/golden/type_errors/collection_helper_method_name_conflict.exit`

- [ ] **Step 1: Add the static return-type fixture**

Create `tests/golden/collection_helpers_interface/input.cd`:

```cd
import "./lib.cd";
```

Create `tests/golden/collection_helpers_interface/lib.cd`:

```cd
let numbers: [number] = [1, 2];
let maybeNumbers: [number?] = [nil];
let sliced = slice(numbers, 0, 1);
let copied = numbers.copy();
let nullable = concat(numbers, maybeNumbers);
let mixed = numbers.concat(["x"]);
let present = contains(numbers, 1);
export sliced, copied, nullable, mixed, present;
```

Create `tests/golden/collection_helpers_interface/module-interface.out`:

```text
module 0 "<repo>/tests/golden/collection_helpers_interface/lib.cd"
  export value copied: [number]
  export value mixed: array
  export value nullable: [number?]
  export value present: bool
  export value sliced: [number]

module 1 entry "<repo>/tests/golden/collection_helpers_interface/input.cd"
```

- [ ] **Step 2: Add representative static-error fixtures**

Create `tests/golden/type_errors/collection_helper_contains_value_mismatch.cd`:

```cd
contains([1], "x");
```

Create its `.err` and `.exit`:

```text
Type error at 1:18: contains value expects number, got string
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_slice_non_number_start.cd`:

```cd
slice([1], "x", 1);
```

Create its `.err` and `.exit`:

```text
Type error at 1:18: slice expects number as second argument, got string
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_concat_non_array_right.cd`:

```cd
concat([1], 2);
```

Create its `.err` and `.exit`:

```text
Type error at 1:14: concat expects array as second argument, got number
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_copy_bad_receiver.cd`:

```cd
1.copy();
```

Create its `.err` and `.exit`:

```text
Type error at 1:8: copy expects array receiver, got number
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_slice_member_wrong_arity.cd`:

```cd
[1].slice(0);
```

Create its `.err` and `.exit`:

```text
Type error at 1:12: expected 2 arguments but got 1
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_copy_wrong_arity.cd`:

```cd
copy([], []);
```

Create its `.err` and `.exit`:

```text
Type error at 1:12: expected 1 arguments but got 2
```

```text
1
```

Create `tests/golden/type_errors/collection_helper_method_name_conflict.cd`:

```cd
struct Bag { value: number }

impl Bag {
  fun copy(): number { return this.value; }
}
```

Create its `.err` and `.exit`:

```text
Type error at 4:7: method `copy` conflicts with builtin member call `copy`
```

```text
1
```

- [ ] **Step 3: Verify static fixtures are RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case collection_helper
```

Expected: FAIL with undefined-variable, unknown-member, or mismatched diagnostic output because the four helpers are not registered yet.

- [ ] **Step 4: Extend the native registry**

In `include/NativeStdlib.hpp`, use this complete enum:

```cpp
enum class NativeFunctionKind {
    Push,
    Pop,
    Floor,
    Ceil,
    Sqrt,
    Str,
    Substr,
    CharAt,
    TypeOf,
    Contains,
    Slice,
    Copy,
    Concat,
};
```

In `src/NativeStdlib.cpp`, replace the table with:

```cpp
constexpr std::array<NativeFunctionSignature, 13> kNativeFunctions{{
    {"push", 2, NativeFunctionKind::Push},
    {"pop", 1, NativeFunctionKind::Pop},
    {"floor", 1, NativeFunctionKind::Floor},
    {"ceil", 1, NativeFunctionKind::Ceil},
    {"sqrt", 1, NativeFunctionKind::Sqrt},
    {"str", 1, NativeFunctionKind::Str},
    {"substr", 3, NativeFunctionKind::Substr},
    {"charAt", 2, NativeFunctionKind::CharAt},
    {"typeOf", 1, NativeFunctionKind::TypeOf},
    {"contains", 2, NativeFunctionKind::Contains},
    {"slice", 3, NativeFunctionKind::Slice},
    {"copy", 1, NativeFunctionKind::Copy},
    {"concat", 2, NativeFunctionKind::Concat},
}};
```

- [ ] **Step 5: Add shared array result-type helpers**

In the anonymous namespace at the top of `src/TypeChecker.cpp`, add:

```cpp
TypeInfo copiedArrayType(const TypeInfo& source)
{
    if (source.kind == StaticType::Array && source.elementType) {
        return arrayType(*source.elementType);
    }
    return simpleType(StaticType::Array);
}

TypeInfo concatenatedArrayType(const TypeInfo& left, const TypeInfo& right)
{
    if (left.kind != StaticType::Array || right.kind != StaticType::Array
        || !left.elementType || !right.elementType) {
        return simpleType(StaticType::Array);
    }
    std::optional<TypeInfo> merged = mergeArrayElementTypes(*left.elementType, *right.elementType);
    if (!merged) {
        return simpleType(StaticType::Array);
    }
    return arrayType(std::move(*merged));
}
```

- [ ] **Step 6: Add function-style static checks**

In `TypeChecker::checkNativeStdlibCall`, add these switch cases after `TypeOf`:

```cpp
    case NativeFunctionKind::Contains: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "contains expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }
        const TypeInfo* expectedElement = arrayArgument.type.kind == StaticType::Array
            ? arrayArgument.type.elementType.get()
            : nullptr;
        const CheckedExpression valueArgument = checkExpressionInfo(*expression.arguments[1], expectedElement);
        if (expectedElement && !compatible(*expectedElement, valueArgument.type)) {
            throw TypeError(expression.paren,
                "contains value expects " + typeInfoName(*expectedElement)
                    + ", got " + typeInfoName(valueArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::Bool)};
    }
    case NativeFunctionKind::Slice: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "slice expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }
        const CheckedExpression startArgument = checkExpressionInfo(*expression.arguments[1]);
        if (startArgument.type.kind != StaticType::Unknown && startArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "slice expects number as second argument, got " + typeInfoName(startArgument.type));
        }
        const CheckedExpression lengthArgument = checkExpressionInfo(*expression.arguments[2]);
        if (lengthArgument.type.kind != StaticType::Unknown && lengthArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "slice expects number as third argument, got " + typeInfoName(lengthArgument.type));
        }
        return CheckedExpression{copiedArrayType(arrayArgument.type)};
    }
    case NativeFunctionKind::Copy: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "copy expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }
        return CheckedExpression{copiedArrayType(arrayArgument.type)};
    }
    case NativeFunctionKind::Concat: {
        const CheckedExpression leftArgument = checkExpressionInfo(*expression.arguments[0]);
        if (leftArgument.type.kind != StaticType::Unknown && leftArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "concat expects array as first argument, got " + typeInfoName(leftArgument.type));
        }
        const CheckedExpression rightArgument = checkExpressionInfo(*expression.arguments[1]);
        if (rightArgument.type.kind != StaticType::Unknown && rightArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "concat expects array as second argument, got " + typeInfoName(rightArgument.type));
        }
        return CheckedExpression{concatenatedArrayType(leftArgument.type, rightArgument.type)};
    }
```

- [ ] **Step 7: Add member-call static checks**

In `TypeChecker::checkMemberCall`, add these branches after the `pop` branch and before `len`:

```cpp
    if (name == "contains") {
        expectArity(1);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "contains expects array receiver, got " + typeInfoName(receiver.type));
        }
        const TypeInfo* expectedElement = receiver.type.kind == StaticType::Array
            ? receiver.type.elementType.get()
            : nullptr;
        const CheckedExpression value = checkExpressionInfo(*expression.arguments[0], expectedElement);
        if (expectedElement && !compatible(*expectedElement, value.type)) {
            throw TypeError(expression.paren,
                "contains value expects " + typeInfoName(*expectedElement) + ", got " + typeInfoName(value.type));
        }
        return CheckedExpression{simpleType(StaticType::Bool)};
    }

    if (name == "slice") {
        expectArity(2);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "slice expects array receiver, got " + typeInfoName(receiver.type));
        }
        const CheckedExpression start = checkExpressionInfo(*expression.arguments[0]);
        if (start.type.kind != StaticType::Unknown && start.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "slice expects number as first argument, got " + typeInfoName(start.type));
        }
        const CheckedExpression length = checkExpressionInfo(*expression.arguments[1]);
        if (length.type.kind != StaticType::Unknown && length.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "slice expects number as second argument, got " + typeInfoName(length.type));
        }
        return CheckedExpression{copiedArrayType(receiver.type)};
    }

    if (name == "copy") {
        expectArity(0);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "copy expects array receiver, got " + typeInfoName(receiver.type));
        }
        return CheckedExpression{copiedArrayType(receiver.type)};
    }

    if (name == "concat") {
        expectArity(1);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "concat expects array receiver, got " + typeInfoName(receiver.type));
        }
        const CheckedExpression right = checkExpressionInfo(*expression.arguments[0]);
        if (right.type.kind != StaticType::Unknown && right.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "concat expects array as first argument, got " + typeInfoName(right.type));
        }
        return CheckedExpression{concatenatedArrayType(receiver.type, right.type)};
    }
```

- [ ] **Step 8: Reserve the new builtin member names**

Change `TypeChecker::isBuiltinMemberName` to:

```cpp
bool TypeChecker::isBuiltinMemberName(const std::string& name) const
{
    return name == "push" || name == "pop" || name == "len"
        || name == "substr" || name == "charAt"
        || name == "contains" || name == "slice" || name == "copy" || name == "concat";
}
```

- [ ] **Step 9: Lower the member forms through `native_call`**

In `IRCompiler::emitMemberCall`, extend the native member-name condition to:

```cpp
    if (expression.name.lexeme == "push"
        || expression.name.lexeme == "pop"
        || expression.name.lexeme == "substr"
        || expression.name.lexeme == "charAt"
        || expression.name.lexeme == "contains"
        || expression.name.lexeme == "slice"
        || expression.name.lexeme == "copy"
        || expression.name.lexeme == "concat") {
        std::vector<IRRegister> arguments;
        arguments.push_back(receiver);
        for (const auto& argument : expression.arguments) {
            arguments.push_back(compileExpression(*argument));
        }
        return ir_.emitNativeCall(expression.name.lexeme, std::move(arguments));
    }
```

- [ ] **Step 10: Build and verify the static/compiler slice is GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case collection_helper
```

Expected: build exits 0 and all `collection_helper` type/interface checks pass. The end-to-end run fixture remains RED until the Rust VM dispatch exists.

- [ ] **Step 11: Commit the compiler slice**

```bash
git add include/NativeStdlib.hpp src/NativeStdlib.cpp src/TypeChecker.cpp src/IRCompiler.cpp tests/golden/collection_helpers_interface tests/golden/type_errors/collection_helper_*
git commit -m "feat: type check array collection helpers"
```

---

### Task 3: Execute helpers in the Rust VM

**Files:**
- Modify: `vm-rs/src/vm.rs`

- [ ] **Step 1: Add focused VM tests before implementation**

Append this test module to `vm-rs/src/vm.rs`:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn empty_program() -> Program {
        Program {
            constants: Vec::new(),
            names: Vec::new(),
            main: FunctionBody {
                registers: 0,
                instructions: Vec::new(),
            },
            functions: Vec::new(),
        }
    }

    fn array_elements(value: &Value) -> Vec<Value> {
        let Value::Array(array) = value else {
            panic!("expected array");
        };
        array.elements.borrow().clone()
    }

    #[test]
    fn native_collection_helpers_query_and_copy_shallowly() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let shared = vm.make_array(vec![Value::number(9.0)]);
        let source = vm.make_array(vec![Value::number(1.0), shared.clone(), Value::number(3.0)]);
        let distinct = vm.make_array(vec![Value::number(9.0)]);

        let contains_shared = vm
            .execute_native_call("contains", vec![source.clone(), shared.clone()])
            .expect("contains succeeds");
        let contains_distinct = vm
            .execute_native_call("contains", vec![source.clone(), distinct])
            .expect("contains succeeds");
        assert!(matches!(contains_shared, Value::Bool(true)));
        assert!(matches!(contains_distinct, Value::Bool(false)));

        let sliced = vm
            .execute_native_call(
                "slice",
                vec![source.clone(), Value::number(1.0), Value::number(2.0)],
            )
            .expect("slice succeeds");
        let copied = vm
            .execute_native_call("copy", vec![source.clone()])
            .expect("copy succeeds");
        let concatenated = vm
            .execute_native_call("concat", vec![sliced.clone(), copied.clone()])
            .expect("concat succeeds");

        assert_eq!(array_elements(&sliced).len(), 2);
        assert_eq!(array_elements(&copied).len(), 3);
        assert_eq!(array_elements(&concatenated).len(), 5);
        assert!(!source.runtime_equals(&copied));
        let source_elements = array_elements(&source);
        let copied_elements = array_elements(&copied);
        assert!(source_elements[1].runtime_equals(&copied_elements[1]));
    }

    #[test]
    fn native_collection_helpers_validate_slice_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = vm.make_array(vec![Value::number(1.0)]);

        let empty = vm
            .execute_native_call(
                "slice",
                vec![source.clone(), Value::number(1.0), Value::number(0.0)],
            )
            .expect("empty end slice succeeds");
        assert!(array_elements(&empty).is_empty());

        for (start, length, expected) in [
            (f64::NAN, 0.0, "slice expects integer start offset"),
            (-1.0, 0.0, "slice start offset out of bounds"),
            (0.0, f64::INFINITY, "slice expects integer length"),
            (0.0, 2.0, "slice length out of bounds"),
        ] {
            let error = vm
                .execute_native_call(
                    "slice",
                    vec![source.clone(), Value::number(start), Value::number(length)],
                )
                .expect_err("slice should fail");
            assert_eq!(error.message, expected);
        }
    }

    #[test]
    fn native_collection_helpers_validate_arity_and_types() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        assert_eq!(
            vm.execute_native_call("contains", vec![]).unwrap_err().message,
            "contains expects 2 arguments"
        );
        assert_eq!(
            vm.execute_native_call("slice", vec![]).unwrap_err().message,
            "slice expects 3 arguments"
        );
        assert_eq!(
            vm.execute_native_call("copy", vec![]).unwrap_err().message,
            "copy expects 1 argument"
        );
        assert_eq!(
            vm.execute_native_call("concat", vec![]).unwrap_err().message,
            "concat expects 2 arguments"
        );
        assert_eq!(
            vm.execute_native_call("copy", vec![Value::number(1.0)]).unwrap_err().message,
            "copy expects array as first argument"
        );
        assert_eq!(
            vm.execute_native_call("concat", vec![Value::Nil, Value::Nil]).unwrap_err().message,
            "concat expects array as first argument"
        );
    }
}
```

- [ ] **Step 2: Verify the VM tests are RED**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml native_collection_helpers
```

Expected: tests compile but fail with `unknown native stdlib function` because dispatch cases are absent.

- [ ] **Step 3: Allow native dispatch to allocate arrays**

Change the receiver on `VM::execute_native_call` from `&self` to `&mut self`:

```rust
    fn execute_native_call(
        &mut self,
        name: &str,
        arguments: Vec<Value>,
    ) -> Result<Value, RuntimeError> {
```

Add these match arms after `typeOf`:

```rust
            "contains" => self.execute_native_contains(arguments),
            "slice" => self.execute_native_slice(arguments),
            "copy" => self.execute_native_copy(arguments),
            "concat" => self.execute_native_concat(arguments),
```

- [ ] **Step 4: Implement membership and shallow-copy helpers**

Add these methods after the existing native stdlib implementations:

```rust
    fn execute_native_contains(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("contains expects 2 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("contains expects array as first argument"));
        };
        let found = array
            .elements
            .borrow()
            .iter()
            .any(|element| element.runtime_equals(&arguments[1]));
        Ok(Value::boolean(found))
    }

    fn execute_native_slice(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 3 {
            return Err(RuntimeError::new("slice expects 3 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("slice expects array as first argument"));
        };
        let Value::Number(start_value) = &arguments[1] else {
            return Err(RuntimeError::new("slice expects number as second argument"));
        };
        let Value::Number(length_value) = &arguments[2] else {
            return Err(RuntimeError::new("slice expects number as third argument"));
        };

        let source_len = array.elements.borrow().len();
        let start = Self::checked_integer_index(
            *start_value,
            "slice expects integer start offset",
            "slice start offset out of bounds",
            source_len,
        )?;
        let length = Self::checked_integer_index(
            *length_value,
            "slice expects integer length",
            "slice length out of bounds",
            source_len,
        )?;
        if length > source_len - start {
            return Err(RuntimeError::new("slice length out of bounds"));
        }
        let elements = array.elements.borrow()[start..start + length].to_vec();
        Ok(self.make_array(elements))
    }

    fn execute_native_copy(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("copy expects 1 argument"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("copy expects array as first argument"));
        };
        let elements = array.elements.borrow().clone();
        Ok(self.make_array(elements))
    }

    fn execute_native_concat(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("concat expects 2 arguments"));
        }
        let Value::Array(left) = &arguments[0] else {
            return Err(RuntimeError::new("concat expects array as first argument"));
        };
        let Value::Array(right) = &arguments[1] else {
            return Err(RuntimeError::new("concat expects array as second argument"));
        };
        let mut elements = left.elements.borrow().clone();
        elements.extend(right.elements.borrow().iter().cloned());
        Ok(self.make_array(elements))
    }
```

- [ ] **Step 5: Verify VM unit tests are GREEN**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml native_collection_helpers
```

Expected: 3 matching tests pass, 0 fail.

- [ ] **Step 6: Verify the end-to-end behavior fixture is GREEN**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_helpers
```

Expected: `collection_helpers emit` and `collection_helpers rust-run` pass.

- [ ] **Step 7: Commit Rust VM execution**

```bash
git add vm-rs/src/vm.rs
git commit -m "feat: execute array collection helpers"
```

---

### Task 4: Add dynamic runtime-error coverage

**Files:**
- Create: `tests/golden/runtime_errors/collection_helper_contains_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/collection_helper_contains_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_contains_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/collection_helper_copy_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/collection_helper_copy_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_copy_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/collection_helper_concat_dynamic_bad_right.cd`
- Create: `tests/golden/runtime_errors/collection_helper_concat_dynamic_bad_right.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_concat_dynamic_bad_right.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_fractional_start.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_fractional_start.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_fractional_start.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_start.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_start.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_start.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_length.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_length.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_dynamic_non_number_length.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_negative_start.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_negative_start.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_negative_start.exit`
- Create: `tests/golden/runtime_errors/collection_helper_slice_length_out_of_bounds.cd`
- Create: `tests/golden/runtime_errors/collection_helper_slice_length_out_of_bounds.run.err`
- Create: `tests/golden/runtime_errors/collection_helper_slice_length_out_of_bounds.exit`

- [ ] **Step 1: Add invalid dynamic argument fixtures**

Create the four `.cd` files:

```cd
fun id(value) { return value; }
contains(id(1), 1);
```

```cd
fun id(value) { return value; }
slice(id(1), 0, 0);
```

```cd
fun id(value) { return value; }
copy(id(1));
```

```cd
fun id(value) { return value; }
concat([], id(1));
```

Create the matching `.run.err` files in the same order:

```text
Runtime error: contains expects array as first argument
```

```text
Runtime error: slice expects array as first argument
```

```text
Runtime error: copy expects array as first argument
```

```text
Runtime error: concat expects array as second argument
```

Create every matching `.exit` file with:

```text
1
```

- [ ] **Step 2: Add slice boundary fixtures**

Create the five `.cd` files:

```cd
fun id(value) { return value; }
slice([1], id("x"), 0);
```

```cd
fun id(value) { return value; }
slice([1], 0, id(false));
```

```cd
slice([1], 0.5, 0);
```

```cd
slice([1], -1, 0);
```

```cd
slice([1], 1, 1);
```

Create their matching `.run.err` files:

```text
Runtime error: slice expects number as second argument
```

```text
Runtime error: slice expects number as third argument
```

```text
Runtime error: slice expects integer start offset
```

```text
Runtime error: slice start offset out of bounds
```

```text
Runtime error: slice length out of bounds
```

Create every matching `.exit` file with:

```text
1
```

- [ ] **Step 3: Run focused C++ golden and Rust VM runtime checks**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case collection_helper
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_helper_contains_dynamic_non_array --case collection_helper_slice_dynamic_non_array --case collection_helper_copy_dynamic_non_array --case collection_helper_concat_dynamic_bad_right --case collection_helper_slice_dynamic_non_number_start --case collection_helper_slice_dynamic_non_number_length --case collection_helper_slice_fractional_start --case collection_helper_slice_negative_start --case collection_helper_slice_length_out_of_bounds
```

Expected: all selected checks pass; runtime-error cases produce no stdout and exit 1.

- [ ] **Step 4: Commit runtime diagnostics**

```bash
git add tests/golden/runtime_errors/collection_helper_*
git commit -m "test: cover collection helper runtime errors"
```

---

### Task 5: Add stable IR, bytecode, and `.cdbc` artifact coverage

**Files:**
- Create: `tests/golden/collection_helpers/ast.out`
- Create: `tests/golden/collection_helpers/ir.out`
- Create: `tests/golden/collection_helpers/bytecode.out`
- Create: `tests/bytecode_artifacts/collection_helpers/input.cd`
- Create: `tests/bytecode_artifacts/collection_helpers/expected.cdbc`
- Create: `tests/bytecode_artifacts/collection_helpers/run.out`

- [ ] **Step 1: Generate only the intentional compiler goldens for the behavior fixture**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update --update-missing --case collection_helpers
```

Expected: creates `ast.out`, `ir.out`, `bytecode.out`, and `module-interface.out` beside the existing fixture. Remove the empty module-interface output with:

```diff
*** Begin Patch
*** Delete File: tests/golden/collection_helpers/module-interface.out
*** End Patch
```

Keep and review the other three compiler outputs, confirming all four helpers use `native_call` and no new opcode appears.

- [ ] **Step 2: Add the compact artifact source and output**

Create `tests/bytecode_artifacts/collection_helpers/input.cd`:

```cd
let xs = [1, 2];
print contains(xs, 2);
print slice(xs, 0, 1);
print copy(xs);
print concat(xs, [3]);
```

Create `tests/bytecode_artifacts/collection_helpers/run.out`:

```text
true
[1]
[1, 2]
[1, 2, 3]
```

- [ ] **Step 3: Add the exact `.cdbc` expectation**

Create `tests/bytecode_artifacts/collection_helpers/expected.cdbc`:

```text
cdbc 0.1

constants:
  c0 = number 1
  c1 = number 2
  c2 = number 2
  c3 = number 0
  c4 = number 1
  c5 = number 3

names:
  n0 = "xs#0"
  n1 = "xs#0"
  n2 = "contains"
  n3 = "xs#0"
  n4 = "slice"
  n5 = "xs#0"
  n6 = "copy"
  n7 = "xs#0"
  n8 = "concat"

main registers=16:
  r0 = constant c0
  r1 = constant c1
  r2 = array [r0, r1]
  store_var n0, r2
  r3 = load_var n1
  r4 = constant c2
  r5 = native_call n2 [r3, r4]
  print r5
  r6 = load_var n3
  r7 = constant c3
  r8 = constant c4
  r9 = native_call n4 [r6, r7, r8]
  print r9
  r10 = load_var n5
  r11 = native_call n6 [r10]
  print r11
  r12 = load_var n7
  r13 = constant c5
  r14 = array [r13]
  r15 = native_call n8 [r12, r14]
  print r15
```

- [ ] **Step 4: Verify compiler, artifact dump, and Rust execution parity**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case collection_helpers
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_helpers
```

Expected: all selected checks pass. If the generated artifact differs only because the established name/constant-table ordering was calculated differently, replace `expected.cdbc` with the compiler-emitted text, inspect the diff, and rerun both artifact commands before committing.

- [ ] **Step 5: Commit compiler and artifact goldens**

```bash
git add tests/golden/collection_helpers tests/bytecode_artifacts/collection_helpers
git commit -m "test: cover collection helper bytecode parity"
```

---

### Task 6: Document the completed collection helper slice

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Document the public API in README**

Add `contains`, `slice`, `copy`, and `concat` to the builtin summary near the top of `README.md`. After the existing `push`/`pop` section, add:

```markdown
The non-mutating array collection helpers are `contains(array, value)`,
`slice(array, start, length)`, `copy(array)`, and `concat(left, right)`.
`contains` uses the language's existing equality rules. `slice`, `copy`, and
`concat` allocate a new top-level array and shallow-copy elements, so nested
arrays, structs, and closures remain shared. Function-style names are shadowable;
the corresponding member forms `array.contains(value)`,
`array.slice(start, length)`, `array.copy()`, and `array.concat(right)` are
builtin member sugar and are not shadowed by lexical bindings.
```

- [ ] **Step 2: Advance the roadmap**

In `docs/roadmap.md`:

- mark the first non-higher-order collection helper slice implemented under Phase 13;
- record the implemented four-function API and shallow-copy convention;
- change both dependency diagrams so the first active item is `runtime diagnostics`;
- retain higher-order helpers after generics and leave `len` migration as optional cleanup.

Use this status text in Phase 13:

```markdown
- The first non-higher-order collection slice is implemented with `contains`,
  `slice`, `copy`, and `concat` in function and member forms. Returned arrays use
  shallow-copy semantics and preserve existing static element information where
  possible.
```

- [ ] **Step 3: Update project memory**

In `AGENTS.md`, extend the array semantics bullet with:

```markdown
The non-mutating array helpers `contains`, `slice`, `copy`, and `concat` are
available in shadowable function form and unshadowed member-call sugar. `slice`,
`copy`, and `concat` return fresh top-level arrays with shallow element copies;
`contains` uses existing runtime equality.
```

- [ ] **Step 4: Verify docs agree with implemented behavior**

Run:

```bash
rg -n 'contains|slice|copy|concat|runtime diagnostics' README.md docs/roadmap.md AGENTS.md
git diff --check
```

Expected: all four API names appear in README and AGENTS, the roadmap's next active step is runtime diagnostics, and `git diff --check` exits 0.

- [ ] **Step 5: Commit docs**

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document array collection helpers"
```

---

### Task 7: Run the full verification suite

**Files:**
- No source changes expected.
- Remove generated `tests/__pycache__/` if Python creates it.

- [ ] **Step 1: Configure and build from the repository root**

```bash
cmake -S . -B build
cmake --build build
```

Expected: both commands exit 0.

- [ ] **Step 2: Run all CTest targets**

```bash
ctest --test-dir build --output-on-failure
```

Expected: 100% tests passed, 0 failed.

- [ ] **Step 3: Run the complete Python and cross-runtime suites**

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: every command exits 0 and reports 0 failed.

- [ ] **Step 4: Run all Rust unit tests**

```bash
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: all Rust tests pass, including the three `native_collection_helpers_*` tests.

- [ ] **Step 5: Clean Python cache and inspect final state**

```bash
rm -rf tests/__pycache__
git status --short
git log --oneline -8
```

Expected: no generated cache is tracked or left behind; the worktree is clean and the focused commits from this plan are visible.

If any verification command fails, stop, diagnose with `superpowers:systematic-debugging`, add the smallest missing regression coverage, rerun the affected focused command, then rerun this full suite before claiming completion.
