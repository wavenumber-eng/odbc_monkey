"""
Python API:
    from odbc_monkey import generate_dblib

    generate_dblib(
        json_path="/path/to/json",
        output_path="/path/to/output",
        dblib_name="mylib",
    )

CLI:
    python -m odbc_monkey.dblib_builder --json-path /path/to/json --output /path/to/dblib
"""

from .dblib_builder import (
    generate_dblib,
    scan_json_classifications,
    classification_to_table_name,
    extract_timestamp_from_uuidv7,
    get_latest_version,
)

__all__ = [

    "generate_dblib",
    "scan_json_classifications",
    "classification_to_table_name",
    "extract_timestamp_from_uuidv7",
    "get_latest_version",
]

__version__ = "1.0.0"
