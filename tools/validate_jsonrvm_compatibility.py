#!/usr/bin/env python3
"""Проверить versioned compatibility contract jsonRVM -> AVM.

Скрипт валидирует только metadata и frozen evidence. Он никогда не исполняет
программы jsonRVM и поэтому не может стать вторым execution path AVM.
"""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "compat" / "jsonrvm-semantics.json"
DETAILS = ROOT / "compat" / "jsonrvm-semantics-details.json"
DOCTEST_GOLDEN = ROOT / "compat" / "jsonrvm-golden.json"
ORACLE_GOLDEN = ROOT / "compat" / "jsonrvm-oracle-golden.json"

ALLOWED_CATEGORIES = {
    "canonical-semantic",
    "projection-syntax",
    "effect-adapter",
    "implementation-artifact",
    "defer",
}
ALLOWED_CANDIDATE_STATUSES = {"selected", "derive-fixture"}
ALLOWED_MUTATIONS = {
    "none",
    "context-local",
    "runtime-state",
    "model-write",
    "external-effect",
    "implementation-only",
    "deferred",
}
ALLOWED_FIXTURE_STATUSES = {
    "frozen",
    "derive-fixture",
    "effect-deferred",
    "not-semantic",
}
ALLOWED_DOCTEST_EQUIVALENCE = {"observable-json-value", "legacy-provenance-only"}
ALLOWED_ORACLE_EQUIVALENCE = {"observable-json-value", "observable-failure"}

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
REQUIRED_DETAIL_FIELDS = {
    "mutation",
    "context_dependencies",
    "failure_contract",
    "fixture_status",
}


def fail(message: str) -> None:
    raise SystemExit(f"jsonRVM compatibility: {message}")


def load_json(path: pathlib.Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"не удалось прочитать {path.relative_to(ROOT)}: {exc}")
    if not isinstance(value, dict):
        fail(f"{path.relative_to(ROOT)}: корень должен быть JSON object")
    return value


def nonempty_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        fail(f"{field} должен быть непустой строкой")
    return value


def validate_source(source: object, prefix: str, require_runtime: bool = True) -> str:
    if not isinstance(source, dict):
        fail(f"{prefix} должен быть object")
    if source.get("repository") != "netkeep80/jsonRVM":
        fail(f"{prefix}.repository должен оставаться netkeep80/jsonRVM")
    commit = nonempty_string(source.get("commit"), f"{prefix}.commit")
    if len(commit) != 40 or any(ch not in "0123456789abcdef" for ch in commit):
        fail(f"{prefix}.commit должен быть lowercase 40-character Git SHA")
    if require_runtime:
        nonempty_string(source.get("runtime_version"), f"{prefix}.runtime_version")
    return commit


def validate_assertions(assertions: object, prefix: str) -> None:
    if not isinstance(assertions, list) or not assertions:
        fail(f"{prefix} должен быть непустым array")
    paths: set[str] = set()
    for index, assertion in enumerate(assertions):
        item = f"{prefix}[{index}]"
        if not isinstance(assertion, dict):
            fail(f"{item} должен быть object")
        path = nonempty_string(assertion.get("path"), f"{item}.path")
        if not path.startswith("/"):
            fail(f"{item}.path должен быть absolute JSON pointer")
        if path in paths:
            fail(f"{prefix}: duplicate path {path}")
        paths.add(path)
        if "equals" not in assertion:
            fail(f"{item} должен содержать equals")


def validate_manifest(data: dict[str, object]) -> tuple[str, set[str], dict[str, set[str]], set[str]]:
    if data.get("schema_version") != 1:
        fail("jsonrvm-semantics.json schema_version должен быть 1")
    commit = validate_source(data.get("source"), "manifest.source")

    if set(data.get("categories") or []) != ALLOWED_CATEGORIES:
        fail("manifest.categories не совпадает с vocabulary schema v1")

    entries = data.get("entries")
    if not isinstance(entries, list) or not entries:
        fail("manifest.entries должен быть непустым array")

    entry_ids: set[str] = set()
    for index, entry in enumerate(entries):
        prefix = f"manifest.entries[{index}]"
        if not isinstance(entry, dict):
            fail(f"{prefix} должен быть object")
        missing = REQUIRED_ENTRY_FIELDS - entry.keys()
        if missing:
            fail(f"{prefix}: отсутствуют поля {', '.join(sorted(missing))}")
        entry_id = nonempty_string(entry["id"], f"{prefix}.id")
        if entry_id in entry_ids:
            fail(f"duplicate semantic id {entry_id}")
        entry_ids.add(entry_id)
        if entry["category"] not in ALLOWED_CATEGORIES:
            fail(f"{prefix}.category неизвестна: {entry['category']!r}")
        paths = entry["source_paths"]
        if not isinstance(paths, list) or not paths:
            fail(f"{prefix}.source_paths должен быть непустым array")
        for path_index, path in enumerate(paths):
            nonempty_string(path, f"{prefix}.source_paths[{path_index}]")
        for field in ("name", "behavior", "current_avm", "decision"):
            nonempty_string(entry[field], f"{prefix}.{field}")
        issue = entry["target_issue"]
        if not isinstance(issue, int) or issue < 122:
            fail(f"{prefix}.target_issue должен ссылаться на AVM 1.5 issue family")

    candidates = data.get("corpus_candidates")
    if not isinstance(candidates, list) or not candidates:
        fail("manifest.corpus_candidates должен быть непустым array")

    candidate_covers: dict[str, set[str]] = {}
    selected: set[str] = set()
    for index, case in enumerate(candidates):
        prefix = f"manifest.corpus_candidates[{index}]"
        if not isinstance(case, dict):
            fail(f"{prefix} должен быть object")
        case_id = nonempty_string(case.get("id"), f"{prefix}.id")
        if case_id in candidate_covers:
            fail(f"duplicate corpus case id {case_id}")
        status = case.get("status")
        if status not in ALLOWED_CANDIDATE_STATUSES:
            fail(f"{prefix}.status должен быть одним из {sorted(ALLOWED_CANDIDATE_STATUSES)}")
        covers = case.get("covers")
        if not isinstance(covers, list) or not covers:
            fail(f"{prefix}.covers должен быть непустым array")
        unknown = [entry_id for entry_id in covers if entry_id not in entry_ids]
        if unknown:
            fail(f"{prefix}.covers содержит неизвестные IDs: {', '.join(unknown)}")
        candidate_covers[case_id] = set(covers)
        if status == "selected":
            selected.add(case_id)
        nonempty_string(case.get("source"), f"{prefix}.source")
        nonempty_string(case.get("notes"), f"{prefix}.notes")

    return commit, entry_ids, candidate_covers, selected


def validate_details(
    data: dict[str, object], source_commit: str, entry_ids: set[str]
) -> set[str]:
    if data.get("schema_version") != 1:
        fail("jsonrvm-semantics-details.json schema_version должен быть 1")
    if validate_source(data.get("source"), "details.source") != source_commit:
        fail("manifest и operational details должны pin-ить один jsonRVM commit")
    if set(data.get("mutation_kinds") or []) != ALLOWED_MUTATIONS:
        fail("details.mutation_kinds не совпадает со schema v1")
    if set(data.get("fixture_statuses") or []) != ALLOWED_FIXTURE_STATUSES:
        fail("details.fixture_statuses не совпадает со schema v1")

    details = data.get("entries")
    if not isinstance(details, dict):
        fail("details.entries должен быть object, keyed by semantic id")
    detail_ids = set(details)
    if detail_ids != entry_ids:
        missing = sorted(entry_ids - detail_ids)
        extra = sorted(detail_ids - entry_ids)
        fail(f"operational details должны покрывать IDs 1:1; missing={missing}, extra={extra}")

    frozen: set[str] = set()
    for entry_id in sorted(entry_ids):
        detail = details[entry_id]
        prefix = f"details.entries[{entry_id}]"
        if not isinstance(detail, dict):
            fail(f"{prefix} должен быть object")
        if set(detail) != REQUIRED_DETAIL_FIELDS:
            fail(f"{prefix} должен содержать ровно {sorted(REQUIRED_DETAIL_FIELDS)}")
        if detail["mutation"] not in ALLOWED_MUTATIONS:
            fail(f"{prefix}.mutation неизвестен")
        status = detail["fixture_status"]
        if status not in ALLOWED_FIXTURE_STATUSES:
            fail(f"{prefix}.fixture_status неизвестен")
        dependencies = detail["context_dependencies"]
        if not isinstance(dependencies, list):
            fail(f"{prefix}.context_dependencies должен быть array")
        for index, dependency in enumerate(dependencies):
            nonempty_string(dependency, f"{prefix}.context_dependencies[{index}]")
        nonempty_string(detail["failure_contract"], f"{prefix}.failure_contract")
        if status == "frozen":
            frozen.add(entry_id)
    return frozen


def validate_doctest_golden(data: dict[str, object], source_commit: str) -> set[str]:
    if data.get("schema_version") != 1:
        fail("jsonrvm-golden.json schema_version должен быть 1")
    if validate_source(data.get("source"), "doctest.source", require_runtime=False) != source_commit:
        fail("doctest golden и manifest должны pin-ить один jsonRVM commit")
    source = data["source"]
    assert isinstance(source, dict)
    nonempty_string(source.get("assertions_from"), "doctest.source.assertions_from")

    cases = data.get("cases")
    if not isinstance(cases, list) or not cases:
        fail("doctest.cases должен быть непустым array")
    ids: set[str] = set()
    for index, case in enumerate(cases):
        prefix = f"doctest.cases[{index}]"
        if not isinstance(case, dict):
            fail(f"{prefix} должен быть object")
        case_id = nonempty_string(case.get("id"), f"{prefix}.id")
        if case_id in ids:
            fail(f"duplicate doctest golden id {case_id}")
        ids.add(case_id)
        for field in ("fixture", "test_name", "avm_target"):
            nonempty_string(case.get(field), f"{prefix}.{field}")
        if case.get("equivalence") not in ALLOWED_DOCTEST_EQUIVALENCE:
            fail(f"{prefix}.equivalence неизвестен")
        validate_assertions(case.get("assertions"), f"{prefix}.assertions")
    return ids


def validate_oracle_golden(
    data: dict[str, object], source_commit: str, entry_ids: set[str]
) -> tuple[set[str], set[str]]:
    if data.get("schema_version") != 1:
        fail("jsonrvm-oracle-golden.json schema_version должен быть 1")
    if validate_source(data.get("source"), "oracle.source") != source_commit:
        fail("live oracle и manifest должны pin-ить один jsonRVM commit")
    source = data["source"]
    assert isinstance(source, dict)
    oracle_path = pathlib.Path(nonempty_string(source.get("oracle"), "oracle.source.oracle"))
    if not (ROOT / oracle_path).is_file():
        fail(f"oracle runner не найден: {oracle_path}")

    cases = data.get("cases")
    if not isinstance(cases, list) or not cases:
        fail("oracle.cases должен быть непустым array")

    ids: set[str] = set()
    covered: set[str] = set()
    for index, case in enumerate(cases):
        prefix = f"oracle.cases[{index}]"
        if not isinstance(case, dict):
            fail(f"{prefix} должен быть object")
        case_id = nonempty_string(case.get("id"), f"{prefix}.id")
        if case_id in ids:
            fail(f"duplicate oracle golden id {case_id}")
        ids.add(case_id)

        fixture = pathlib.Path(nonempty_string(case.get("fixture"), f"{prefix}.fixture"))
        if not (ROOT / fixture).is_file():
            fail(f"{prefix}.fixture не найден: {fixture}")
        nonempty_string(case.get("avm_target"), f"{prefix}.avm_target")

        covers = case.get("covers")
        if not isinstance(covers, list) or not covers:
            fail(f"{prefix}.covers должен быть непустым array")
        unknown = [entry_id for entry_id in covers if entry_id not in entry_ids]
        if unknown:
            fail(f"{prefix}.covers содержит неизвестные IDs: {', '.join(unknown)}")
        covered.update(covers)

        equivalence = case.get("equivalence")
        if equivalence not in ALLOWED_ORACLE_EQUIVALENCE:
            fail(f"{prefix}.equivalence неизвестен")
        if equivalence == "observable-json-value":
            if "failure" in case:
                fail(f"{prefix}: success-case не должен содержать failure")
            validate_assertions(case.get("assertions"), f"{prefix}.assertions")
        else:
            if "assertions" in case:
                fail(f"{prefix}: failure-case не должен содержать assertions")
            failure = case.get("failure")
            if not isinstance(failure, dict):
                fail(f"{prefix}.failure должен быть object")
            exit_code = failure.get("exit_code")
            if not isinstance(exit_code, int) or exit_code == 0:
                fail(f"{prefix}.failure.exit_code должен быть non-zero integer")
            if failure.get("diagnostic_json") is not True:
                fail(f"{prefix}.failure.diagnostic_json должен быть true")
            nonempty_string(failure.get("contains"), f"{prefix}.failure.contains")

    return ids, covered


def main() -> int:
    manifest = load_json(MANIFEST)
    source_commit, entry_ids, candidate_covers, selected = validate_manifest(manifest)
    frozen = validate_details(load_json(DETAILS), source_commit, entry_ids)
    doctest_ids = validate_doctest_golden(load_json(DOCTEST_GOLDEN), source_commit)
    oracle_ids, oracle_covered = validate_oracle_golden(
        load_json(ORACLE_GOLDEN), source_commit, entry_ids
    )

    evidence_ids = doctest_ids | oracle_ids
    missing_selected = selected - evidence_ids
    if missing_selected:
        fail(f"selected corpus cases без frozen evidence: {', '.join(sorted(missing_selected))}")

    # Старые doctest cases берут semantic coverage из versioned manifest.
    doctest_covered: set[str] = set()
    for case_id in doctest_ids:
        doctest_covered.update(candidate_covers.get(case_id, set()))

    covered = doctest_covered | oracle_covered
    missing_frozen = frozen - covered
    if missing_frozen:
        fail(
            "fixture_status=frozen без semantic evidence: "
            + ", ".join(sorted(missing_frozen))
        )

    print(
        "jsonRVM compatibility OK: "
        f"{len(entry_ids)} semantic entries, {len(frozen)} frozen semantic IDs, "
        f"{len(doctest_ids)} doctest cases, {len(oracle_ids)} live-oracle cases, "
        f"source {source_commit[:12]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
