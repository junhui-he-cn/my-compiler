#!/usr/bin/env python3

"""Execute the M0D verification matrix and record its measurement baseline.

The runner deliberately keeps assertion ownership in the existing specialized
runners.  Matrix cells either execute an environment wrapper (clean builds and
sanitizer CTest) or project to a named result family in the canonical runner.
The latter cells are marked as covered rather than executed a second time.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shlex
import statistics
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TESTS_DIR.parent
DEFAULT_MATRIX = TESTS_DIR / "verification_matrix.json"
DEFAULT_REPORT = REPO_ROOT / "build" / "verification-matrix-report.json"

sys.path.insert(0, str(TESTS_DIR))
import verification_matrix  # noqa: E402


@dataclass(frozen=True)
class CommandObservation:
    command: list[str]
    duration_seconds: float
    returncode: int
    stdout: str
    stderr: str


def command_display(command: Iterable[str]) -> str:
    return shlex.join(str(part) for part in command)


def run_command(command: list[str], cwd: Path) -> CommandObservation:
    started = time.perf_counter()
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            text=True,
            capture_output=True,
            check=False,
            env=environment,
        )
        return CommandObservation(
            command=command,
            duration_seconds=time.perf_counter() - started,
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
        )
    except OSError as error:
        return CommandObservation(
            command=command,
            duration_seconds=time.perf_counter() - started,
            returncode=127,
            stdout="",
            stderr=f"{type(error).__name__}: {error}",
        )


def rounded(value: float) -> float:
    return round(value, 6)


def substitute(command: list[str], substitutions: dict[str, str]) -> list[str]:
    return [
        part.replace("{default_build_dir}", substitutions["default_build_dir"])
        .replace("{warnings_build_dir}", substitutions["warnings_build_dir"])
        .replace("{sanitizer_build_dir}", substitutions["sanitizer_build_dir"])
        .replace("{compiler}", substitutions["compiler"])
        .replace("{vm}", substitutions["vm"])
        .replace("{report_dir}", substitutions["report_dir"])
        for part in command
    ]


def process_failure(observation: CommandObservation, label: str) -> str:
    parts = [
        f"{label} exited with {observation.returncode}: {command_display(observation.command)}",
    ]
    if observation.stdout:
        parts.extend(["STDOUT:", observation.stdout.rstrip()])
    if observation.stderr:
        parts.extend(["STDERR:", observation.stderr.rstrip()])
    return "\n".join(parts)


def execute_commands(
    commands: list[list[str]],
    substitutions: dict[str, str],
    cwd: Path,
    label: str,
) -> tuple[bool, list[CommandObservation], str | None]:
    observations: list[CommandObservation] = []
    for command_template in commands:
        command = substitute(command_template, substitutions)
        observation = run_command(command, cwd)
        observations.append(observation)
        if observation.returncode != 0:
            return False, observations, process_failure(observation, label)
    return True, observations, None


def cell_by_id(matrix: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(cell["cell_id"]): cell for cell in matrix["cells"]}


def base_cell_result(
    cell: dict[str, Any],
    *,
    status: str,
    passed: bool,
    duration_seconds: float | None = None,
    message: str | None = None,
    **extra: Any,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "cell_id": cell["cell_id"],
        "family": cell["family"],
        "environment": cell["environment"],
        "status": status,
        "passed": passed,
    }
    if duration_seconds is not None:
        result["duration_seconds"] = rounded(duration_seconds)
    if message:
        result["message"] = message
    result.update(extra)
    return result


def run_build_cell(
    cell: dict[str, Any],
    substitutions: dict[str, str],
    repo_root: Path,
) -> tuple[dict[str, Any], list[CommandObservation]]:
    passed, observations, message = execute_commands(
        cell["commands"], substitutions, repo_root, str(cell["cell_id"])
    )
    duration = sum(item.duration_seconds for item in observations)
    return (
        base_cell_result(
            cell,
            status="executed",
            passed=passed,
            duration_seconds=duration,
            message=message,
        ),
        observations,
    )


def delegated_build_result(
    cell: dict[str, Any],
    substitutions: dict[str, str],
    *,
    available: bool,
) -> dict[str, Any]:
    reachability = cell["reachability"]
    return base_cell_result(
        cell,
        status="delegated",
        passed=available,
        delegated_to=reachability["name"],
        build_directory=substitutions.get(
            {
                "build.default": "default_build_dir",
                "build.warnings": "warnings_build_dir",
                "build.sanitizers": "sanitizer_build_dir",
            }.get(str(cell["cell_id"]), "default_build_dir"),
            "",
        ),
        **({"message": "required build executable was not found"} if not available else {}),
    )


def run_canonical_cell(
    cell: dict[str, Any],
    substitutions: dict[str, str],
    repo_root: Path,
) -> tuple[dict[str, Any], list[CommandObservation], Path]:
    passed, observations, message = execute_commands(
        cell["commands"], substitutions, repo_root, str(cell["cell_id"])
    )
    report_path = Path(substitutions["report_dir"]) / "verification-report.json"
    duration = sum(item.duration_seconds for item in observations)
    return (
        base_cell_result(
            cell,
            status="executed",
            passed=passed,
            duration_seconds=duration,
            message=message,
            report_path="build/matrix/verification-report.json",
        ),
        observations,
        report_path,
    )


def load_report(path: Path) -> tuple[dict[str, Any] | None, str | None]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        return None, f"unable to read canonical report {path}: {error}"
    if not isinstance(value, dict):
        return None, f"canonical report root is not an object: {path}"
    return value, None


def validate_canonical_report(
    report: dict[str, Any],
    matrix: dict[str, Any],
) -> tuple[bool, str | None, dict[str, Any]]:
    summary = report.get("summary")
    if not isinstance(summary, dict):
        return False, "canonical report has no summary", {}
    failed = summary.get("failed")
    untracked = summary.get("untracked_results")
    if failed != 0 or untracked != 0:
        return (
            False,
            f"canonical report is not green: failed={failed}, untracked_results={untracked}",
            {},
        )

    cases = report.get("cases")
    if not isinstance(cases, list):
        return False, "canonical report has no cases", {}
    runners = {str(case.get("runner")) for case in cases if isinstance(case, dict)}
    required_runners = {
        "ctest",
        "golden",
        "artifact",
        "rust_vm_artifact",
        "rust_vm_golden",
        "rust_vm_runtime_error",
        "cargo_test",
    }
    missing = sorted(required_runners - runners)
    if missing:
        return False, "canonical report is missing runners: " + ", ".join(missing), {}
    if report.get("inventory_revision") != matrix["inventory_revision"]:
        return (
            False,
            "canonical report inventory revision mismatch: "
            f"{report.get('inventory_revision')} != {matrix['inventory_revision']}",
            {},
        )
    return True, None, {
        "inventory_revision": report.get("inventory_revision"),
        "total": summary.get("total"),
        "passed": summary.get("passed"),
        "failed": failed,
        "untracked_results": untracked,
        "runners": sorted(runners),
    }


def artifact_size(repo_root: Path, policy: dict[str, Any]) -> dict[str, Any]:
    root = repo_root / str(policy["root"])
    pattern = str(policy["pattern"])
    files = sorted(path for path in root.rglob(pattern) if path.is_file())
    digest = hashlib.sha256()
    entries: list[dict[str, Any]] = []
    total_bytes = 0
    for path in files:
        content = path.read_bytes()
        relative = path.relative_to(repo_root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(content)
        total_bytes += len(content)
        entries.append({"path": relative, "bytes": len(content)})
    return {
        "root": str(policy["root"]),
        "pattern": pattern,
        "file_count": len(files),
        "total_bytes": total_bytes,
        "sha256": digest.hexdigest(),
        "files": entries,
    }


def compiler_inputs(repo_root: Path, workload: dict[str, Any]) -> list[Path]:
    fixture = repo_root / str(workload["fixture"])
    args_path = fixture / "args.txt"
    if args_path.is_file():
        entries = args_path.read_text(encoding="utf-8").split()
        return [fixture / entry for entry in entries]
    return [repo_root / str(source) for source in workload["sources"]]


def run_runtime_workload(
    repo_root: Path,
    compiler: Path,
    vm_dir: Path,
    workload: dict[str, Any],
    repetitions: int,
) -> dict[str, Any]:
    workload_id = str(workload["workload_id"])
    expected_path = repo_root / str(workload["expected_output"])
    expected = expected_path.read_text(encoding="utf-8")
    expected_digest = hashlib.sha256(expected.encode("utf-8")).hexdigest()
    sources = compiler_inputs(repo_root, workload)
    errors: list[str] = []
    samples: list[float] = []

    with tempfile.TemporaryDirectory(prefix=f"compiler-design-{workload_id}-") as temp_dir:
        artifact = Path(temp_dir) / "workload.cdbc"
        compile_command = [str(compiler), "--emit-bytecode", str(artifact), *(str(source) for source in sources)]
        compiled = run_command(compile_command, repo_root)
        if compiled.returncode != 0:
            errors.append(process_failure(compiled, f"workload {workload_id} compile"))
        elif compiled.stdout or compiled.stderr:
            errors.append(f"workload {workload_id} compile produced unexpected output")
        elif not artifact.is_file():
            errors.append(f"workload {workload_id} did not emit an artifact")

        if not errors:
            run_command_line = [
                "cargo",
                "run",
                "--quiet",
                "--manifest-path",
                str(vm_dir / "Cargo.toml"),
                "--",
                "run",
                str(artifact),
            ]
            for repetition in range(repetitions):
                executed = run_command(run_command_line, repo_root)
                if executed.returncode != 0:
                    errors.append(
                        process_failure(
                            executed,
                            f"workload {workload_id} run {repetition + 1}",
                        )
                    )
                    continue
                if executed.stderr:
                    errors.append(
                        f"workload {workload_id} run {repetition + 1} produced stderr: "
                        f"{executed.stderr.rstrip()}"
                    )
                if executed.stdout != expected:
                    errors.append(
                        f"workload {workload_id} run {repetition + 1} stdout mismatch"
                    )
                samples.append(executed.duration_seconds)

    result: dict[str, Any] = {
        "workload_id": workload_id,
        "fixture": workload["fixture"],
        "expected_output": workload["expected_output"],
        "expected_output_sha256": expected_digest,
        "repetitions": repetitions,
        "passed": not errors and len(samples) == repetitions,
        "samples_seconds": [rounded(value) for value in samples],
    }
    if samples:
        result["median_seconds"] = rounded(statistics.median(samples))
    if errors:
        result["errors"] = errors
    return result


def first_line(command: list[str], cwd: Path) -> str:
    observation = run_command(command, cwd)
    output = observation.stdout or observation.stderr
    return output.splitlines()[0] if output.splitlines() else "unavailable"


def cmake_cache_values(build_dir: Path) -> dict[str, str]:
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        return {}
    values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if ":" not in line or line.startswith("//") or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        name = key.split(":", 1)[0]
        if name in {"CMAKE_CXX_COMPILER_ID", "CMAKE_BUILD_TYPE", "CMAKE_GENERATOR"}:
            values[name] = value
    return values


def environment_record(
    repo_root: Path,
    build_mode: str,
    build_dirs: dict[str, Path],
) -> dict[str, Any]:
    uname = platform.uname()
    return {
        "build_mode": build_mode,
        "toolchain": {
            "cmake": first_line(["cmake", "--version"], repo_root),
            "cxx": first_line(["c++", "--version"], repo_root),
            "python": first_line([sys.executable, "--version"], repo_root),
            "cargo": first_line(["cargo", "--version"], repo_root),
            "rustc": first_line(["rustc", "--version"], repo_root),
        },
        "host": {
            "system": uname.system,
            "release": uname.release,
            "version": uname.version,
            "machine": uname.machine,
            "processor": uname.processor,
            "python_implementation": platform.python_implementation(),
            "cpu_count": os.cpu_count(),
        },
        "cmake_cache": {
            name: cmake_cache_values(path)
            for name, path in build_dirs.items()
            if path.is_dir()
        },
    }


def comparable_environment(current: dict[str, Any], baseline: dict[str, Any]) -> bool:
    if current.get("build_mode") != baseline.get("build_mode"):
        return False
    if current.get("toolchain") != baseline.get("toolchain"):
        return False
    current_host = current.get("host", {})
    baseline_host = baseline.get("host", {})
    return (
        current_host.get("system") == baseline_host.get("system")
        and current_host.get("machine") == baseline_host.get("machine")
        and current_host.get("python_implementation") == baseline_host.get("python_implementation")
    )


def value_within_tolerance(current: float, baseline: float, tolerance_percent: float) -> bool:
    if baseline == 0:
        return current == 0
    return abs(current - baseline) / abs(baseline) * 100.0 <= tolerance_percent


def compare_baseline(
    current: dict[str, Any],
    baseline_path: Path,
    matrix: dict[str, Any],
    strict_performance: bool,
) -> dict[str, Any]:
    baseline, load_error = load_report(baseline_path)
    if load_error or baseline is None:
        return {
            "passed": False,
            "deterministic_differences": [load_error or "baseline is empty"],
            "performance": [],
            "disposition": "baseline-unreadable",
        }

    deterministic: list[str] = []
    for field in ("matrix_revision", "inventory_revision"):
        if current.get(field) != baseline.get(field):
            deterministic.append(
                f"{field}: current={current.get(field)!r}, baseline={baseline.get(field)!r}"
            )

    current_artifact = current.get("measurements", {}).get("artifact_size")
    baseline_artifact = baseline.get("measurements", {}).get("artifact_size")
    if current_artifact != baseline_artifact:
        deterministic.append("measurements.artifact_size differs")

    current_workloads = current.get("runtime_workloads", [])
    baseline_workloads = baseline.get("runtime_workloads", [])
    current_workload_ids = [item.get("workload_id") for item in current_workloads]
    baseline_workload_ids = [item.get("workload_id") for item in baseline_workloads]
    if current_workload_ids != baseline_workload_ids:
        deterministic.append(
            f"runtime workload IDs: current={current_workload_ids!r}, baseline={baseline_workload_ids!r}"
        )
    for current_item, baseline_item in zip(current_workloads, baseline_workloads):
        if current_item.get("expected_output_sha256") != baseline_item.get("expected_output_sha256"):
            deterministic.append(
                f"workload {current_item.get('workload_id')} expected output differs"
            )

    environment_matches = comparable_environment(
        current.get("environment", {}), baseline.get("environment", {})
    )
    performance: list[dict[str, Any]] = []
    if environment_matches:
        policies = matrix["measurements"]
        current_compile = current.get("measurements", {}).get("compile_time", {}).get("samples_seconds", {})
        baseline_compile = baseline.get("measurements", {}).get("compile_time", {}).get("samples_seconds", {})
        for name in sorted(set(current_compile) & set(baseline_compile)):
            policy = policies["compile_time"]
            current_value = float(current_compile[name])
            baseline_value = float(baseline_compile[name])
            within = value_within_tolerance(
                current_value, baseline_value, float(policy["tolerance_percent"])
            )
            performance.append({
                "measurement": f"compile_time.{name}",
                "current": current_value,
                "baseline": baseline_value,
                "tolerance_percent": policy["tolerance_percent"],
                "within_tolerance": within,
            })

        for name in ("test_duration",):
            policy = policies[name]
            current_value = current.get("measurements", {}).get(name, {}).get("total_seconds")
            baseline_value = baseline.get("measurements", {}).get(name, {}).get("total_seconds")
            if current_value is None or baseline_value is None:
                continue
            within = value_within_tolerance(
                float(current_value), float(baseline_value), float(policy["tolerance_percent"])
            )
            performance.append({
                "measurement": name,
                "current": current_value,
                "baseline": baseline_value,
                "tolerance_percent": policy["tolerance_percent"],
                "within_tolerance": within,
            })

        for current_item, baseline_item in zip(current_workloads, baseline_workloads):
            current_value = current_item.get("median_seconds")
            baseline_value = baseline_item.get("median_seconds")
            if current_value is None or baseline_value is None:
                continue
            policy = policies["runtime_workload"]
            within = value_within_tolerance(
                float(current_value), float(baseline_value), float(policy["tolerance_percent"])
            )
            performance.append({
                "measurement": f"runtime_workload.{current_item.get('workload_id')}",
                "current": current_value,
                "baseline": baseline_value,
                "tolerance_percent": policy["tolerance_percent"],
                "within_tolerance": within,
            })

    performance_failures = [item for item in performance if not item["within_tolerance"]]
    passed = not deterministic and (not strict_performance or not performance_failures)
    return {
        "passed": passed,
        "deterministic_differences": deterministic,
        "performance": performance,
        "performance_failures": performance_failures,
        "environment_comparable": environment_matches,
        "disposition": (
            "zero unexplained differences"
            if passed
            else "differences require review"
        ),
    }


def run_matrix(
    matrix: dict[str, Any],
    repo_root: Path,
    *,
    mode: str,
    compiler: Path | None,
    vm_dir: Path,
    build_dir: Path | None,
    warnings_build_dir: Path | None,
    sanitizer_build_dir: Path | None,
    canonical_report_path: Path | None,
    run_workloads: bool,
    strict_performance: bool,
    baseline_path: Path | None,
) -> dict[str, Any]:
    cells = cell_by_id(matrix)
    observations: list[CommandObservation] = []
    compile_samples: dict[str, float] = {}
    test_samples: dict[str, float] = {}
    errors: list[str] = []

    temporary_root: tempfile.TemporaryDirectory[str] | None = None
    if mode == "clean":
        temporary_root = tempfile.TemporaryDirectory(prefix="compiler-design-m0d-")
        root = Path(temporary_root.name)
        default_build = root / "default"
        warning_build = root / "warnings"
        sanitizer_build = root / "sanitizers"
    else:
        if compiler is None:
            raise ValueError("reuse mode requires a compiler path")
        default_build = (build_dir or compiler.parent).resolve()
        warning_build = (warnings_build_dir or default_build).resolve()
        sanitizer_build = (sanitizer_build_dir or repo_root / "build-sanitize").resolve()
        root = repo_root / "build" / "matrix"

    report_dir = root / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)
    if mode == "clean":
        compiler = default_build / "compiler_design"

    substitutions = {
        "default_build_dir": str(default_build),
        "warnings_build_dir": str(warning_build),
        "sanitizer_build_dir": str(sanitizer_build),
        "compiler": str(compiler),
        "vm": str(vm_dir),
        "report_dir": str(report_dir),
    }

    cell_results: dict[str, dict[str, Any]] = {}
    build_cell_ids = ("build.default", "build.warnings", "build.sanitizers")
    for cell_id in build_cell_ids:
        cell = cells[cell_id]
        if mode == "clean":
            result, command_observations = run_build_cell(cell, substitutions, repo_root)
            observations.extend(command_observations)
            compile_samples[cell_id] = sum(item.duration_seconds for item in command_observations)
        else:
            executable = Path(substitutions[{
                "build.default": "default_build_dir",
                "build.warnings": "warnings_build_dir",
                "build.sanitizers": "sanitizer_build_dir",
            }[cell_id]]) / "compiler_design"
            result = delegated_build_result(
                cell,
                substitutions,
                available=(executable.is_file() or cell_id == "build.sanitizers"),
            )
        cell_results[cell_id] = result
        if not result["passed"] and result.get("message"):
            errors.append(result["message"])

    sanitizer_ctest = cells["tests.sanitizer_ctest"]
    if mode == "clean" or sanitizer_build.is_dir():
        passed, command_observations, message = execute_commands(
            sanitizer_ctest["commands"], substitutions, repo_root, str(sanitizer_ctest["cell_id"])
        )
        observations.extend(command_observations)
        duration = sum(item.duration_seconds for item in command_observations)
        test_samples["sanitizer_ctest"] = duration
        sanitizer_result = base_cell_result(
            sanitizer_ctest,
            status="executed",
            passed=passed,
            duration_seconds=duration,
            message=message,
        )
    else:
        sanitizer_result = base_cell_result(
            sanitizer_ctest,
            status="delegated",
            passed=True,
            delegated_to=sanitizer_ctest["reachability"]["name"],
        )
    cell_results["tests.sanitizer_ctest"] = sanitizer_result
    if not sanitizer_result["passed"] and sanitizer_result.get("message"):
        errors.append(sanitizer_result["message"])

    canonical_cell = cells["canonical.verification"]
    canonical_report: dict[str, Any] | None = None
    canonical_report_summary: dict[str, Any] = {}
    if canonical_report_path is not None:
        canonical_report, load_error = load_report(canonical_report_path.resolve())
        if load_error:
            canonical_result = base_cell_result(
                canonical_cell,
                status="reused",
                passed=False,
                message=load_error,
                reused_report=str(canonical_report_path),
            )
            errors.append(load_error)
        else:
            passed, message, canonical_report_summary = validate_canonical_report(canonical_report, matrix)
            canonical_result = base_cell_result(
                canonical_cell,
                status="reused",
                passed=passed,
                message=message,
                reused_report=str(canonical_report_path),
            )
            if not passed and message:
                errors.append(message)
    else:
        canonical_result, command_observations, generated_report_path = run_canonical_cell(
            canonical_cell, substitutions, repo_root
        )
        observations.extend(command_observations)
        test_samples["canonical_verification"] = sum(
            item.duration_seconds for item in command_observations
        )
        canonical_report, load_error = load_report(generated_report_path)
        if load_error:
            canonical_result["passed"] = False
            canonical_result["message"] = load_error
            errors.append(load_error)
        else:
            passed, message, canonical_report_summary = validate_canonical_report(canonical_report, matrix)
            canonical_result["passed"] = canonical_result["passed"] and passed
            if message:
                canonical_result["message"] = message
                errors.append(message)
    cell_results["canonical.verification"] = canonical_result
    if not canonical_result["passed"] and canonical_result.get("message"):
        if canonical_result["message"] not in errors:
            errors.append(canonical_result["message"])

    canonical_ok = bool(canonical_result["passed"])
    for cell_id, cell in cells.items():
        if cell.get("execution") != "canonical":
            continue
        cell_results[cell_id] = base_cell_result(
            cell,
            status="covered",
            passed=canonical_ok,
            covered_by="canonical.verification",
            duplicate_of=cell.get("duplicate_of"),
            **({"message": "canonical runner did not produce a green report"} if not canonical_ok else {}),
        )

    artifact_measurement = artifact_size(repo_root, matrix["measurements"]["artifact_size"])
    workload_results: list[dict[str, Any]] = []
    if run_workloads:
        repetitions = int(matrix["measurements"]["runtime_workload"]["repetitions"])
        if compiler is None or not compiler.is_file():
            errors.append(f"compiler not found for runtime workloads: {compiler}")
            workload_results = [
                {
                    "workload_id": workload["workload_id"],
                    "passed": False,
                    "errors": ["compiler unavailable"],
                }
                for workload in matrix["workloads"]
            ]
        else:
            for workload in matrix["workloads"]:
                workload_result = run_runtime_workload(
                    repo_root, compiler, vm_dir, workload, repetitions
                )
                workload_results.append(workload_result)
                if not workload_result["passed"]:
                    errors.extend(workload_result.get("errors", [
                        f"runtime workload failed: {workload_result['workload_id']}"
                    ]))

    test_total = sum(test_samples.values())
    compile_total = sum(compile_samples.values())
    build_dirs = {
        "default": default_build,
        "warnings": warning_build,
        "sanitizers": sanitizer_build,
    }
    current_commit = first_line(["git", "rev-parse", "HEAD"], repo_root)
    report: dict[str, Any] = {
        "schema_version": 1,
        "milestone": "M0D",
        "matrix_revision": matrix["matrix_revision"],
        "inventory_revision": matrix["inventory_revision"],
        "reference_commit": matrix["reference_commit"],
        "commit": current_commit,
        "mode": mode,
        "canonical_command": matrix["canonical_command"],
        "matrix_definition": {
            "cells": [
                {
                    "cell_id": cell["cell_id"],
                    "family": cell["family"],
                    "environment": cell["environment"],
                    "commands": cell["commands"],
                    "coverage": cell["coverage"],
                    "reachability": cell["reachability"],
                    **({"duplicate_of": cell["duplicate_of"]} if "duplicate_of" in cell else {}),
                }
                for cell in matrix["cells"]
            ],
            "measurement_policies": matrix["measurements"],
            "workloads": matrix["workloads"],
        },
        "environment": environment_record(repo_root, mode, build_dirs),
        "cells": [cell_results[str(cell["cell_id"])] for cell in matrix["cells"]],
        "measurements": {
            "compile_time": {
                "unit": matrix["measurements"]["compile_time"]["unit"],
                "statistic": matrix["measurements"]["compile_time"]["statistic"],
                "repetitions": matrix["measurements"]["compile_time"]["repetitions"],
                "samples_seconds": {
                    name: rounded(value) for name, value in sorted(compile_samples.items())
                },
                "total_seconds": rounded(compile_total),
            },
            "test_duration": {
                "unit": matrix["measurements"]["test_duration"]["unit"],
                "statistic": matrix["measurements"]["test_duration"]["statistic"],
                "repetitions": matrix["measurements"]["test_duration"]["repetitions"],
                "samples_seconds": {
                    name: rounded(value) for name, value in sorted(test_samples.items())
                },
                "total_seconds": rounded(test_total),
            },
            "artifact_size": artifact_measurement,
            "runtime_workload": {
                "unit": matrix["measurements"]["runtime_workload"]["unit"],
                "statistic": matrix["measurements"]["runtime_workload"]["statistic"],
                "repetitions": matrix["measurements"]["runtime_workload"]["repetitions"],
                "samples_seconds": {
                    str(result["workload_id"]): result.get("samples_seconds", [])
                    for result in workload_results
                },
                "median_seconds": {
                    str(result["workload_id"]): result.get("median_seconds")
                    for result in workload_results
                    if result.get("median_seconds") is not None
                },
            },
        },
        "canonical_observation": canonical_report_summary,
        "runtime_workloads": workload_results,
        "summary": {
            "total_cells": len(cell_results),
            "passed_cells": sum(result["passed"] for result in cell_results.values()),
            "failed_cells": sum(not result["passed"] for result in cell_results.values()),
            "executed_cells": sum(result["status"] == "executed" for result in cell_results.values()),
            "covered_cells": sum(result["status"] == "covered" for result in cell_results.values()),
            "delegated_cells": sum(result["status"] == "delegated" for result in cell_results.values()),
            "reused_cells": sum(result["status"] == "reused" for result in cell_results.values()),
            "total_workloads": len(workload_results),
            "passed_workloads": sum(result.get("passed", False) for result in workload_results),
            "failed_workloads": sum(not result.get("passed", False) for result in workload_results),
        },
        "errors": errors,
    }
    if baseline_path is not None:
        report["comparison"] = compare_baseline(
            report, baseline_path, matrix, strict_performance
        )
    else:
        report["comparison"] = {
            "passed": True,
            "deterministic_differences": [],
            "performance": [],
            "disposition": "baseline not requested",
        }

    if temporary_root is not None:
        temporary_root.cleanup()
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the M0D verification matrix.")
    parser.add_argument("compiler", nargs="?", type=Path, help="compiler_design path for reuse mode")
    parser.add_argument("vm", nargs="?", type=Path, help="vm-rs directory or Cargo.toml")
    parser.add_argument("--mode", choices=("clean", "reuse"), help="clean temporary builds or reuse existing builds")
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--baseline", type=Path, help="compare deterministic fields with a baseline report")
    parser.add_argument("--strict-performance", action="store_true", help="fail when comparable performance exceeds tolerance")
    parser.add_argument("--build-dir", type=Path, help="default build directory in reuse mode")
    parser.add_argument("--warnings-build-dir", type=Path, help="warning build directory in reuse mode")
    parser.add_argument("--sanitizer-build-dir", type=Path, help="sanitizer build directory in reuse mode")
    parser.add_argument("--canonical-report", type=Path, help="reuse an already generated canonical report")
    parser.add_argument("--skip-workloads", action="store_true", help="skip runtime workload execution")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = REPO_ROOT.resolve()
    try:
        matrix = verification_matrix.load_matrix(args.matrix.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"verification matrix: {error}", file=sys.stderr)
        return 64
    errors = verification_matrix.validate_matrix(matrix, repo_root)
    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        return 1

    mode = args.mode or ("reuse" if args.compiler is not None else "clean")
    compiler = args.compiler.resolve() if args.compiler is not None else None
    vm_path = args.vm.resolve() if args.vm is not None else repo_root / "vm-rs"
    vm_dir = vm_path if vm_path.is_dir() else vm_path.parent
    if not (vm_dir / "Cargo.toml").is_file():
        print(f"Rust VM manifest not found under: {vm_dir}", file=sys.stderr)
        return 64
    if mode == "reuse" and (compiler is None or not compiler.is_file()):
        print(f"compiler not found for reuse mode: {compiler}", file=sys.stderr)
        return 64

    try:
        report = run_matrix(
            matrix,
            repo_root,
            mode=mode,
            compiler=compiler,
            vm_dir=vm_dir,
            build_dir=args.build_dir,
            warnings_build_dir=args.warnings_build_dir,
            sanitizer_build_dir=args.sanitizer_build_dir,
            canonical_report_path=args.canonical_report,
            run_workloads=not args.skip_workloads,
            strict_performance=args.strict_performance,
            baseline_path=args.baseline.resolve() if args.baseline else None,
        )
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"verification matrix: {error}", file=sys.stderr)
        return 1

    report_path = args.report.resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    summary = report["summary"]
    print(
        "verification matrix: "
        f"{summary['passed_cells']} passed cells, "
        f"{summary['failed_cells']} failed cells, "
        f"{summary['passed_workloads']} passed workloads, "
        f"{summary['failed_workloads']} failed workloads"
    )
    for error in report["errors"]:
        print(f"FAIL {error}", file=sys.stderr)
    comparison = report["comparison"]
    if not comparison.get("passed", False):
        for difference in comparison.get("deterministic_differences", []):
            print(f"FAIL baseline: {difference}", file=sys.stderr)
        if args.strict_performance:
            for difference in comparison.get("performance_failures", []):
                print(f"FAIL performance: {difference}", file=sys.stderr)
    return 1 if report["errors"] or not comparison.get("passed", False) else 0


if __name__ == "__main__":
    raise SystemExit(main())
