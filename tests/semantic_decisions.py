#!/usr/bin/env python3

"""Validate the versioned M0.5A semantic decision log."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
DECISION_PATH = Path(__file__).resolve().parent.parent / "docs" / "decisions" / "m05a-semantic-decisions.json"
DECISION_ID_PATTERN = re.compile(r"^SEM-[A-Z0-9-]+$")
REQUIRED_DOMAINS = {
    "evaluation-and-effects",
    "binding-and-scope",
    "types",
    "source-and-ranges",
    "diagnostics",
    "modules",
    "patterns",
    "backend-boundary",
}
ALLOWED_STATUSES = {"resolved", "open", "deferred"}


def load_decisions(path: Path = DECISION_PATH) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"decision log root must be an object: {path}")
    return value


def _check_string_list(value: Any, label: str, errors: list[str], *, non_empty: bool = True) -> None:
    if not isinstance(value, list) or (non_empty and not value) or not all(
        isinstance(item, str) and item for item in value
    ):
        errors.append(f"{label} must be a list of non-empty strings")


def _validate_entries(
    decisions: dict[str, Any],
    repo_root: Path,
    errors: list[str],
) -> tuple[set[str], set[str]]:
    entries = decisions.get("entries")
    if not isinstance(entries, list) or not entries:
        errors.append("entries must be a non-empty list")
        return set(), set()

    decision_ids: list[str] = []
    domains: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            errors.append("every decision entry must be an object")
            continue
        decision_id = str(entry.get("decision_id", "<unknown>"))
        decision_ids.append(decision_id)
        for field in (
            "decision_id",
            "domain",
            "question",
            "decision",
            "status",
            "owner",
            "m1_blocking",
            "rationale",
            "affected_consumers",
            "compatibility_consequences",
            "evidence",
        ):
            if field not in entry:
                errors.append(f"{decision_id} missing field: {field}")
        if not DECISION_ID_PATTERN.match(decision_id):
            errors.append(f"invalid decision_id: {decision_id}")
        domain = entry.get("domain")
        if not isinstance(domain, str) or not domain:
            errors.append(f"{decision_id} domain must be a non-empty string")
        else:
            domains.add(domain)
        status = entry.get("status")
        if status not in ALLOWED_STATUSES:
            errors.append(f"{decision_id} has invalid status: {status}")
        if not isinstance(entry.get("m1_blocking"), bool):
            errors.append(f"{decision_id}.m1_blocking must be boolean")
        elif entry["m1_blocking"] and status != "resolved":
            errors.append(f"{decision_id} blocks M1 but is not resolved")
        if status in {"open", "deferred"}:
            closure = entry.get("closure")
            if not isinstance(closure, dict):
                errors.append(f"{decision_id} {status} entry requires closure")
            else:
                for field in ("milestone", "condition"):
                    if not isinstance(closure.get(field), str) or not closure[field]:
                        errors.append(f"{decision_id}.closure.{field} is required")
        _check_string_list(entry.get("affected_consumers"), f"{decision_id}.affected_consumers", errors)
        evidence = entry.get("evidence")
        _check_string_list(evidence, f"{decision_id}.evidence", errors)
        if isinstance(evidence, list):
            for path_value in evidence:
                if not (repo_root / path_value).exists():
                    errors.append(f"{decision_id} evidence path does not exist: {path_value}")

    if len(decision_ids) != len(set(decision_ids)):
        errors.append("decision IDs must be unique")
    if decision_ids != sorted(decision_ids):
        errors.append("decision entries must be sorted by decision_id")
    return set(decision_ids), domains


def _validate_duplicate_inventory(
    decisions: dict[str, Any],
    repo_root: Path,
    errors: list[str],
) -> None:
    inventory = decisions.get("duplicated_decision_inventory")
    if not isinstance(inventory, dict):
        errors.append("duplicated_decision_inventory must be an object")
        return
    for field in (
        "inventory_id",
        "baseline_commit",
        "command",
        "canonicalization",
        "comparison_statistic",
        "categories",
        "decision_families",
    ):
        if field not in inventory:
            errors.append(f"duplicated_decision_inventory missing field: {field}")
    _check_string_list(inventory.get("command"), "duplicated_decision_inventory.command", errors)
    if isinstance(inventory.get("command"), list) and "tests/semantic_decision_inventory.py" not in inventory["command"]:
        errors.append("duplicate inventory command must invoke semantic_decision_inventory.py")

    categories = inventory.get("categories")
    if not isinstance(categories, list) or not categories:
        errors.append("duplicated_decision_inventory.categories must be a non-empty list")
        categories = []
    category_ids: list[str] = []
    site_ids: set[str] = set()
    for category in categories:
        if not isinstance(category, dict):
            errors.append("every duplicate inventory category must be an object")
            continue
        category_id = str(category.get("category_id", "<unknown>"))
        category_ids.append(category_id)
        for field in ("category_id", "description", "files", "patterns"):
            if field not in category:
                errors.append(f"duplicate category {category_id} missing field: {field}")
        _check_string_list(category.get("files"), f"duplicate category {category_id}.files", errors)
        if isinstance(category.get("files"), list):
            for path_value in category["files"]:
                if not (repo_root / path_value).is_file():
                    errors.append(f"duplicate category {category_id} missing file: {path_value}")
        patterns = category.get("patterns")
        if not isinstance(patterns, list) or not patterns:
            errors.append(f"duplicate category {category_id}.patterns must be non-empty")
            patterns = []
        pattern_ids: list[str] = []
        for pattern in patterns:
            if not isinstance(pattern, dict):
                errors.append(f"duplicate category {category_id} pattern must be an object")
                continue
            pattern_id = str(pattern.get("pattern_id", "<unknown>"))
            pattern_ids.append(pattern_id)
            site_ids.add(f"{category_id}.{pattern_id}")
            if not isinstance(pattern.get("regex"), str) or not pattern["regex"]:
                errors.append(f"duplicate pattern {category_id}.{pattern_id} regex is required")
            else:
                try:
                    re.compile(pattern["regex"])
                except re.error as error:
                    errors.append(f"duplicate pattern {category_id}.{pattern_id} regex is invalid: {error}")
        if pattern_ids != sorted(pattern_ids):
            errors.append(f"duplicate category {category_id}.patterns must be sorted")
        if len(pattern_ids) != len(set(pattern_ids)):
            errors.append(f"duplicate category {category_id}.pattern IDs must be unique")
    if category_ids != sorted(category_ids):
        errors.append("duplicate inventory categories must be sorted")
    if len(category_ids) != len(set(category_ids)):
        errors.append("duplicate inventory category IDs must be unique")

    families = inventory.get("decision_families")
    if not isinstance(families, list) or len(families) < 3:
        errors.append("duplicate inventory requires at least three decision families")
        families = []
    family_ids: list[str] = []
    for family in families:
        if not isinstance(family, dict):
            errors.append("every decision family must be an object")
            continue
        family_id = str(family.get("family_id", "<unknown>"))
        family_ids.append(family_id)
        for field in ("family_id", "sites", "migration_owner", "deletion_condition"):
            if field not in family:
                errors.append(f"decision family {family_id} missing field: {field}")
        _check_string_list(family.get("sites"), f"decision family {family_id}.sites", errors)
        if isinstance(family.get("sites"), list):
            for site in family["sites"]:
                if site not in site_ids:
                    errors.append(f"decision family {family_id} references unknown site: {site}")
    if len(family_ids) != len(set(family_ids)):
        errors.append("decision family IDs must be unique")


def validate_decisions(decisions: dict[str, Any], repo_root: Path) -> list[str]:
    errors: list[str] = []
    if decisions.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"schema_version must be {SCHEMA_VERSION}")
    for field in ("decision_revision", "baseline_commit", "inventory_revision", "purpose"):
        if not decisions.get(field):
            errors.append(f"{field} is required")
    status_policy = decisions.get("status_policy")
    if not isinstance(status_policy, dict) or set(status_policy) != ALLOWED_STATUSES:
        errors.append("status_policy must describe exactly resolved, open, and deferred")
    _, domains = _validate_entries(decisions, repo_root, errors)
    errors.extend(f"missing required decision domain: {domain}" for domain in sorted(REQUIRED_DOMAINS - domains))
    _validate_duplicate_inventory(decisions, repo_root, errors)

    inventory_path = repo_root / "tests" / "verification_inventory.json"
    try:
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        errors.append(f"unable to read verification inventory: {error}")
    else:
        if decisions.get("inventory_revision") != inventory.get("inventory_revision"):
            errors.append(
                "decision inventory_revision does not match verification inventory: "
                f"{decisions.get('inventory_revision')} != {inventory.get('inventory_revision')}"
            )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the M0.5A semantic decision log.")
    parser.add_argument("--decision-log", type=Path, default=DECISION_PATH)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    try:
        decisions = load_decisions(args.decision_log.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"semantic decisions: {error}", file=sys.stderr)
        return 1
    errors = validate_decisions(decisions, args.repo_root.resolve())
    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        return 1
    print(
        f"semantic decisions: {len(decisions['entries'])} entries, "
        f"{len(decisions['duplicated_decision_inventory']['categories'])} duplicate-site categories validated"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
