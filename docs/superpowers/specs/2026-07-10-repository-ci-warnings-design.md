# Repository CI and Optional Warnings Design

## Goal

Complete the remaining M0 front-end stabilization item by adding repository CI for the existing verification suite and a CMake option for compiler warnings. This slice must not change language behavior, CLI behavior, bytecode output, or golden files.

## Scope

In scope:

- Add a GitHub Actions workflow for pushes and pull requests targeting `master`.
- Run the same verification steps developers are expected to run locally:
  - configure CMake
  - build all CMake targets
  - run CTest
  - run Python golden tests
  - run golden runner selftests
  - run bytecode artifact tests
  - run Rust VM golden parity tests
  - run Rust `cargo test`
- Add an optional CMake switch named `COMPILER_DESIGN_ENABLE_WARNINGS`.
- When the switch is enabled on GCC or Clang, apply warning flags to C++ targets.
- Keep warning support optional so this slice can land without forcing a warning cleanup campaign.

Out of scope:

- ASan/UBSan CI jobs. Sanitizer builds are valuable, but should be a separate M0 follow-up because they may expose independent runtime issues or require CI runtime tuning.
- Windows/macOS CI. The first CI path targets Ubuntu to match the current easiest Rust/CMake/Python setup.
- Changing test fixtures, compiler semantics, diagnostics, or bytecode text format.
- Making warnings fatal by default.

## Proposed Approach

Use one GitHub Actions workflow file, `.github/workflows/ci.yml`, with one Ubuntu job. The job checks out the repository, installs or enables the required toolchains, configures CMake with the optional warnings switch enabled, builds, and runs the full verification suite.

The CMake warning option will be centralized in `CMakeLists.txt` near target definitions. A small helper function will apply warning flags to each C++ target owned by this repository:

- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC placeholder support may use `/W4` if the project is later built on Windows, but Windows CI is not part of this slice.

The warning option should apply to:

- `compiler_design`
- `frontend_session_tests`
- `flow_facts_tests`
- `module_symbols_tests`

## CI Command Shape

The workflow should run commands equivalent to:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
```

The local AGENTS verification command remains valid without requiring warnings. CI can enable warnings to exercise the new option.

## Error Handling and Failure Behavior

GitHub Actions should fail fast when any command exits non-zero. CTest already provides `--output-on-failure`; Python and Cargo test commands print their own failure details. No custom log parsing is required.

If warning-enabled CMake builds fail because the current code emits warnings, the implementation should either fix narrowly scoped warning issues or make the warning set slightly less aggressive while preserving the optional switch. It should not hide real compile errors or remove tests.

## Testing Strategy

Before implementation is considered complete:

1. Configure locally with warnings enabled:

   ```sh
   cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
   ```

2. Build all targets:

   ```sh
   cmake --build build
   ```

3. Run the full repository verification suite from AGENTS.md.

4. Confirm `git status --short` is clean except for intentional source and workflow changes before committing, and clean after the final commit.

## Documentation Impact

Update `README.md` only if it already documents verification workflows in a place where CI or warning-enabled local builds naturally belongs. Do not document planned sanitizer work as implemented.

## Self-Review

- Placeholder scan: no placeholders, TODOs, or deferred implementation details remain in this spec.
- Internal consistency: the scope, CI commands, and CMake warning option all target the same M0 stabilization slice.
- Scope check: this is a single focused infrastructure slice; sanitizer CI is explicitly deferred.
- Ambiguity check: warning flags, target list, workflow trigger, and verification commands are explicit.
