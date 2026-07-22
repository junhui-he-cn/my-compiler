#!/usr/bin/env python3

import copy
import json
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace


TESTS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS_DIR))

import run_verification  # noqa: E402
import verification_inventory  # noqa: E402


class VerificationInventoryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo_root = TESTS_DIR.parent
        self.inventory = verification_inventory.load_inventory(
            TESTS_DIR / "verification_inventory.json"
        )

    def test_checked_in_inventory_matches_fixture_discovery(self) -> None:
        self.assertEqual(
            verification_inventory.validate_inventory(self.inventory, self.repo_root),
            [],
        )

    def test_inventory_case_ids_are_unique_and_stable(self) -> None:
        cases = self.inventory["checks"]
        case_ids = [case["case_id"] for case in cases]
        self.assertEqual(len(case_ids), len(set(case_ids)))
        self.assertEqual(case_ids, sorted(case_ids))

    def test_inventory_detects_missing_case(self) -> None:
        changed = copy.deepcopy(self.inventory)
        changed["checks"] = changed["checks"][1:]
        errors = verification_inventory.validate_inventory(changed, self.repo_root)
        self.assertTrue(any(error.startswith("missing case_id:") for error in errors))

    def test_result_mapping_reuses_runner_assertions(self) -> None:
        checks = [
            {"case_id": "suite.one", "runner": "suite", "result_name": "one"},
            {"case_id": "suite.two", "runner": "suite", "result_name": "two"},
        ]
        raw_results = [
            SimpleNamespace(name="one", passed=True, message=""),
            SimpleNamespace(name="two", passed=False, message="expected mismatch"),
        ]
        results, extras = run_verification.record_runner_results(checks, "suite", raw_results)
        self.assertEqual(extras, [])
        self.assertEqual(
            results,
            [
                run_verification.RecordedResult("suite.one", True, ""),
                run_verification.RecordedResult("suite.two", False, "expected mismatch"),
            ],
        )

    def test_result_mapping_reports_missing_and_untracked_results(self) -> None:
        checks = [{"case_id": "suite.one", "runner": "suite", "result_name": "one"}]
        raw_results = [SimpleNamespace(name="unexpected", passed=True, message="")]
        results, extras = run_verification.record_runner_results(checks, "suite", raw_results)
        self.assertEqual(results, [run_verification.RecordedResult("suite.one", False, "suite did not report expected result: one")])
        self.assertEqual(extras, ["suite: untracked result unexpected"])

    def test_report_is_machine_readable_and_marks_missing_results(self) -> None:
        minimal_inventory = {
            "inventory_revision": "test",
            "baseline_commit": "test",
            "checks": [
                {"case_id": "suite.one", "runner": "suite", "stage": "test", "backend": "test"}
            ],
        }
        report = run_verification.build_report(minimal_inventory, [], [])
        self.assertEqual(report["summary"], {"total": 1, "passed": 0, "failed": 1, "untracked_results": 0})
        json.dumps(report)


if __name__ == "__main__":
    unittest.main()
