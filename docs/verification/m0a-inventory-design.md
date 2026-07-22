# M0A Verification Inventory and Canonical Runner

Status: implemented against baseline commit `0481624`.

## Deliverable

M0A establishes a versioned, machine-readable verification inventory at
`tests/verification_inventory.json` and one canonical command:

```sh
python3 tests/run_verification.py ./build/compiler_design vm-rs --report build/verification-report.json
```

The inventory revision is `m0a-2026-07-22-r1`. It contains 1,563 stable case
IDs and records the fixture or named suite, expected stage, capability tags,
backend, expected-result kind, and the existing runner result name. Fixture
and CTest discovery is deterministic; adding either one without refreshing the
inventory is a validation failure.

The canonical runner reuses the existing assertions from:

- `tests/run_golden_tests.py`;
- `tests/bytecode_artifact_tests.py`;
- `tests/run_rust_vm_tests.py`;
- the existing CTest tests; and
- `cargo test --manifest-path vm-rs/Cargo.toml`.

It adds orchestration, stable case mapping, missing/untracked-result detection,
and a JSON report. It does not reimplement fixture comparisons or change the
compiler, bytecode, or VM behavior.

The inventory breakdown is:

| Runner family | Cases |
| --- | ---: |
| CTest named checks | 16 |
| C++ golden assertions | 719 |
| C++ artifact assertions | 116 |
| Rust artifact execution assertions | 104 |
| Rust golden execution assertions | 398 |
| Rust runtime-error assertions | 208 |
| Golden runner selftest suite | 1 |
| Rust unit-test suite | 1 |
| **Total** | **1,563** |

The two named suites intentionally remain suite-level cases. Their underlying
test counts are retained in the baseline comparison report.

## Migration and comparison

The existing direct CI commands remain in `.github/workflows/ci.yml` during the
migration. The canonical command runs after them and uploads
`build/verification-report.json` as a CI artifact. This keeps the old
orchestration as the comparison reference while exercising the new inventory
path on every normal verification job.

The checked-in baseline report is
`docs/verification/m0a-baseline.json`. It binds the comparison to the inventory
revision and baseline commit, records the exact legacy commands, and records
the canonical/legacy projection used for named suites. The baseline was run
with no compiler or language behavior changes; canonical and legacy results had
zero unexplained discovery, classification, exit-code, or failure-text
differences.

## Quantitative gate

The M0A gate is proved by:

1. `python3 tests/verification_inventory.py`, which requires the checked-in
   inventory to match all discovered fixture and CTest case metadata exactly;
2. `python3 tests/run_verification.py ... --report ...`, which must report
   `1,563 passed, 0 failed` and zero untracked results;
3. the legacy command set recorded in `m0a-baseline.json`, whose commands must
   retain zero failing checks and zero exit-code differences; and
4. CTest's `verification_inventory` and `verification_runner_selftest` tests,
   which protect inventory coverage and canonical result mapping.

The inventory is the machine-readable corpus revision. The baseline commit,
commands, observed counts, and comparison dispositions are recorded in the
JSON report rather than inferred from a later checkout.

## Old-path deletion condition

No existing runner or CI step is deleted in M0A. The legacy direct commands can
be removed only after the canonical command has passed the same baseline in
two consecutive CI runs and its report provides equivalent case discovery,
pass/fail/skip classification, exit status, and failure diagnostics. Until
then, the specialized runners remain the assertion owners and the canonical
runner remains an orchestration layer.

## Explicit non-goals

M0A does not add boundary comparison dumps, fuzzing, semantic decision policy,
artifact-version negotiation, performance measurements, HIR, or compiler
behavior changes. Those remain the independently scheduled M0B, M0C, M0.5,
M1, and M0D/M4 work described in `docs/roadmap.md`.
