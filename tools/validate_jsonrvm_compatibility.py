#!/usr/bin/env python3
"""Validate the versioned jsonRVM -> AVM compatibility inventory.

This tool validates metadata and frozen legacy assertions only. It never
interprets jsonRVM programs, so it cannot become a second execution path.
"""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "compat" / "jsonrvm-semantics.json"
GOLDEN = ROOT / "compat" / "jsonrvm-golden.json"

ALLOWED_CATEGORIES = {
    "canonical-semantic",
    "projection-syntax",
    "effect-adapter",
    "implementation-artifact",
    "defer",
}
ALLOWED_STATUSES = {"selected", "derive-fixture"}
ALLOWED_EQUIVALENCE = {"observable-json-value", "legacy-provenance-only"}
REQUIRED_ENTRY_FIELDS = {
    "id",
    "name",
    "category",
    "source_paths",
    "behavior",
    "current_avm",
    "decision",
    "target_issue",
}


def fail(message: str) -> None:
    raise SystemExit(f"jsonRVM compatibility manifest: {message}")


def load_json(path: pathlib.Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read valid JSON from {path.relative_to(ROOT)}: {exc}")
    if not isinstance(value, dict):
        fail(f"{path.relative_to(ROOT)} root must be an object")
    return value


def require_nonempty_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        fail(f"{field} must be a non-empty string")
    return value


def require_source(source: object, prefix: str, require_runtime: bool) -> str:
    if not isinstance(source, dict):
        fail(f"{prefix} must be an object")
    if source.get("repository") != "netkeep80/jsonRVM":
        fail(f"{prefix}.repository must remain netkeep80/jsonRVM")
    commit = require_nonempty_string(source.get("commit"), f"{prefix}.commit")
    if len(commit) != 40 or any(ch not in "0123456789abcdef" for ch in commit):
        fail(f"{prefix}.commit must be a lowercase 40-character Git commit SHA")
    if require_runtime:
        require_nonempty_string(source.get("runtime_version"), f"{prefix}.runtime_version")
    return commit


def validate_manifest(data: dict[str, object]) -> tuple[str, int, set[str]]:
    if data.get("schema_version") != 1:
        fail("compat/jsonrvm-semantics.json schema_version must be 1")

    commit = require_source(data.get("source"), "source", require_runtime=True)

    declared_categories = data.get("categories")
    if set(declared_categories or []) != ALLOWED_CATEGORIES:
        fail("categories must exactly match the version-1 classification vocabulary")

    entries = data.get("entries")
    if not isinstance(entries, list) or not entries:
        fail("entries must be a non-empty array")

    entry_ids: set[str] = set()
    for index, entry in enumerate(entries):
        prefix = f"entries[{index}]"
        if not isinstance(entry, dict):
            fail(f"{prefix} must be an object")
        missing = REQUIRED_ENTRY_FIELDS - entry.keys()
        if missing:
            fail(f"{prefix} misses fields: {', '.join(sorted(missing))}")

        entry_id = require_nonempty_string(entry["id"], f"{prefix}.id")
        if entry_id in entry_ids:
            fail(f"duplicate entry id {entry_id}")
        entry_ids.add(entry_id)

        require_nonempty_string(entry["name"], f"{prefix}.name")
        require_nonempty_string(entry["behavior"], f"{prefix}.behavior")
        require_nonempty_string(entry["current_avm"], f"{prefix}.current_avm")
        require_nonempty_string(entry["decision"], f"{prefix}.decision")

        if entry["category"] not in ALLOWED_CATEGORIES:
            fail(f"{prefix}.category is unknown: {entry['category']!r}")

        paths = entry["source_paths"]
        if not isinstance(paths, list) or not paths:
            fail(f"{prefix}.source_paths must be a non-empty array")
        for path_index, path in enumerate(paths):
            require_nonempty_string(path, f"{prefix}.source_paths[{path_index}]")

        issue = entry["target_issue"]
        if not isinstance(issue, int) or issue < 122:
            fail(f"{prefix}.target_issue must reference the AVM 1.5 issue family")

    corpus = data.get("corpus_candidates")
    if not isinstance(corpus, list) or not corpus:
        fail("corpus_candidates must be a non-empty array")

    case_ids: set[str] = set()
    selected_case_ids: set[str] = set()
    for index, case in enumerate(corpus):
        prefix = f"corpus_candidates[{index}]"
        if not isinstance(case, dict):
            fail(f"{prefix} must be an object")
        case_id = require_nonempty_string(case.get("id"), f"{prefix}.id")
        if case_id in case_ids:
            fail(f"duplicate corpus case id {case_id}")
        case_ids.add(case_id)
        require_nonempty_string(case.get("source"), f"{prefix}.source")
        require_nonempty_string(case.get("notes"), f"{prefix}.notes")

        status = case.get("status")
        if status not in ALLOWED_STATUSES:
            fail(f"{prefix}.status must be one of {sorted(ALLOWED_STATUSES)}")
        if status == "selected":
            selected_case_ids.add(case_id)

        covers = case.get("covers")
        if not isinstance(covers, list) or not covers:
            fail(f"{prefix}.covers must be a non-empty array")
        unknown = [entry_id for entry_id in covers if entry_id not in entry_ids]
        if unknown:
            fail(f"{prefix}.covers references unknown entries: {', '.join(unknown)}")

    return commit, len(entries), selected_case_ids


def validate_golden(data: dict[str, object], source_commit: str, selected_case_ids: set[str]) -> int:
    if data.get("schema_version") != 1:
        fail("compat/jsonrvm-golden.json schema_version must be 1")

    golden_commit = require_source(data.get("source"), "golden.source", require_runtime=False)
    if golden_commit != source_commit:
        fail("semantic inventory and golden assertions must pin the same jsonRVM commit")

    source = data["source"]
    assert isinstance(source, dict)
    require_nonempty_string(source.get("assertions_from"), "golden.source.assertions_from")

    cases = data.get("cases")
    if not isinstance(cases, list) or not cases:
        fail("golden.cases must be a non-empty array")

    golden_ids: set[str] = set()
    for index, case in enumerate(cases):
        prefix = f"golden.cases[{index}]"
        if not isinstance(case, dict):
            fail(f"{prefix} must be an object")
        case_id = require_nonempty_string(case.get("id"), f"{prefix}.id")
        if case_id in golden_ids:
            fail(f"duplicate golden case id {case_id}")
        golden_ids.add(case_id)

        require_nonempty_string(case.get("fixture"), f"{prefix}.fixture")
        require_nonempty_string(case.get("test_name"), f"{prefix}.test_name")
        require_nonempty_string(case.get("avm_target"), f"{prefix}.avm_target")
        if case.get("equivalence") not in ALLOWED_EQUIVALENCE:
            fail(f"{prefix}.equivalence must be one of {sorted(ALLOWED_EQUIVALENCE)}")

        assertions = case.get("assertions")
        if not isinstance(assertions, list) or not assertions:
            fail(f"{prefix}.assertions must be a non-empty array")
        assertion_paths: set[str] = set()
        for assertion_index, assertion in enumerate(assertions):
            assertion_prefix = f"{prefix}.assertions[{assertion_index}]"
            if not isinstance(assertion, dict):
                fail(f"{assertion_prefix} must be an object")
            path = require_nonempty_string(assertion.get("path"), f"{assertion_prefix}.path")
            if not path.startswith("/"):
                fail(f"{assertion_prefix}.path must be an absolute JSON pointer used only as a legacy oracle path")
            if path in assertion_paths:
                fail(f"duplicate assertion path {path} in {case_id}")
            assertion_paths.add(path)
            if "equals" not in assertion:
                fail(f"{assertion_prefix} must contain equals")

    missing_golden = selected_case_ids - golden_ids
    if missing_golden:
        fail(f"selected corpus cases lack frozen golden assertions: {', '.join(sorted(missing_golden))}")

    return len(cases)


def main() -> int:
    manifest = load_json(MANIFEST)
    source_commit, entry_count, selected_case_ids = validate_manifest(manifest)
    golden_count = validate_golden(load_json(GOLDEN), source_commit, selected_case_ids)

    corpus = manifest["corpus_candidates"]
    assert isinstance(corpus, list)
    print(
        "jsonRVM compatibility inventory OK: "
        f"{entry_count} semantic entries, {len(corpus)} corpus candidates, "
        f"{golden_count} frozen legacy cases, source {source_commit[:12]}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
