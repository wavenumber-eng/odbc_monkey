#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""
Validate the installed odbc_monkey driver against the repo example data.

Usage:
    python example/scripts/validate_example.py
"""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

import pyodbc

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from dblib_builder import classification_to_table_name, get_latest_version

EXAMPLE_DIR = REPO_ROOT / "example"
JSON_DIR = EXAMPLE_DIR / "json"
DRIVER_NAME = "odbc-monkey"


def connection_string() -> str:
    return f"DRIVER={{{DRIVER_NAME}}};DataSource={JSON_DIR}"


def expect(condition: bool, message: str):
    if not condition:
        raise AssertionError(message)
    print(f"[PASS] {message}")


def load_expected_rows() -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    json_files = sorted(JSON_DIR.glob("*.json"))
    expect(bool(json_files), f"found example JSON files in {JSON_DIR}")

    for json_file in json_files:
        with open(json_file, "r", encoding="utf-8") as handle:
            data = json.load(handle)

        latest = get_latest_version(data.get("versions", []))
        expect(latest is not None, f"{json_file.name} has at least one version")

        classification = latest.get("classification", "")
        cad_reference = latest.get("cad-reference", "")
        value = latest.get("value", "")

        expect(bool(classification), f"{json_file.name} has classification")
        expect(bool(cad_reference), f"{json_file.name} has cad-reference")

        rows.append(
            {
                "file_name": json_file.name,
                "table_name": classification_to_table_name(classification),
                "cad-reference": cad_reference,
                "value": value,
            }
        )

    return rows


def fetch_rows(cursor: pyodbc.Cursor, query: str) -> list[dict[str, object]]:
    cursor.execute(query)
    columns = [column[0] for column in cursor.description]
    return [dict(zip(columns, row)) for row in cursor.fetchall()]


def get_field(row: dict[str, object], field_name: str) -> object:
    field_name = field_name.casefold()
    for key, value in row.items():
        if key.casefold() == field_name:
            return value
    raise KeyError(field_name)


def validate_tables(cursor: pyodbc.Cursor, expected_rows: list[dict[str, str]]):
    expected_tables = {"Parts", *(row["table_name"] for row in expected_rows)}
    actual_tables = {row.table_name for row in cursor.tables()}
    expect(actual_tables == expected_tables, f"table set matches example data: {sorted(expected_tables)}")


def validate_parts_table(cursor: pyodbc.Cursor, expected_rows: list[dict[str, str]]):
    parts_rows = fetch_rows(cursor, "SELECT * FROM Parts")
    expect(len(parts_rows) == len(expected_rows), f"Parts query returned {len(expected_rows)} row(s)")

    by_reference = {str(row["cad-reference"]): row for row in parts_rows}
    for expected in expected_rows:
        actual = by_reference.get(expected["cad-reference"])
        expect(actual is not None, f"Parts contains {expected['cad-reference']}")
        expect(str(get_field(actual, "value")) == expected["value"], f"Parts value matches for {expected['cad-reference']}")


def validate_classification_tables(cursor: pyodbc.Cursor, expected_rows: list[dict[str, str]]):
    expected_counts = Counter(row["table_name"] for row in expected_rows)

    for table_name, expected_count in sorted(expected_counts.items()):
        rows = fetch_rows(cursor, f"SELECT * FROM [{table_name}]")
        expect(len(rows) == expected_count, f"{table_name} returned {expected_count} row(s)")

        expected_refs = sorted(row["cad-reference"] for row in expected_rows if row["table_name"] == table_name)
        actual_refs = sorted(str(row["cad-reference"]) for row in rows)
        expect(actual_refs == expected_refs, f"{table_name} cad-reference values match")


def validate_where_queries(cursor: pyodbc.Cursor, expected_rows: list[dict[str, str]]):
    first_reference = expected_rows[0]["cad-reference"]
    rows = fetch_rows(cursor, f"SELECT * FROM Parts WHERE [cad-reference] = '{first_reference}'")
    expect(len(rows) == 1, "equality WHERE query returned exactly one row")
    expect(str(rows[0]["cad-reference"]) == first_reference, "equality WHERE query returned the expected part")

    rows = fetch_rows(cursor, "SELECT * FROM Parts WHERE [cad-reference] LIKE 'EXAMPLE_%'")
    expect(len(rows) == len(expected_rows), "LIKE WHERE query returned the expected rows")


def main() -> int:
    expected_rows = load_expected_rows()
    conn = None

    print(f"Connecting with: {connection_string()}")

    try:
        conn = pyodbc.connect(connection_string())
        print("[PASS] connected to odbc_monkey")

        cursor = conn.cursor()
        validate_tables(cursor, expected_rows)
        validate_parts_table(cursor, expected_rows)
        validate_classification_tables(cursor, expected_rows)
        validate_where_queries(cursor, expected_rows)
        cursor.close()

        print("Validation succeeded")
        return 0
    except Exception as exc:
        print(f"[FAIL] {exc}")
        return 1
    finally:
        if conn is not None:
            conn.close()


if __name__ == "__main__":
    raise SystemExit(main())
