# Native Stdlib String Builtins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add shadowable native stdlib string builtins `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` using the existing `native_call` IR/bytecode path.

**Architecture:** Extend the existing `NativeStdlib` registry with three new function kinds and add static checks in `TypeChecker::checkNativeStdlibCall`. The IR compiler and bytecode compiler already lower registered unshadowed native calls through `native_call`, so implementation work focuses on C++ IR interpreter and Rust VM native dispatch plus tests and docs.

**Tech Stack:** C++17 compiler/type checker/IR interpreter, Python golden tests, `.cdbc` bytecode artifacts, Rust VM executor.

---

## File Structure

- Modify `include/NativeStdlib.hpp`: add `Str`, `Substr`, and `CharAt` native function kinds.
- Modify `src/NativeStdlib.cpp`: register `str`, `substr`, and `charAt` with arities 1, 3, and 2.
- Modify `src/TypeChecker.cpp`: add native stdlib static checks and return type rules for the new functions.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute the string native calls in the C++ `--run` path.
- Modify `vm-rs/src/vm.rs`: execute the same string native calls in the Rust VM.
- Modify `tests/run_rust_vm_tests.py`: include the new success golden in Rust VM parity.
- Create `tests/golden/native_stdlib_strings/`: success fixture with AST, IR, bytecode, and run outputs.
- Create `tests/golden/type_errors/*str*`, `*substr*`, and `*char_at*`: static diagnostics.
- Create `tests/golden/runtime_errors/*substr*` and `*char_at*`: runtime diagnostics.
- Create `tests/bytecode_artifacts/native_stdlib_strings/`: `.cdbc` artifact and run output.
- Modify `README.md`, `docs/roadmap.md`, and `AGENTS.md`: document implemented string builtins.

---

### Task 1: RED success fixture for string builtins

**Files:**
- Create: `tests/golden/native_stdlib_strings/input.cd`
- Create: `tests/golden/native_stdlib_strings/ast.out`
- Create: `tests/golden/native_stdlib_strings/run.out`

- [ ] **Step 1: Add success input**

Create `tests/golden/native_stdlib_strings/input.cd`:

```cd
print str(123);
print str(true);
print str(nil);
print str([1, "x"]);
print substr("hello", 1, 3);
print substr("hello", 0, 5);
print substr("hello", 5, 0);
print charAt("hello", 0);
print charAt("hello", 4);
```

- [ ] **Step 2: Add AST expectation**

Create `tests/golden/native_stdlib_strings/ast.out`:

```text
Program
  Print (call str 123)
  Print (call str true)
  Print (call str nil)
  Print (call str (array 1 "x"))
  Print (call substr "hello" 1 3)
  Print (call substr "hello" 0 5)
  Print (call substr "hello" 5 0)
  Print (call charAt "hello" 0)
  Print (call charAt "hello" 4)
```

- [ ] **Step 3: Add run expectation**

Create `tests/golden/native_stdlib_strings/run.out` with one intentional blank line after `hello` for the empty substring:

```text
123
true
nil
[1, x]
ell
hello

h
o
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_strings` fails because unshadowed `str`, `substr`, and `charAt` are not registered yet.

- [ ] **Step 5: Commit RED fixture**

```bash
git add tests/golden/native_stdlib_strings/input.cd tests/golden/native_stdlib_strings/ast.out tests/golden/native_stdlib_strings/run.out
git commit -m "test: add native stdlib string fixture"
```

---

### Task 2: Registry and static type checking

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/str_wrong_arity.cd`
- Create: `tests/golden/type_errors/str_wrong_arity.err`
- Create: `tests/golden/type_errors/str_wrong_arity.exit`
- Create: `tests/golden/type_errors/substr_wrong_arity.cd`
- Create: `tests/golden/type_errors/substr_wrong_arity.err`
- Create: `tests/golden/type_errors/substr_wrong_arity.exit`
- Create: `tests/golden/type_errors/substr_non_string_static.cd`
- Create: `tests/golden/type_errors/substr_non_string_static.err`
- Create: `tests/golden/type_errors/substr_non_string_static.exit`
- Create: `tests/golden/type_errors/substr_non_number_start_static.cd`
- Create: `tests/golden/type_errors/substr_non_number_start_static.err`
- Create: `tests/golden/type_errors/substr_non_number_start_static.exit`
- Create: `tests/golden/type_errors/substr_non_number_length_static.cd`
- Create: `tests/golden/type_errors/substr_non_number_length_static.err`
- Create: `tests/golden/type_errors/substr_non_number_length_static.exit`
- Create: `tests/golden/type_errors/char_at_non_string_static.cd`
- Create: `tests/golden/type_errors/char_at_non_string_static.err`
- Create: `tests/golden/type_errors/char_at_non_string_static.exit`
- Create: `tests/golden/type_errors/char_at_non_number_index_static.cd`
- Create: `tests/golden/type_errors/char_at_non_number_index_static.err`
- Create: `tests/golden/type_errors/char_at_non_number_index_static.exit`
- Create: `tests/golden/type_errors/char_at_shadowed_call_non_function.cd`
- Create: `tests/golden/type_errors/char_at_shadowed_call_non_function.err`
- Create: `tests/golden/type_errors/char_at_shadowed_call_non_function.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/str_wrong_arity.cd`:

```cd
str();
```

Create `tests/golden/type_errors/str_wrong_arity.err`:

```text
Type error at 1:5: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/str_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/substr_wrong_arity.cd`:

```cd
substr("abc", 1);
```

Create `tests/golden/type_errors/substr_wrong_arity.err`:

```text
Type error at 1:16: expected 3 arguments but got 2
```

Create `tests/golden/type_errors/substr_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/substr_non_string_static.cd`:

```cd
substr(123, 0, 1);
```

Create `tests/golden/type_errors/substr_non_string_static.err`:

```text
Type error at 1:17: substr expects string as first argument, got number
```

Create `tests/golden/type_errors/substr_non_string_static.exit`:

```text
1
```

Create `tests/golden/type_errors/substr_non_number_start_static.cd`:

```cd
substr("abc", "x", 1);
```

Create `tests/golden/type_errors/substr_non_number_start_static.err`:

```text
Type error at 1:21: substr expects number as second argument, got string
```

Create `tests/golden/type_errors/substr_non_number_start_static.exit`:

```text
1
```

Create `tests/golden/type_errors/substr_non_number_length_static.cd`:

```cd
substr("abc", 0, false);
```

Create `tests/golden/type_errors/substr_non_number_length_static.err`:

```text
Type error at 1:23: substr expects number as third argument, got bool
```

Create `tests/golden/type_errors/substr_non_number_length_static.exit`:

```text
1
```

Create `tests/golden/type_errors/char_at_non_string_static.cd`:

```cd
charAt(true, 0);
```

Create `tests/golden/type_errors/char_at_non_string_static.err`:

```text
Type error at 1:15: charAt expects string as first argument, got bool
```

Create `tests/golden/type_errors/char_at_non_string_static.exit`:

```text
1
```

Create `tests/golden/type_errors/char_at_non_number_index_static.cd`:

```cd
charAt("abc", "x");
```

Create `tests/golden/type_errors/char_at_non_number_index_static.err`:

```text
Type error at 1:18: charAt expects number as second argument, got string
```

Create `tests/golden/type_errors/char_at_non_number_index_static.exit`:

```text
1
```

Create `tests/golden/type_errors/char_at_shadowed_call_non_function.cd`:

```cd
let charAt = 123;
charAt("x", 0);
```

Create `tests/golden/type_errors/char_at_shadowed_call_non_function.err`:

```text
Type error at 2:14: can only call functions
```

Create `tests/golden/type_errors/char_at_shadowed_call_non_function.exit`:

```text
1
```

- [ ] **Step 2: Verify RED for static fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: unshadowed string builtin fixtures fail with undefined-variable diagnostics; the shadowing fixture reports normal non-function call behavior.

- [ ] **Step 3: Extend native function kind enum**

In `include/NativeStdlib.hpp`, change the enum to:

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
};
```

- [ ] **Step 4: Register the three functions**

In `src/NativeStdlib.cpp`, change the table declaration to contain eight entries:

```cpp
constexpr std::array<NativeFunctionSignature, 8> kNativeFunctions{{
    {"push", 2, NativeFunctionKind::Push},
    {"pop", 1, NativeFunctionKind::Pop},
    {"floor", 1, NativeFunctionKind::Floor},
    {"ceil", 1, NativeFunctionKind::Ceil},
    {"sqrt", 1, NativeFunctionKind::Sqrt},
    {"str", 1, NativeFunctionKind::Str},
    {"substr", 3, NativeFunctionKind::Substr},
    {"charAt", 2, NativeFunctionKind::CharAt},
}};
```

- [ ] **Step 5: Add type-checking branches**

In `src/TypeChecker.cpp`, extend `TypeChecker::checkNativeStdlibCall` after the math branch with these cases:

```cpp
    case NativeFunctionKind::Str:
        checkExpressionInfo(*expression.arguments[0]);
        return CheckedExpression{simpleType(StaticType::String)};
    case NativeFunctionKind::Substr: {
        const CheckedExpression stringArgument = checkExpressionInfo(*expression.arguments[0]);
        if (stringArgument.type.kind != StaticType::Unknown && stringArgument.type.kind != StaticType::String) {
            throw TypeError(expression.paren,
                "substr expects string as first argument, got " + typeInfoName(stringArgument.type));
        }
        const CheckedExpression startArgument = checkExpressionInfo(*expression.arguments[1]);
        if (startArgument.type.kind != StaticType::Unknown && startArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "substr expects number as second argument, got " + typeInfoName(startArgument.type));
        }
        const CheckedExpression lengthArgument = checkExpressionInfo(*expression.arguments[2]);
        if (lengthArgument.type.kind != StaticType::Unknown && lengthArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "substr expects number as third argument, got " + typeInfoName(lengthArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }
    case NativeFunctionKind::CharAt: {
        const CheckedExpression stringArgument = checkExpressionInfo(*expression.arguments[0]);
        if (stringArgument.type.kind != StaticType::Unknown && stringArgument.type.kind != StaticType::String) {
            throw TypeError(expression.paren,
                "charAt expects string as first argument, got " + typeInfoName(stringArgument.type));
        }
        const CheckedExpression indexArgument = checkExpressionInfo(*expression.arguments[1]);
        if (indexArgument.type.kind != StaticType::Unknown && indexArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "charAt expects number as second argument, got " + typeInfoName(indexArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }
```

Keep the final `throw TypeError(...)` after the switch.

- [ ] **Step 6: Verify type checking and parse/build**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: static type-error fixtures pass; the success fixture still fails in `--run` because C++ native execution has not implemented the new names.

- [ ] **Step 7: Commit registry and type checking**

```bash
git add include/NativeStdlib.hpp src/NativeStdlib.cpp src/TypeChecker.cpp tests/golden/type_errors/str_wrong_arity.* tests/golden/type_errors/substr_wrong_arity.* tests/golden/type_errors/substr_non_string_static.* tests/golden/type_errors/substr_non_number_start_static.* tests/golden/type_errors/substr_non_number_length_static.* tests/golden/type_errors/char_at_non_string_static.* tests/golden/type_errors/char_at_non_number_index_static.* tests/golden/type_errors/char_at_shadowed_call_non_function.*
git commit -m "feat: type check string builtins"
```

---

### Task 3: C++ runtime execution and runtime errors

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Create: runtime-error fixtures listed below under `tests/golden/runtime_errors/`

- [ ] **Step 1: Add runtime-error fixtures**

Create these files:

```bash
cat > tests/golden/runtime_errors/substr_dynamic_non_string.cd <<'CASE'
fun id(x) { return x; }
substr(id(123), 0, 1);
CASE
cat > tests/golden/runtime_errors/substr_dynamic_non_string.run.err <<'CASE'
Runtime error: substr expects string as first argument
CASE
printf '1\n' > tests/golden/runtime_errors/substr_dynamic_non_string.exit

cat > tests/golden/runtime_errors/substr_dynamic_non_number_start.cd <<'CASE'
fun id(x) { return x; }
substr("abc", id("x"), 1);
CASE
cat > tests/golden/runtime_errors/substr_dynamic_non_number_start.run.err <<'CASE'
Runtime error: substr expects number as second argument
CASE
printf '1\n' > tests/golden/runtime_errors/substr_dynamic_non_number_start.exit

cat > tests/golden/runtime_errors/substr_dynamic_non_number_length.cd <<'CASE'
fun id(x) { return x; }
substr("abc", 0, id(false));
CASE
cat > tests/golden/runtime_errors/substr_dynamic_non_number_length.run.err <<'CASE'
Runtime error: substr expects number as third argument
CASE
printf '1\n' > tests/golden/runtime_errors/substr_dynamic_non_number_length.exit

cat > tests/golden/runtime_errors/substr_non_integer_start.cd <<'CASE'
substr("abc", 1.5, 1);
CASE
cat > tests/golden/runtime_errors/substr_non_integer_start.run.err <<'CASE'
Runtime error: substr expects integer start offset
CASE
printf '1\n' > tests/golden/runtime_errors/substr_non_integer_start.exit

cat > tests/golden/runtime_errors/substr_non_integer_length.cd <<'CASE'
substr("abc", 1, 1.5);
CASE
cat > tests/golden/runtime_errors/substr_non_integer_length.run.err <<'CASE'
Runtime error: substr expects integer length
CASE
printf '1\n' > tests/golden/runtime_errors/substr_non_integer_length.exit

cat > tests/golden/runtime_errors/substr_negative_start.cd <<'CASE'
substr("abc", -1, 1);
CASE
cat > tests/golden/runtime_errors/substr_negative_start.run.err <<'CASE'
Runtime error: substr start offset out of bounds
CASE
printf '1\n' > tests/golden/runtime_errors/substr_negative_start.exit

cat > tests/golden/runtime_errors/substr_length_out_of_bounds.cd <<'CASE'
substr("abc", 2, 2);
CASE
cat > tests/golden/runtime_errors/substr_length_out_of_bounds.run.err <<'CASE'
Runtime error: substr length out of bounds
CASE
printf '1\n' > tests/golden/runtime_errors/substr_length_out_of_bounds.exit

cat > tests/golden/runtime_errors/char_at_dynamic_non_string.cd <<'CASE'
fun id(x) { return x; }
charAt(id(123), 0);
CASE
cat > tests/golden/runtime_errors/char_at_dynamic_non_string.run.err <<'CASE'
Runtime error: charAt expects string as first argument
CASE
printf '1\n' > tests/golden/runtime_errors/char_at_dynamic_non_string.exit

cat > tests/golden/runtime_errors/char_at_dynamic_non_number_index.cd <<'CASE'
fun id(x) { return x; }
charAt("abc", id("x"));
CASE
cat > tests/golden/runtime_errors/char_at_dynamic_non_number_index.run.err <<'CASE'
Runtime error: charAt expects number as second argument
CASE
printf '1\n' > tests/golden/runtime_errors/char_at_dynamic_non_number_index.exit

cat > tests/golden/runtime_errors/char_at_non_integer_index.cd <<'CASE'
charAt("abc", 1.5);
CASE
cat > tests/golden/runtime_errors/char_at_non_integer_index.run.err <<'CASE'
Runtime error: charAt expects integer index
CASE
printf '1\n' > tests/golden/runtime_errors/char_at_non_integer_index.exit

cat > tests/golden/runtime_errors/char_at_index_out_of_bounds.cd <<'CASE'
charAt("abc", 3);
CASE
cat > tests/golden/runtime_errors/char_at_index_out_of_bounds.run.err <<'CASE'
Runtime error: charAt index out of bounds
CASE
printf '1\n' > tests/golden/runtime_errors/char_at_index_out_of_bounds.exit
```

- [ ] **Step 2: Add interpreter declarations**

In `include/IRInterpreter.hpp`, add these private methods after `executeNativeSqrt`:

```cpp
    Value executeNativeStr(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeSubstr(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeCharAt(const Frame& frame, const std::vector<IRRegister>& arguments);
```

- [ ] **Step 3: Add finite integer helper**

In `src/IRInterpreter.cpp`, inside the anonymous namespace after `divideNumber`, add:

```cpp
std::size_t checkedIntegerIndex(double value, const std::string& integerMessage, const std::string& boundsMessage, std::size_t upperBoundInclusive)
{
    if (!std::isfinite(value) || std::floor(value) != value) {
        throw IRRuntimeError(integerMessage);
    }
    if (value < 0.0 || value > static_cast<double>(upperBoundInclusive)) {
        throw IRRuntimeError(boundsMessage);
    }
    return static_cast<std::size_t>(value);
}
```

- [ ] **Step 4: Dispatch native names**

In `IRInterpreter::executeNativeCall`, add these branches after `sqrt`:

```cpp
    if (name == "str") {
        return executeNativeStr(frame, arguments);
    }
    if (name == "substr") {
        return executeNativeSubstr(frame, arguments);
    }
    if (name == "charAt") {
        return executeNativeCharAt(frame, arguments);
    }
```

- [ ] **Step 5: Implement native string functions**

In `src/IRInterpreter.cpp`, after `executeNativeSqrt`, add:

```cpp
Value IRInterpreter::executeNativeStr(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("str expects 1 arguments");
    }
    return Value::string(valueToString(readRegister(frame, arguments[0])));
}

Value IRInterpreter::executeNativeSubstr(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 3) {
        throw IRRuntimeError("substr expects 3 arguments");
    }
    const Value& source = readRegister(frame, arguments[0]);
    if (source.type() != Value::Type::String) {
        throw IRRuntimeError("substr expects string as first argument");
    }
    const Value& startValue = readRegister(frame, arguments[1]);
    if (startValue.type() != Value::Type::Number) {
        throw IRRuntimeError("substr expects number as second argument");
    }
    const Value& lengthValue = readRegister(frame, arguments[2]);
    if (lengthValue.type() != Value::Type::Number) {
        throw IRRuntimeError("substr expects number as third argument");
    }

    const std::string& text = source.asString();
    const std::size_t start = checkedIntegerIndex(
        startValue.asNumber(), "substr expects integer start offset", "substr start offset out of bounds", text.size());
    const std::size_t length = checkedIntegerIndex(
        lengthValue.asNumber(), "substr expects integer length", "substr length out of bounds", text.size());
    if (length > text.size() - start) {
        throw IRRuntimeError("substr length out of bounds");
    }
    return Value::string(text.substr(start, length));
}

Value IRInterpreter::executeNativeCharAt(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 2) {
        throw IRRuntimeError("charAt expects 2 arguments");
    }
    const Value& source = readRegister(frame, arguments[0]);
    if (source.type() != Value::Type::String) {
        throw IRRuntimeError("charAt expects string as first argument");
    }
    const Value& indexValue = readRegister(frame, arguments[1]);
    if (indexValue.type() != Value::Type::Number) {
        throw IRRuntimeError("charAt expects number as second argument");
    }

    const std::string& text = source.asString();
    if (text.empty()) {
        throw IRRuntimeError("charAt index out of bounds");
    }
    const std::size_t index = checkedIntegerIndex(
        indexValue.asNumber(), "charAt expects integer index", "charAt index out of bounds", text.size() - 1);
    return Value::string(std::string(1, text[index]));
}
```

- [ ] **Step 6: Verify C++ runtime path**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass except possible missing `ir.out` and `bytecode.out` for `native_stdlib_strings`, which are added in the next step.

- [ ] **Step 7: Refresh success IR and bytecode goldens**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
git diff -- tests/golden/native_stdlib_strings
```

Expected: `tests/golden/native_stdlib_strings/ir.out` and `tests/golden/native_stdlib_strings/bytecode.out` are created and contain `native_call` lines for `str`, `substr`, and `charAt`. Review the diff before committing.

- [ ] **Step 8: Commit C++ runtime and goldens**

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/native_stdlib_strings tests/golden/runtime_errors/substr_dynamic_non_string.* tests/golden/runtime_errors/substr_dynamic_non_number_start.* tests/golden/runtime_errors/substr_dynamic_non_number_length.* tests/golden/runtime_errors/substr_non_integer_start.* tests/golden/runtime_errors/substr_non_integer_length.* tests/golden/runtime_errors/substr_negative_start.* tests/golden/runtime_errors/substr_length_out_of_bounds.* tests/golden/runtime_errors/char_at_dynamic_non_string.* tests/golden/runtime_errors/char_at_dynamic_non_number_index.* tests/golden/runtime_errors/char_at_non_integer_index.* tests/golden/runtime_errors/char_at_index_out_of_bounds.*
git commit -m "feat: execute string builtins"
```

---

### Task 4: Rust VM parity and bytecode artifact

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`
- Create: `tests/bytecode_artifacts/native_stdlib_strings/input.cd`
- Create: `tests/bytecode_artifacts/native_stdlib_strings/run.out`
- Create: `tests/bytecode_artifacts/native_stdlib_strings/expected.cdbc`

- [ ] **Step 1: Add Rust integer helper**

In `vm-rs/src/vm.rs`, inside `impl Vm` near the native-call helpers, add:

```rust
    fn checked_integer_index(
        value: f64,
        integer_message: &'static str,
        bounds_message: &'static str,
        upper_bound_inclusive: usize,
    ) -> Result<usize, RuntimeError> {
        if !value.is_finite() || value.floor() != value {
            return Err(RuntimeError::new(integer_message));
        }
        if value < 0.0 || value > upper_bound_inclusive as f64 {
            return Err(RuntimeError::new(bounds_message));
        }
        Ok(value as usize)
    }
```

- [ ] **Step 2: Dispatch native names in Rust**

In `execute_native_call`, add match arms after `sqrt`:

```rust
            "str" => self.execute_native_str(arguments),
            "substr" => self.execute_native_substr(arguments),
            "charAt" => self.execute_native_char_at(arguments),
```

- [ ] **Step 3: Implement Rust native functions**

In `vm-rs/src/vm.rs`, after `execute_native_sqrt`, add:

```rust
    fn execute_native_str(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("str expects 1 arguments"));
        }
        Ok(Value::string(arguments[0].to_string()))
    }

    fn execute_native_substr(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 3 {
            return Err(RuntimeError::new("substr expects 3 arguments"));
        }
        let Value::String(text) = &arguments[0] else {
            return Err(RuntimeError::new("substr expects string as first argument"));
        };
        let Value::Number(start_value) = arguments[1] else {
            return Err(RuntimeError::new("substr expects number as second argument"));
        };
        let Value::Number(length_value) = arguments[2] else {
            return Err(RuntimeError::new("substr expects number as third argument"));
        };

        let start = Self::checked_integer_index(
            start_value,
            "substr expects integer start offset",
            "substr start offset out of bounds",
            text.len(),
        )?;
        let length = Self::checked_integer_index(
            length_value,
            "substr expects integer length",
            "substr length out of bounds",
            text.len(),
        )?;
        if length > text.len() - start {
            return Err(RuntimeError::new("substr length out of bounds"));
        }
        let bytes = &text.as_bytes()[start..start + length];
        let value = String::from_utf8(bytes.to_vec())
            .map_err(|_| RuntimeError::new("substr produced invalid utf-8"))?;
        Ok(Value::string(value))
    }

    fn execute_native_char_at(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("charAt expects 2 arguments"));
        }
        let Value::String(text) = &arguments[0] else {
            return Err(RuntimeError::new("charAt expects string as first argument"));
        };
        let Value::Number(index_value) = arguments[1] else {
            return Err(RuntimeError::new("charAt expects number as second argument"));
        };
        if text.is_empty() {
            return Err(RuntimeError::new("charAt index out of bounds"));
        }
        let index = Self::checked_integer_index(
            index_value,
            "charAt expects integer index",
            "charAt index out of bounds",
            text.len() - 1,
        )?;
        let bytes = &text.as_bytes()[index..index + 1];
        let value = String::from_utf8(bytes.to_vec())
            .map_err(|_| RuntimeError::new("charAt produced invalid utf-8"))?;
        Ok(Value::string(value))
    }
```

- [ ] **Step 4: Add Rust VM golden allowlist entry**

In `tests/run_rust_vm_tests.py`, add this string near the existing native stdlib entries:

```python
            "native_stdlib_strings",
```

- [ ] **Step 5: Add bytecode artifact input and run output**

Create `tests/bytecode_artifacts/native_stdlib_strings/input.cd`:

```cd
print str([1, "x"]);
print substr("hello", 1, 3);
print charAt("hello", 4);
```

Create `tests/bytecode_artifacts/native_stdlib_strings/run.out`:

```text
[1, x]
ell
o
```

- [ ] **Step 6: Generate expected `.cdbc` artifact**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/native_stdlib_strings/expected.cdbc tests/bytecode_artifacts/native_stdlib_strings/input.cd
sed -n '1,160p' tests/bytecode_artifacts/native_stdlib_strings/expected.cdbc
```

Expected: the artifact contains names `"str"`, `"substr"`, and `"charAt"`, and `native_call` instructions for each.

- [ ] **Step 7: Verify Rust and artifact paths**

Run:

```bash
cmake --build build
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: bytecode artifact tests, Rust VM golden parity, and Rust unit tests pass.

- [ ] **Step 8: Commit Rust VM parity**

```bash
git add vm-rs/src/vm.rs tests/run_rust_vm_tests.py tests/bytecode_artifacts/native_stdlib_strings/input.cd tests/bytecode_artifacts/native_stdlib_strings/run.out tests/bytecode_artifacts/native_stdlib_strings/expected.cdbc
git commit -m "feat: run string builtins in rust vm"
```

---

### Task 5: Documentation and final verification

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README builtin overview**

In `README.md`, update the opening builtin list so it includes the new string helpers:

```markdown
source imports, and builtins such as `len`, `push`, `pop`, `floor`, `ceil`,
`sqrt`, `str`, `substr`, and `charAt`.
```

- [ ] **Step 2: Add README string builtin paragraph**

Near the existing builtin sections, add:

```markdown
The string native stdlib includes `str(value)`, `substr(string, start, length)`, and `charAt(string, index)`. `str` returns the same textual representation used by `print`. `substr` and `charAt` use byte offsets, matching the current `len(string)` byte-length behavior; offsets must be finite integer numbers and in bounds. User bindings with the same names shadow these builtins.
```

- [ ] **Step 3: Update roadmap phase 13 status**

In `docs/roadmap.md`, extend the Phase 13 status paragraph to mention:

```markdown
A string helper slice is implemented with `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` on the same shadowable native-call path.
```

- [ ] **Step 4: Update project memory semantics**

In `AGENTS.md`, update the numeric native stdlib bullet or add a neighboring bullet:

```markdown
- The string native stdlib includes shadowable `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` helpers. `str` uses the same formatting as `print`; `substr` and `charAt` use byte offsets consistent with `len(string)` and validate integer bounds at runtime when needed.
```

- [ ] **Step 5: Run full verification**

Run exactly:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
git status --short
```

Expected:

- CMake configure and build exit 0.
- CTest reports all tests passed.
- Golden tests report 0 failed.
- Golden selftests report OK.
- Bytecode artifact tests report 0 failed.
- Rust VM golden tests report 0 failed.
- Cargo tests report all tests passed.
- `git status --short` shows only intentional documentation changes before the final commit, then clean after committing.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document string builtins"
```

- [ ] **Step 7: Report results**

After the final commit and clean status, report the commits made and the exact verification command results.
