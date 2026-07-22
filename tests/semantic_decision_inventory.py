#!/usr/bin/env python3

"""Measure the M0.5A duplicated type/name/lowering source-site inventory."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TESTS_DIR.parent
DEFAULT_DECISION_LOG = REPO_ROOT / "docs" / "decisions" / "m05a-semantic-decisions.json"
sys.path.insert(0, str(TESTS_DIR))
import semantic_decisions  # noqa: E402


def canonical_text(line: str) -> str:
    return " ".join(line.strip().split())


def scan_category(repo_root: Path, category: dict[str, Any]) -> dict[str, Any]:
    matches: list[dict[str, Any]] = []
    patterns = [
        (str(pattern["pattern_id"]), re.compile(str(pattern["regex"])))
        for pattern in category["patterns"]
    ]
    for file_value in category["files"]:
        path = repo_root / str(file_value)
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            for pattern_id, regex in patterns:
                if regex.search(line):
                    matches.append(
                        {
                            "file": str(file_value),
                            "line": line_number,
                            "pattern_id": pattern_id,
                            "text": canonical_text(line),
                        }
                    )

    matches.sort(key=lambda match: (match["file"], match["line"], match["pattern_id"], match["text"]))
    digest_input = json.dumps(matches, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return {
        "category_id": category["category_id"],
        "description": category["description"],
        "files": category["files"],
        "patterns": category["patterns"],
        "match_count": len(matches),
        "match_sha256": hashlib.sha256(digest_input).hexdigest(),
        "matches": matches,
    }


def build_report(decisions: dict[str, Any], repo_root: Path) -> dict[str, Any]:
    inventory = decisions["duplicated_decision_inventory"]
    categories = [scan_category(repo_root, category) for category in inventory["categories"]]
    categories.sort(key=lambda category: str(category["category_id"]))
    return {
        "schema_version": 1,
        "inventory_id": inventory["inventory_id"],
        "decision_revision": decisions["decision_revision"],
        "baseline_commit": decisions["baseline_commit"],
        "verification_inventory_revision": decisions["inventory_revision"],
        "command": inventory["command"],
        "canonicalization": inventory["canonicalization"],
        "comparison_statistic": inventory["comparison_statistic"],
        "categories": categories,
        "summary": {
            "category_count": len(categories),
            "total_matches": sum(category["match_count"] for category in categories),
            "by_category": {
                str(category["category_id"]): category["match_count"]
                for category in categories
            },
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Measure duplicated semantic decision sites.")
    parser.add_argument("--decision-log", type=Path, default=DEFAULT_DECISION_LOG)
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--report", type=Path, help="Write the machine-readable inventory report")
    args = parser.parse_args()
    repo_root = args.repo_root.resolve()
    try:
        decisions = semantic_decisions.load_decisions(args.decision_log.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"semantic decision inventory: {error}", file=sys.stderr)
        return 64
    errors = semantic_decisions.validate_decisions(decisions, repo_root)
    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        return 1
    report = build_report(decisions, repo_root)
    if args.report:
        report_path = args.report.resolve()
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        "semantic decision inventory: "
        f"{report['summary']['total_matches']} matches in "
        f"{report['summary']['category_count']} categories"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
