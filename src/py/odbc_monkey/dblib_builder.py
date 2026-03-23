#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""
Standalone DbLib Builder for OdbcMonkey

Generates Altium .DbLib files that connect to the OdbcMonkey JSON ODBC driver.
This module has ZERO external dependencies - it uses only Python standard library.

Can be used:
    1. As a standalone CLI tool
    2. As a library imported by other projects

Usage (CLI):
    python dblib_builder.py --json-path /path/to/json --output /path/to/output
    python dblib_builder.py --json-path /path/to/json --output /path/to/output --name mylib

Usage (Library):
    from odbc_monkey.dblib_builder import generate_dblib

    generate_dblib(
        json_path="/path/to/json",
        output_path="/path/to/output",
        dblib_name="mylib",
    )

JSON Format Expected:
    Each .json file should have a "versions" array with objects containing:
    - "id": UUIDv7 string (used to select latest version)
    - "classification": "category/table" format string

    Example:
    {
        "versions": [
            {
                "id": "0199fe35-26d0-706f-6bb6-5d53ea8396d3",
                "classification": "capacitors/Murata_C0402",
                "cad-reference": "CAP_100nF_0402",
                ...
            }
        ]
    }
"""

import argparse
import json
import logging
from collections import defaultdict
from pathlib import Path

log = logging.getLogger(__name__)


# =============================================================================
# UUIDv7 Helpers
# =============================================================================

def extract_timestamp_from_uuidv7(uuid_str: str) -> int:
    """
    Extract timestamp from UUIDv7 (first 48 bits = 12 hex chars).

    UUIDv7 format: xxxxxxxx-xxxx-7xxx-xxxx-xxxxxxxxxxxx
    First 12 hex chars (without hyphens) encode milliseconds since Unix epoch.

    Args:
        uuid_str: UUIDv7 string (with or without hyphens)

    Returns:
        Timestamp as integer (milliseconds since epoch), or 0 on error
    """
    try:
        hex_str = uuid_str.replace("-", "")
        if len(hex_str) >= 12:
            return int(hex_str[:12], 16)
    except (ValueError, TypeError):
        pass
    return 0


def get_latest_version(versions: list[dict]) -> dict | None:
    """
    Get the latest version from a versions array based on UUIDv7 timestamp.

    Args:
        versions: List of version dicts, each with an 'id' field containing UUIDv7

    Returns:
        The version dict with the highest timestamp, or None if empty
    """
    if not versions:
        return None
    if len(versions) == 1:
        return versions[0]
    return max(versions, key=lambda v: extract_timestamp_from_uuidv7(v.get('id', '')))


# =============================================================================
# Classification Helpers
# =============================================================================

def classification_to_table_name(classification: str, separator: str = "#") -> str:
    """
    Convert classification format to ODBC table name format.

    Replaces "/" with separator (default "#") to match OdbcMonkey's table naming.
    Altium doesn't handle "/" in table names, so we use "#" instead.

    Args:
        classification: Classification string like "capacitors/Murata_C0402"
        separator: Character to replace "/" with (default: "#")

    Returns:
        Table name like "capacitors#Murata_C0402"
    """
    return classification.replace("/", separator)


def scan_json_classifications(
    json_path: Path,
    classification_field: str = "classification",
) -> dict[str, list[str]]:
    """
    Scan JSON files and extract unique classifications grouped by category.

    Uses UUIDv7-based latest version selection to ensure consistency
    with the OdbcMonkey ODBC driver.

    Args:
        json_path: Path to directory containing JSON files
        classification_field: Field name containing classification (default: "classification")

    Returns:
        Dictionary mapping category names to sorted lists of table names
        e.g., {'capacitors': ['Murata_0201', 'Walsin_C0201'], 'resistors': ['0402', '0603']}
    """
    category_tables = defaultdict(set)
    json_path = Path(json_path)

    json_files = list(json_path.glob("*.json"))
    log.info(f"Scanning {len(json_files)} JSON files for classifications...")

    for json_file in json_files:
        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            versions = data.get('versions', [])
            latest = get_latest_version(versions)

            if latest:
                classification = latest.get(classification_field, '')

                if classification and '/' in classification:
                    parts = classification.split('/', 1)
                    category = parts[0].strip()
                    table = parts[1].strip()

                    if category and table:
                        category_tables[category].add(table)

        except (json.JSONDecodeError, KeyError, TypeError, OSError) as e:
            log.warning(f"Error reading {json_file}: {e}")
            continue

    # Convert sets to sorted lists
    result = {cat: sorted(tables) for cat, tables in sorted(category_tables.items())}
    log.info(f"Found {len(result)} categories with {sum(len(t) for t in result.values())} tables")

    return result


# =============================================================================
# DbLib Generator
# =============================================================================

def generate_dblib(
    json_path: Path | str,
    output_path: Path | str,
    *,
    dblib_name: str = "database",
    primary_key: str = "cad-reference",
    driver_name: str = "odbc-monkey",
    classification_field: str = "classification",
) -> Path | None:
    """
    Generate an Altium .DbLib file for the OdbcMonkey JSON ODBC driver.

    Creates a single .DbLib file containing ALL classifications as tables.
    Each table appears as a separate entry in Altium's Components panel dropdown.

    Uses a direct DRIVER= connection string so no System DSN setup is required.

    Args:
        json_path: Path to directory containing JSON files
        output_path: Directory to write .DbLib file
        dblib_name: Name of the DbLib file (without extension)
        primary_key: Column name used for single-key lookup in Altium
        driver_name: ODBC driver name (default: "odbc-monkey")
        classification_field: JSON field containing classification (default: "classification")

    Returns:
        Path to generated .DbLib file, or None on error
    """
    json_path = Path(json_path)
    output_path = Path(output_path)

    # Validate inputs
    if not json_path.exists():
        log.error(f"JSON path does not exist: {json_path}")
        return None

    if not json_path.is_dir():
        log.error(f"JSON path is not a directory: {json_path}")
        return None

    # Create output directory
    output_path.mkdir(parents=True, exist_ok=True)

    # Build connection string using driver directly (no DSN required)
    connection_string = f'DRIVER={{{driver_name}}};DataSource={json_path}'

    # Scan JSON files to get classifications
    category_tables = scan_json_classifications(json_path, classification_field)

    if not category_tables:
        log.warning("No classifications found in JSON files")
        return None

    # Build flat list of all tables (category#table format)
    all_tables = []
    for category in sorted(category_tables.keys()):
        for table in sorted(category_tables[category]):
            all_tables.append(classification_to_table_name(f"{category}/{table}"))

    # Sanitize dblib_name to prevent path traversal
    safe_name = Path(dblib_name).name
    if not safe_name or safe_name != dblib_name:
        log.error(f"Invalid dblib_name (path traversal): {dblib_name!r}")
        return None

    dblib_path = output_path / f"{safe_name}.DbLib"

    # Build DbLib content (INI-like format)
    dblib_content = [
        '[OutputDatabaseLinkFile]',
        'Version=1.1',
        '[DatabaseLinks]',
        f'ConnectionString={connection_string}',
        'AddMode=3',
        'RemoveMode=1',
        'UpdateMode=2',
        'ViewMode=0',
        'LeftQuote=[',
        'RightQuote=]',
        'QuoteTableNames=1',
        'UseTableSchemaName=0',
        'DefaultColumnType=NVARCHAR(255)',
        'LibraryDatabaseType=',
        'LibraryDatabasePath=',
        'DatabasePathRelative=0',
        'TopPanelCollapsed=0',
        'LibrarySearchPath=',
        'OrcadMultiValueDelimiter=,',
        'SearchSubDirectories=0',
        'SchemaName=',
        f'LastFocusedTable={all_tables[0]}',
        ''
    ]

    # Add "Parts" mega-table (merged view of all tables)
    # This must have a [TableN] entry so Altium can resolve the UserWhereText
    # for placement lookups, not just browsing.
    dblib_content.extend([
        '[Table1]',
        'SchemaName=',
        'TableName=Parts',
        'Enabled=True',
        'UserWhere=1',
        f"UserWhereText=[{primary_key}] = '{{{primary_key}}}'",
        ''
    ])

    # Add individual classification tables
    for idx, table in enumerate(all_tables, 2):
        dblib_content.extend([
            f'[Table{idx}]',
            'SchemaName=',
            f'TableName={table}',
            'Enabled=True',
            'UserWhere=1',
            f"UserWhereText=[{primary_key}] = '{{{primary_key}}}'",
            ''
        ])

    # Write with Windows line endings (required by Altium)
    with open(dblib_path, 'w', newline='\r\n') as f:
        f.write('\n'.join(dblib_content))

    log.info(f"Generated {dblib_path} ({len(all_tables)} tables + Parts mega-table)")
    return dblib_path


# =============================================================================
# CLI Interface
# =============================================================================

def main():
    """Command-line interface for DbLib generation."""
    parser = argparse.ArgumentParser(
        description='Generate Altium .DbLib file for OdbcMonkey JSON driver',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Basic usage
    python dblib_builder.py --json-path C:/libs/json --output C:/libs/dblib

    # With custom name and key
    python dblib_builder.py --json-path C:/libs/json --output C:/libs/dblib \\
        --name mycomponents --primary-key "Part Number"

    # Verbose output
    python dblib_builder.py --json-path C:/libs/json --output C:/libs/dblib -v
"""
    )

    parser.add_argument(
        '--json-path', '-j',
        type=Path,
        required=True,
        help='Path to directory containing JSON files'
    )

    parser.add_argument(
        '--output', '-o',
        type=Path,
        required=True,
        help='Output directory for .DbLib file'
    )

    parser.add_argument(
        '--name', '-n',
        type=str,
        default='database',
        help='DbLib filename (without extension, default: database)'
    )

    parser.add_argument(
        '--primary-key', '-k',
        type=str,
        default='cad-reference',
        help='Primary key column for Altium lookup (default: cad-reference)'
    )

    parser.add_argument(
        '--driver', '-d',
        type=str,
        default='odbc-monkey',
        help='ODBC driver name (default: odbc-monkey)'
    )

    parser.add_argument(
        '--classification-field', '-c',
        type=str,
        default='classification',
        help='JSON field containing classification (default: classification)'
    )

    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )

    args = parser.parse_args()

    # Setup logging
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(levelname)s: %(message)s'
    )

    # Generate DbLib
    result = generate_dblib(
        json_path=args.json_path,
        output_path=args.output,
        dblib_name=args.name,
        primary_key=args.primary_key,
        driver_name=args.driver,
        classification_field=args.classification_field,
    )

    if result:
        print(f"Generated: {result}")
        return 0
    else:
        print("Failed to generate DbLib file")
        return 1


if __name__ == '__main__':
    exit(main())
