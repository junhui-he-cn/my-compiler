#!/usr/bin/env python3

"""Load and validate the versioned M0D verification matrix.

The matrix describes two kinds of coverage:

* environment wrappers, such as warning and sanitizer builds; and
* the canonical runner's projections, such as golden, artifact, Rust VM, and
  cargo-test assertions.

Execution lives in ``run_verification_matrix.py``.  Keeping manifest validation
here lets CTest and the runner self-test reject stale or incomplete matrix
metadata without building the compiler.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
MATRIX_PATH = Path(__file__).resolve().with_name("verification_matrix.json")
INVENTORY_PATH = Path(__file__).resolve().with_name("verification_inventory.json")

REQUIRED_COVERAGE = {
    "warning-build",
    "sanitizer-build",
    "cpp-tests",
    "python-goldens",
    "artifact-tests",
    "rust-vm-tests",
    "cargo-test",
}
REQUIRED_MEASUREMENTS = {
    "compile_time",
    "test_duration",
    "artifact_size",
    "runtime_workload",
}
ALLOWED_EXECUTION = {"matrix", "canonical"}
ALLOWED_REACHABILITY = {"canonical", "environment-wrapper", "matrix-wrapper"}
PLACEHOLDER_PATTERN = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def load_matrix(path: Path = MATRIX_PATH) -> dict[str, Any]:
    return read_json(path)


def load_inventory_revision(path: Path = INVENTORY_PATH) -> str | None:
    try:
        value = read_json(path)
    except (OSError, ValueError, json.JSONDecodeError):
        return None
    revision = value.get("inventory_revision")
    return str(revision) if revision else None


def _is_command(value: Any) -> bool:
    return (
        isinstance(value, list)
        and bool(value)
        and all(isinstance(part, str) and part for part in value)
    )


def _check_paths(repo_root: Path, matrix: dict[str, Any], errors: list[str]) -> None:
    workloads = matrix.get("workloads")
    if not isinstance(workloads, list) or not workloads:
        errors.append("workloads must be a non-empty list")
        return

    workload_ids: list[str] = []
    for workload in workloads:
        if not isinstance(workload, dict):
            errors.append("every workload must be an object")
            continue
        workload_id = str(workload.get("workload_id", "<unknown>"))
        workload_ids.append(workload_id)
        for field in ("workload_id", "fixture", "sources", "expected_output"):
            if field not in workload:
                errors.append(f"{workload_id} missing field: {field}")
        sources = workload.get("sources", [])
        if not isinstance(sources, list) or not sources:
            errors.append(f"{workload_id} sources must be a non-empty list")
        else:
            for source in sources:
                if not isinstance(source, str) or not (repo_root / source).is_file():
                    errors.append(f"{workload_id} missing source: {source}")
        for field in ("fixture", "expected_output"):
            path_value = workload.get(field)
            if not isinstance(path_value, str) or not (repo_root / path_value).exists():
                errors.append(f"{workload_id} missing path: {path_value}")

    if len(workload_ids) != len(set(workload_ids)):
        errors.append("workload IDs must be unique")
    if workload_ids != sorted(workload_ids):
        errors.append("workloads must be sorted by workload_id")


def _check_measurements(matrix: dict[str, Any], errors: list[str]) -> None:
    measurements = matrix.get("measurements")
    if not isinstance(measurements, dict):
        errors.append("measurements must be an object")
        return

    missing = sorted(REQUIRED_MEASUREMENTS - set(measurements))
    errors.extend(f"missing measurement policy: {name}" for name in missing)
    for name in sorted(REQUIRED_MEASUREMENTS & set(measurements)):
        policy = measurements[name]
        if not isinstance(policy, dict):
            errors.append(f"measurement policy must be an object: {name}")
            continue
        for field in ("unit", "statistic", "repetitions", "enforcement"):
            if field not in policy:
                errors.append(f"{name} missing policy field: {field}")
        repetitions = policy.get("repetitions")
        if not isinstance(repetitions, int) or repetitions <= 0:
            errors.append(f"{name}.repetitions must be a positive integer")
        if name == "runtime_workload" and policy.get("statistic") != "median":
            errors.append("runtime_workload.statistic must be median")
        if name == "artifact_size":
            if policy.get("statistic") != "exact":
                errors.append("artifact_size.statistic must be exact")
            if policy.get("tolerance_bytes") != 0:
                errors.append("artifact_size.tolerance_bytes must be zero")
        else:
            tolerance = policy.get("tolerance_percent")
            if not isinstance(tolerance, (int, float)) or tolerance < 0:
                errors.append(f"{name}.tolerance_percent must be non-negative")


def validate_matrix(matrix: dict[str, Any], repo_root: Path) -> list[str]:
    errors: list[str] = []
    if matrix.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"schema_version must be {SCHEMA_VERSION}")
    for field in ("matrix_revision", "inventory_revision", "reference_commit"):
        if not matrix.get(field):
            errors.append(f"{field} is required")

    canonical_command = matrix.get("canonical_command")
    if not _is_command(canonical_command):
        errors.append("canonical_command must be a non-empty command")
    elif "tests/run_verification.py" not in canonical_command:
        errors.append("canonical_command must invoke tests/run_verification.py")

    cells = matrix.get("cells")
    if not isinstance(cells, list) or not cells:
        errors.append("cells must be a non-empty list")
        cells = []

    cell_ids: list[str] = []
    coverage: set[str] = set()
    for cell in cells:
        if not isinstance(cell, dict):
            errors.append("every matrix cell must be an object")
            continue
        cell_id = str(cell.get("cell_id", "<unknown>"))
        cell_ids.append(cell_id)
        for field in (
            "cell_id",
            "family",
            "environment",
            "execution",
            "commands",
            "coverage",
            "reachability",
        ):
            if field not in cell:
                errors.append(f"{cell_id} missing field: {field}")

        execution = cell.get("execution")
        if execution not in ALLOWED_EXECUTION:
            errors.append(f"{cell_id} has invalid execution: {execution}")

        commands = cell.get("commands")
        if not isinstance(commands, list) or not commands or not all(_is_command(command) for command in commands):
            errors.append(f"{cell_id}.commands must be a non-empty list of commands")

        cell_coverage = cell.get("coverage")
        if not isinstance(cell_coverage, list) or not cell_coverage or not all(
            isinstance(tag, str) and tag for tag in cell_coverage
        ):
            errors.append(f"{cell_id}.coverage must be a non-empty list of strings")
        else:
            coverage.update(cell_coverage)

        reachability = cell.get("reachability")
        if not isinstance(reachability, dict):
            errors.append(f"{cell_id}.reachability must be an object")
        else:
            kind = reachability.get("kind")
            if kind not in ALLOWED_REACHABILITY:
                errors.append(f"{cell_id} has invalid reachability kind: {kind}")
            if not reachability.get("name"):
                errors.append(f"{cell_id}.reachability.name is required")

        for command in commands if isinstance(commands, list) else []:
            for part in command if isinstance(command, list) else []:
                for placeholder in PLACEHOLDER_PATTERN.findall(part):
                    if placeholder not in {
                        "default_build_dir",
                        "warnings_build_dir",
                        "sanitizer_build_dir",
                        "compiler",
                        "vm",
                        "report_dir",
                    }:
                        errors.append(f"{cell_id} uses unknown placeholder: {{{placeholder}}}")

    if len(cell_ids) != len(set(cell_ids)):
        errors.append("matrix cell IDs must be unique")
    if cell_ids != sorted(cell_ids):
        errors.append("matrix cells must be sorted by cell_id")
    errors.extend(
        f"missing required coverage: {tag}"
        for tag in sorted(REQUIRED_COVERAGE - coverage)
    )
    if not any(cell.get("cell_id") == "canonical.verification" for cell in cells if isinstance(cell, dict)):
        errors.append("canonical.verification cell is required")

    known_ids = set(cell_ids)
    for cell in cells:
        if not isinstance(cell, dict):
            continue
        duplicate_of = cell.get("duplicate_of")
        if duplicate_of is not None and duplicate_of not in known_ids:
            errors.append(f"{cell.get('cell_id')} duplicate_of is unknown: {duplicate_of}")

    _check_measurements(matrix, errors)
    _check_paths(repo_root, matrix, errors)

    inventory_revision = load_inventory_revision(repo_root / "tests" / "verification_inventory.json")
    if inventory_revision and matrix.get("inventory_revision") != inventory_revision:
        errors.append(
            "matrix inventory_revision does not match tests/verification_inventory.json: "
            f"{matrix.get('inventory_revision')} != {inventory_revision}"
        )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the verification matrix manifest.")
    parser.add_argument("--matrix", type=Path, default=MATRIX_PATH)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    try:
        matrix = load_matrix(args.matrix.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verification matrix: {error}", file=sys.stderr)
        return 1
    errors = validate_matrix(matrix, args.repo_root.resolve())
    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        return 1
    print(
        f"verification matrix: {len(matrix['cells'])} cells, "
        f"{len(matrix['workloads'])} runtime workloads validated"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
