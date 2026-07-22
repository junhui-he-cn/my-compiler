#!/usr/bin/env python3

import copy
import json
import sys
import unittest
from pathlib import Path


TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TESTS_DIR.parent
sys.path.insert(0, str(TESTS_DIR))

import semantic_decision_inventory  # noqa: E402
import semantic_decisions  # noqa: E402


class SemanticDecisionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.decisions = semantic_decisions.load_decisions()

    def test_checked_in_decisions_are_valid(self) -> None:
        self.assertEqual(semantic_decisions.validate_decisions(self.decisions, REPO_ROOT), [])

    def test_all_required_domains_and_m1_blockers_are_resolved(self) -> None:
        domains = {entry["domain"] for entry in self.decisions["entries"]}
        self.assertTrue(semantic_decisions.REQUIRED_DOMAINS.issubset(domains))
        self.assertTrue(
            all(
                entry["status"] == "resolved"
                for entry in self.decisions["entries"]
                if entry["m1_blocking"]
            )
        )

    def test_validation_rejects_an_unresolved_m1_blocker(self) -> None:
        changed = copy.deepcopy(self.decisions)
        changed["entries"][0]["status"] = "deferred"
        changed["entries"][0]["m1_blocking"] = True
        errors = semantic_decisions.validate_decisions(changed, REPO_ROOT)
        self.assertTrue(any("blocks M1 but is not resolved" in error for error in errors))

    def test_validation_rejects_a_missing_domain(self) -> None:
        changed = copy.deepcopy(self.decisions)
        changed["entries"] = [
            entry
            for entry in changed["entries"]
            if entry["domain"] != "patterns"
        ]
        errors = semantic_decisions.validate_decisions(changed, REPO_ROOT)
        self.assertIn("missing required decision domain: patterns", errors)

    def test_duplicate_site_inventory_is_deterministic(self) -> None:
        first = semantic_decision_inventory.build_report(self.decisions, REPO_ROOT)
        second = semantic_decision_inventory.build_report(self.decisions, REPO_ROOT)
        self.assertEqual(first, second)
        self.assertGreater(first["summary"]["total_matches"], 0)
        self.assertEqual(first["summary"]["category_count"], 3)
        json.dumps(first)

    def test_checked_in_baseline_matches_duplicate_site_inventory(self) -> None:
        baseline = json.loads(
            (REPO_ROOT / "docs" / "verification" / "m05a-baseline.json").read_text(
                encoding="utf-8"
            )
        )
        report = semantic_decision_inventory.build_report(self.decisions, REPO_ROOT)
        self.assertEqual(
            baseline["duplicate_decision_inventory"]["total_matches"],
            report["summary"]["total_matches"],
        )
        baseline_categories = {
            item["category_id"]: item
            for item in baseline["duplicate_decision_inventory"]["categories"]
        }
        report_categories = {
            item["category_id"]: item
            for item in report["categories"]
        }
        self.assertEqual(
            {
                category_id: {
                    "match_count": item["match_count"],
                    "match_sha256": item["match_sha256"],
                }
                for category_id, item in baseline_categories.items()
            },
            {
                category_id: {
                    "match_count": item["match_count"],
                    "match_sha256": item["match_sha256"],
                }
                for category_id, item in report_categories.items()
            },
        )

    def test_validation_rejects_unknown_family_site(self) -> None:
        changed = copy.deepcopy(self.decisions)
        changed["duplicated_decision_inventory"]["decision_families"][0]["sites"].append(
            "missing.category"
        )
        errors = semantic_decisions.validate_decisions(changed, REPO_ROOT)
        self.assertTrue(any("references unknown site" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
