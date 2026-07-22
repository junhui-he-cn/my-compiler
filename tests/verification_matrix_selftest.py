#!/usr/bin/env python3

import copy
import json
import sys
import unittest
from pathlib import Path


TESTS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS_DIR))

import run_verification_matrix  # noqa: E402
import verification_matrix  # noqa: E402


class VerificationMatrixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo_root = TESTS_DIR.parent
        self.matrix = verification_matrix.load_matrix(TESTS_DIR / "verification_matrix.json")

    def test_checked_in_matrix_is_valid(self) -> None:
        self.assertEqual(verification_matrix.validate_matrix(self.matrix, self.repo_root), [])

    def test_required_coverage_is_explicit(self) -> None:
        coverage = {
            tag
            for cell in self.matrix["cells"]
            for tag in cell["coverage"]
        }
        self.assertTrue(verification_matrix.REQUIRED_COVERAGE.issubset(coverage))
        self.assertEqual(
            {cell["cell_id"] for cell in self.matrix["cells"] if cell["execution"] == "canonical"},
            {
                "tests.artifact",
                "tests.cargo",
                "tests.ctest",
                "tests.golden",
                "tests.rust_vm",
            },
        )

    def test_validation_detects_missing_coverage(self) -> None:
        changed = copy.deepcopy(self.matrix)
        changed["cells"] = [
            cell for cell in changed["cells"] if "warning-build" not in cell["coverage"]
        ]
        errors = verification_matrix.validate_matrix(changed, self.repo_root)
        self.assertIn("missing required coverage: warning-build", errors)

    def test_validation_detects_duplicate_cell_ids(self) -> None:
        changed = copy.deepcopy(self.matrix)
        changed["cells"].append(copy.deepcopy(changed["cells"][0]))
        errors = verification_matrix.validate_matrix(changed, self.repo_root)
        self.assertIn("matrix cell IDs must be unique", errors)

    def test_command_substitution_and_tolerance_are_stable(self) -> None:
        command = ["cmake", "-B", "{default_build_dir}", "{compiler}"]
        substituted = run_verification_matrix.substitute(
            command,
            {
                "default_build_dir": "/tmp/default",
                "warnings_build_dir": "/tmp/warnings",
                "sanitizer_build_dir": "/tmp/sanitizers",
                "compiler": "/tmp/compiler_design",
                "vm": "/tmp/vm-rs",
                "report_dir": "/tmp/reports",
            },
        )
        self.assertEqual(substituted, ["cmake", "-B", "/tmp/default", "/tmp/compiler_design"])
        self.assertTrue(run_verification_matrix.value_within_tolerance(11.0, 10.0, 10.0))
        self.assertFalse(run_verification_matrix.value_within_tolerance(11.1, 10.0, 10.0))
        self.assertTrue(run_verification_matrix.value_within_tolerance(0.0, 0.0, 0.0))

    def test_artifact_measurement_contains_a_digest(self) -> None:
        measurement = run_verification_matrix.artifact_size(
            self.repo_root,
            self.matrix["measurements"]["artifact_size"],
        )
        self.assertGreater(measurement["file_count"], 0)
        self.assertGreater(measurement["total_bytes"], 0)
        self.assertEqual(len(measurement["sha256"]), 64)
        self.assertEqual(
            measurement["file_count"],
            len(measurement["files"]),
        )

    def test_canonical_report_projection_rejects_missing_runner(self) -> None:
        report = {
            "inventory_revision": self.matrix["inventory_revision"],
            "summary": {"failed": 0, "untracked_results": 0, "total": 1, "passed": 1},
            "cases": [{"runner": "ctest"}],
        }
        passed, message, _ = run_verification_matrix.validate_canonical_report(report, self.matrix)
        self.assertFalse(passed)
        self.assertIn("missing runners", message or "")

    def test_report_projection_is_json_serializable(self) -> None:
        json.dumps(self.matrix)


if __name__ == "__main__":
    unittest.main()
