# Export List Syntax Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace declaration-wrapping exports with standalone `export name[, name...];` declarations that export already-defined top-level variables, functions, and structs.

**Architecture:** Parse `ExportStmt` as a list of identifier tokens instead of a wrapped declaration. Type checking resolves each exported name against the current module top-level value and struct tables, records exports, and leaves IR/bytecode generation unchanged except that export statements are skipped. Update tests, goldens, bytecode artifacts, and docs to the new syntax.

**Tech Stack:** C++17 recursive-descent parser/AST/type checker/IR compiler, Python golden tests, Rust VM parity tests, Markdown/EBNF docs.

---

## File Structure

- `include/Ast.hpp`: change `ExportStmt` to store `std::vector<Token> names` instead of `StmtPtr declaration`.
- `src/Ast.cpp`: update compact and tree AST printing plus `ExportStmt` constructor.
- `include/Parser.hpp`: remove `exportTargetDeclaration()` and make `exportDeclaration()` parse identifier lists.
- `src/Parser.cpp`: parse `export identifier {, identifier};`, reject old declaration-wrapping forms and trailing commas.
- `include/TypeChecker.hpp`: keep the existing `checkExport(const ExportStmt&)` signature and add module-local struct-name tracking so imported structs are not accidentally re-exported.
- `src/TypeChecker.cpp`: stop checking a wrapped declaration; resolve each export name against top-level local value bindings and local struct type declarations.
- `src/IRCompiler.cpp`: skip export statements instead of compiling an inner declaration.
- `tests/golden/**`: update export fixtures and add parse/type error cases for new syntax.
- `tests/bytecode_artifacts/**`: update export fixtures and refreshed `.cdbc` artifacts.
- `tests/cli_multi_source_tests.py`: update temporary source strings and diagnostic assertions to new export syntax.
- `tests/run_rust_vm_tests.py`: update only if a case list or expected artifact name changes; no change is expected.
- `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, `AGENTS.md`: document `export name, other;` instead of `export let/fun/struct`.

---

### Task 1: Add RED fixtures for export lists

**Files:**
- Modify: `tests/golden/import_basic/lib.cd`
- Modify: `tests/golden/import_duplicate/lib.cd`
- Modify: `tests/golden/import_nested/inner.cd`
- Modify: `tests/golden/import_nested/lib.cd`
- Modify: `tests/golden/module_import_order/lib.cd`
- Modify: `tests/golden/module_exports_basic/lib.cd`
- Modify: `tests/golden/module_exports_private_helper/lib.cd`
- Modify: `tests/golden/type_errors/module_private_name_deps/lib.cd`
- Modify: `tests/golden/type_errors/module_duplicate_export_deps/a.cd`
- Modify: `tests/golden/type_errors/module_duplicate_export_deps/b.cd`
- Modify: `tests/golden/type_errors/imported_file_type_error_deps/lib.cd`
- Modify: `tests/golden/type_errors/imported_file_type_error.err`
- Create: `tests/golden/type_errors/export_missing_name.cd`
- Create: `tests/golden/type_errors/export_missing_name.err`
- Create: `tests/golden/type_errors/export_missing_name.exit`
- Modify: `tests/golden/parse_errors/imported_file_parse_error_deps/lib.cd`
- Modify: `tests/golden/parse_errors/imported_file_parse_error.err`
- Modify: `tests/golden/parse_errors/export_invalid_statement.cd`
- Modify: `tests/golden/parse_errors/export_invalid_statement.err`
- Modify: `tests/golden/parse_errors/export_nested.cd`
- Modify: `tests/golden/parse_errors/export_nested.err`
- Create: `tests/golden/parse_errors/export_invalid_fun.cd`
- Create: `tests/golden/parse_errors/export_invalid_fun.err`
- Create: `tests/golden/parse_errors/export_invalid_fun.exit`
- Create: `tests/golden/parse_errors/export_invalid_struct.cd`
- Create: `tests/golden/parse_errors/export_invalid_struct.err`
- Create: `tests/golden/parse_errors/export_invalid_struct.exit`
- Create: `tests/golden/parse_errors/export_trailing_comma.cd`
- Create: `tests/golden/parse_errors/export_trailing_comma.err`
- Create: `tests/golden/parse_errors/export_trailing_comma.exit`
- Modify: `tests/bytecode_artifacts/import_basic/lib.cd`
- Modify: `tests/bytecode_artifacts/module_exports/lib.cd`
- Modify: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Rewrite success and existing error fixtures to desired syntax**

Run this script from the repo root:

```bash
cat > tests/golden/import_basic/lib.cd <<'CD'
fun add(a, b) { return a + b; }
export add;
CD

cat > tests/golden/import_duplicate/lib.cd <<'CD'
let value = 1;
export value;
CD

cat > tests/golden/import_nested/inner.cd <<'CD'
let value = 41;
export value;
CD

cat > tests/golden/import_nested/lib.cd <<'CD'
import "./inner.cd";
fun next() { return value + 1; }
export next;
CD

cat > tests/golden/module_import_order/lib.cd <<'CD'
let value = 1;
export value;
CD

cat > tests/golden/module_exports_basic/lib.cd <<'CD'
let value = 7;
fun add(a, b) { return a + b; }
struct Point { x: number, y: number }
export value, add, Point;
CD

cat > tests/golden/module_exports_private_helper/lib.cd <<'CD'
let secret = 40;
fun inc(x) { return x + 1; }
fun answer() { return inc(secret) + 1; }
export answer;
CD

cat > tests/golden/type_errors/module_private_name_deps/lib.cd <<'CD'
let secret = 1;
let visible = 2;
export visible;
CD

cat > tests/golden/type_errors/module_duplicate_export_deps/a.cd <<'CD'
let value = 1;
export value;
CD

cat > tests/golden/type_errors/module_duplicate_export_deps/b.cd <<'CD'
let value = 2;
export value;
CD

cat > tests/golden/type_errors/imported_file_type_error_deps/lib.cd <<'CD'
let value = missing;
export value;
CD

cat > tests/golden/type_errors/imported_file_type_error.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/imported_file_type_error_deps/lib.cd:1:13: undefined variable `missing`
  let value = missing;
              ^
ERR

cat > tests/golden/parse_errors/imported_file_parse_error_deps/lib.cd <<'CD'
let value = ;
export value;
CD

cat > tests/golden/parse_errors/imported_file_parse_error.err <<'ERR'
Parse error at /home/junhe/compiler/tests/golden/parse_errors/imported_file_parse_error_deps/lib.cd:1:13: expected expression
  let value = ;
              ^
ERR

cat > tests/golden/parse_errors/export_invalid_statement.cd <<'CD'
export let value = 1;
CD

cat > tests/golden/parse_errors/export_invalid_statement.err <<'ERR'
Parse error at 1:8: expected identifier after `export`
ERR

cat > tests/golden/parse_errors/export_nested.cd <<'CD'
{
  export value;
}
CD

cat > tests/golden/parse_errors/export_nested.err <<'ERR'
Parse error at 2:3: `export` is only allowed at top level
ERR

cat > tests/bytecode_artifacts/import_basic/lib.cd <<'CD'
fun add(a, b) { return a + b; }
export add;
CD

cat > tests/bytecode_artifacts/module_exports/lib.cd <<'CD'
let base = 39;
fun add3(x) { return x + 3; }
fun answer() { return add3(base); }
export answer;
CD
```

- [ ] **Step 2: Add new parse/type error fixtures**

Run this script from the repo root:

```bash
cat > tests/golden/parse_errors/export_invalid_fun.cd <<'CD'
export fun f() { return 1; }
CD

cat > tests/golden/parse_errors/export_invalid_fun.err <<'ERR'
Parse error at 1:8: expected identifier after `export`
ERR

cat > tests/golden/parse_errors/export_invalid_fun.exit <<'ERR'
1
ERR

cat > tests/golden/parse_errors/export_invalid_struct.cd <<'CD'
export struct Point { x: number }
CD

cat > tests/golden/parse_errors/export_invalid_struct.err <<'ERR'
Parse error at 1:8: expected identifier after `export`
ERR

cat > tests/golden/parse_errors/export_invalid_struct.exit <<'ERR'
1
ERR

cat > tests/golden/parse_errors/export_trailing_comma.cd <<'CD'
export value,;
CD

cat > tests/golden/parse_errors/export_trailing_comma.err <<'ERR'
Parse error at 1:14: expected identifier after `,` in export list
ERR

cat > tests/golden/parse_errors/export_trailing_comma.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/export_missing_name.cd <<'CD'
export missing;
CD

cat > tests/golden/type_errors/export_missing_name.err <<'ERR'
Type error at 1:8: cannot export undefined name `missing`
ERR

cat > tests/golden/type_errors/export_missing_name.exit <<'ERR'
1
ERR
```

- [ ] **Step 3: Update CLI multi-source unit tests to expected syntax and diagnostics**

In `tests/cli_multi_source_tests.py`, replace the export-related temporary source strings and diagnostics with these exact snippets:

```python
(root / "lib.cd").write_text('let value = "direct";\nexport value;\n', encoding="utf-8")
```

```python
(nested / "lib.cd").write_text(
    'import "./inner.cd";\n'
    'fun getValue() { return value; }\n'
    'export getValue;\n',
    encoding="utf-8",
)
(nested / "inner.cd").write_text('let value = "relative";\nexport value;\n', encoding="utf-8")
```

```python
lib.write_text('let value = ;\nexport value;\n', encoding="utf-8")
```

```python
self.assertEqual(
    completed.stderr,
    f"Parse error at {lib}:1:13: expected expression\n"
    "  let value = ;\n"
    "              ^\n",
)
```

```python
lib.write_text('let value = missing;\nexport value;\n', encoding="utf-8")
```

```python
self.assertEqual(
    completed.stderr,
    f"Type error at {lib}:1:13: undefined variable `missing`\n"
    "  let value = missing;\n"
    "              ^\n",
)
```

- [ ] **Step 4: Run focused RED test commands**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --cases module_exports_basic module_exports_private_helper import_basic import_duplicate import_nested module_import_order
python3 tests/run_golden_tests.py ./build/compiler_design --cases export_invalid_statement export_invalid_fun export_invalid_struct export_nested export_trailing_comma imported_file_parse_error
python3 tests/run_golden_tests.py ./build/compiler_design --cases export_missing_name imported_file_type_error module_private_name module_duplicate_export_main
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --cases import_basic module_exports
```

Expected before implementation: failures showing the parser does not yet accept `export value;`, old `export let` parse-error expectations do not match, and artifacts are stale or not emitted.

- [ ] **Step 5: Commit RED fixtures**

```bash
git add tests/golden tests/bytecode_artifacts tests/cli_multi_source_tests.py
git commit -m "test: add export list fixtures"
```

---

### Task 2: Parse and print export lists

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Change `ExportStmt` in `include/Ast.hpp`**

Replace the current `ExportStmt` declaration with:

```cpp
struct ExportStmt final : Stmt {
    ExportStmt(Token keyword, std::vector<Token> names);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    std::vector<Token> names;
};
```

- [ ] **Step 2: Update parser declarations in `include/Parser.hpp`**

Replace:

```cpp
StmtPtr exportDeclaration();
StmtPtr exportTargetDeclaration(const Token& exportKeyword);
```

with:

```cpp
StmtPtr exportDeclaration();
```

- [ ] **Step 3: Update `Parser::exportDeclaration()` in `src/Parser.cpp`**

Replace both `exportDeclaration()` and `exportTargetDeclaration()` with:

```cpp
StmtPtr Parser::exportDeclaration()
{
    Token keyword = previous();
    std::vector<Token> names;

    names.push_back(consume(TokenType::Identifier, "expected identifier after `export`"));
    while (match(TokenType::Comma)) {
        names.push_back(consume(TokenType::Identifier, "expected identifier after `,` in export list"));
    }
    consume(TokenType::Semicolon, "expected `;` after export list");
    return std::make_unique<ExportStmt>(std::move(keyword), std::move(names));
}
```

- [ ] **Step 4: Update compact AST printing in `src/Ast.cpp`**

Replace the `ExportStmt` branch in `writeInlineStmt()` with:

```cpp
if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&stmt)) {
    out << "(export";
    for (const auto& name : exportStmt->names) {
        out << ' ' << name.lexeme;
    }
    out << ')';
    return;
}
```

- [ ] **Step 5: Update `ExportStmt` constructor and tree printing in `src/Ast.cpp`**

Replace the current constructor and `print()` implementation with:

```cpp
ExportStmt::ExportStmt(Token keyword, std::vector<Token> names)
    : keyword(std::move(keyword))
    , names(std::move(names))
{
}

void ExportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Export";
    for (const auto& name : names) {
        out << ' ' << name.lexeme;
    }
    out << "\n";
}
```

- [ ] **Step 6: Build and run parser-focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --cases export_invalid_statement export_invalid_fun export_invalid_struct export_nested export_trailing_comma imported_file_parse_error
```

Expected after this task: parse-error fixtures pass; success fixtures still fail later during type checking or IR because `TypeChecker` and `IRCompiler` still refer to `ExportStmt::declaration`.

- [ ] **Step 7: Commit parser/AST change**

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/parse_errors
git commit -m "feat: parse export lists"
```

---

### Task 3: Type-check standalone export names and skip them in IR

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Modify: `src/IRCompiler.cpp`
- Test: export golden fixtures from Task 1

- [ ] **Step 1: Add module-local struct tracking in `include/TypeChecker.hpp`**

Add this private field beside `moduleStructExports_`:

```cpp
std::unordered_map<std::size_t, std::unordered_set<std::string>> moduleLocalStructNames_;
```

- [ ] **Step 2: Clear local struct tracking in `TypeChecker::check()`**

After `moduleStructExports_.clear();`, add:

```cpp
moduleLocalStructNames_.clear();
```

- [ ] **Step 3: Record local struct declarations in `TypeChecker::checkStructDeclaration()`**

After `structTypes_.emplace(statement.name.lexeme, std::move(declaration));`, add:

```cpp
if (!moduleStack_.empty()) {
    moduleLocalStructNames_[moduleStack_.back()].insert(statement.name.lexeme);
}
```

- [ ] **Step 4: Update `TypeChecker::checkStatement()` export handling**

Replace the `ExportStmt` branch with:

```cpp
if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&statement)) {
    checkExport(*exportStmt);
    return;
}
```

- [ ] **Step 5: Replace `TypeChecker::checkExport()`**

Replace the full function with:

```cpp
void TypeChecker::checkExport(const ExportStmt& statement)
{
    if (moduleStack_.empty()) {
        throw TypeError(statement.keyword, "`export` is only allowed at top level");
    }

    const std::size_t moduleId = moduleStack_.back();
    for (const auto& name : statement.names) {
        bool exported = false;

        if (Binding* binding = findVariable(name.lexeme)) {
            if (binding->scopeDepth == 0 && !binding->imported) {
                moduleExports_[moduleId].emplace(name.lexeme, *binding);
                exported = true;
            }
        }

        const auto localStructs = moduleLocalStructNames_.find(moduleId);
        if (localStructs != moduleLocalStructNames_.end()
            && localStructs->second.find(name.lexeme) != localStructs->second.end()) {
            if (const StructTypeDecl* structType = findStructType(name.lexeme)) {
                moduleStructExports_[moduleId].emplace(name.lexeme, *structType);
                exported = true;
            }
        }

        if (!exported) {
            throw TypeError(name, "cannot export undefined name `" + name.lexeme + "`");
        }
    }
}
```

- [ ] **Step 6: Update `IRCompiler::compileStatement()` export handling**

Replace the `ExportStmt` branch with:

```cpp
if (dynamic_cast<const ExportStmt*>(&statement)) {
    return;
}
```

- [ ] **Step 7: Build and run export-focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --cases module_exports_basic module_exports_private_helper import_basic import_duplicate import_nested module_import_order
python3 tests/run_golden_tests.py ./build/compiler_design --cases export_missing_name imported_file_type_error module_private_name module_duplicate_export_main
```

Expected: all listed golden cases pass except cases whose expected `ast.out`, `ir.out`, or `bytecode.out` files need refresh because export syntax changes printed output.

- [ ] **Step 8: Commit type-check and IR behavior**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp src/IRCompiler.cpp tests/golden/type_errors
git commit -m "feat: type check export lists"
```

---

### Task 4: Refresh golden outputs and bytecode artifacts

**Files:**
- Modify: `tests/golden/**/ast.out` where export output changed
- Modify: `tests/golden/**/ir.out` only if changed by fixture source rewrite
- Modify: `tests/golden/**/bytecode.out` only if changed by fixture source rewrite
- Modify: `tests/bytecode_artifacts/import_basic/expected.cdbc`
- Modify: `tests/bytecode_artifacts/module_exports/expected.cdbc`

- [ ] **Step 1: Refresh relevant golden outputs**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --cases module_exports_basic module_exports_private_helper import_basic import_duplicate import_nested module_import_order --update
```

Expected: golden files for changed AST/IR/bytecode output are updated. Review the diff before committing.

- [ ] **Step 2: Refresh bytecode text artifacts**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --cases import_basic module_exports --update
```

Expected: `expected.cdbc` files are updated only where source line numbering or emitted constants changed. Review the diff before committing.

- [ ] **Step 3: Verify refreshed focused suites**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --cases module_exports_basic module_exports_private_helper import_basic import_duplicate import_nested module_import_order export_invalid_statement export_invalid_fun export_invalid_struct export_nested export_trailing_comma export_missing_name imported_file_parse_error imported_file_type_error module_private_name module_duplicate_export_main
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --cases import_basic module_exports
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --cases import_basic module_exports --goldens
```

Expected: all listed tests pass.

- [ ] **Step 4: Commit refreshed outputs**

```bash
git add tests/golden tests/bytecode_artifacts
git commit -m "test: refresh export list goldens"
```

---

### Task 5: Update documentation and project memory

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar**

In `docs/language-grammar.ebnf`, replace:

```ebnf
exportDecl  = "export", ( structDecl | funDecl | letDecl ) ;
```

with:

```ebnf
exportDecl  = "export", identifier,
              { ",", identifier }, ";" ;
```

- [ ] **Step 2: Update README syntax summary**

In `README.md`, replace the export syntax lines:

```text
export let name = expression;
export fun name(parameter[: type]*) [: type] { declaration* }
export struct Name { field: type, ... }
```

with:

```text
export name[, name...];
```

- [ ] **Step 3: Update README source import example**

Replace the source import example body with:

```cd
// lib.cd
let hidden = 1;
fun visible() { return hidden + 1; }
export visible;

// main.cd
import "./lib.cd";
print visible();
```

Replace the paragraph that begins `This phase supports` with:

```markdown
This phase supports standalone export lists such as `export value;` and
`export value, helper, Point;` for already-defined top-level variables,
functions, and structs. It does not add namespaces, `import ... as name`,
re-export syntax, package search paths, separate compilation, or imports from
stdin. `import` inside strings or `//` comments is ignored by the loader.
```

- [ ] **Step 4: Update roadmap wording**

In `docs/roadmap.md`, replace Phase 14C wording that says:

```text
`export let`, `export fun`, and `export struct` expose selected declarations to importers
```

with:

```text
standalone export lists expose selected already-defined top-level declarations to importers
```

Replace the suggested feature bullet:

```text
- `export let`, `export fun`, and `export struct` for explicit cross-file visibility. Implemented.
```

with:

```text
- Standalone `export name[, name...];` lists for explicit cross-file visibility. Implemented.
```

- [ ] **Step 5: Update AGENTS project memory**

In `AGENTS.md`, replace:

```text
`export let`, `export fun`, and `export struct` expose selected declarations to importers
```

with:

```text
standalone `export name[, name...];` lists expose selected already-defined top-level declarations to importers
```

- [ ] **Step 6: Verify no current docs/tests still advertise old syntax**

Run:

```bash
grep -R "export \\(let\\|fun\\|struct\\)" -n README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md tests/golden tests/bytecode_artifacts tests/cli_multi_source_tests.py
```

Expected: no matches in current docs, tests, or fixtures. Matches inside historical `docs/superpowers/specs/2026-07-07-module-exports-design.md` and `docs/superpowers/plans/2026-07-07-module-exports.md` are acceptable if the grep command is intentionally extended to include all `docs/superpowers` history.

- [ ] **Step 7: Commit docs**

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document export list syntax"
```

---

### Task 6: Full verification and cleanup

**Files:**
- No source changes expected except generated cache cleanup.

- [ ] **Step 1: Run full project verification**

Run the full command set from `AGENTS.md`:

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
```

Expected: every command exits 0.

- [ ] **Step 2: Check workspace diff**

Run:

```bash
git status --short
git diff --stat
```

Expected: no untracked generated files such as `tests/__pycache__/`; only intentional source, tests, artifact, and docs changes remain if a final commit is still pending.

- [ ] **Step 3: Commit any final cleanup**

If Step 2 shows intentional remaining source, fixture, artifact, or documentation changes that were missed by earlier commits, inspect them first:

```bash
git diff
```

Then stage all remaining intentional tracked and untracked changes:

```bash
git add -A
git diff --cached --stat
git commit -m "chore: finalize export list syntax"
```

Expected: final commit created, or no commit needed because all changes were already committed and `git status --short` is empty.

- [ ] **Step 4: Report verification evidence**

In the final response, include the exact verification commands run and their exit status. Do not claim completion unless the commands in Step 1 have passed in the current session.
