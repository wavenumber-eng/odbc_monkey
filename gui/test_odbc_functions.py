# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
"""
ODBC Function Test Suite
Tests each ODBC function through pyodbc to identify unsupported operations.
"""

import pyodbc
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = REPO_ROOT / "example" / "json"
CONNECTION_STRING = f"DRIVER={{odbc-monkey}};DataSource={JSON_PATH}"

def test_connect():
    """Test basic connection."""
    print("=" * 60)
    print("TEST: Connection")
    print("=" * 60)
    try:
        conn = pyodbc.connect(CONNECTION_STRING)
        print(f"  [OK] Connected successfully")
        print(f"  Connection: {conn}")
        return conn
    except Exception as e:
        print(f"  [FAIL] {e}")
        return None


def test_connection_info(conn):
    """Test SQLGetInfo - connection attributes."""
    print("\n" + "=" * 60)
    print("TEST: Connection Info (SQLGetInfo)")
    print("=" * 60)

    info_types = [
        ("getinfo_driver_name", "Driver Name"),
        ("getinfo_dbms_name", "DBMS Name"),
        ("getinfo_dbms_ver", "DBMS Version"),
        ("getinfo_server_name", "Server Name"),
    ]

    # Test specific getinfo calls
    try:
        print(f"  [OK] Driver Name: {conn.getinfo(pyodbc.SQL_DRIVER_NAME)}")
    except Exception as e:
        print(f"  [FAIL] Driver Name: {e}")

    try:
        print(f"  [OK] DBMS Name: {conn.getinfo(pyodbc.SQL_DBMS_NAME)}")
    except Exception as e:
        print(f"  [FAIL] DBMS Name: {e}")

    try:
        print(f"  [OK] DBMS Ver: {conn.getinfo(pyodbc.SQL_DBMS_VER)}")
    except Exception as e:
        print(f"  [FAIL] DBMS Ver: {e}")


def test_cursor_create(conn):
    """Test cursor creation."""
    print("\n" + "=" * 60)
    print("TEST: Cursor Creation (SQLAllocHandle STMT)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        print(f"  [OK] Cursor created: {cursor}")
        return cursor
    except Exception as e:
        print(f"  [FAIL] {e}")
        return None


def test_tables(cursor):
    """Test SQLTables."""
    print("\n" + "=" * 60)
    print("TEST: List Tables (SQLTables)")
    print("=" * 60)
    try:
        tables = cursor.tables()
        table_list = list(tables)
        print(f"  [OK] Found {len(table_list)} tables")
        for i, t in enumerate(table_list[:5]):
            print(f"       {i+1}. {t.table_name}")
        if len(table_list) > 5:
            print(f"       ... and {len(table_list) - 5} more")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_columns(cursor):
    """Test SQLColumns."""
    print("\n" + "=" * 60)
    print("TEST: List Columns (SQLColumns)")
    print("=" * 60)
    try:
        columns = cursor.columns(table="Parts")
        col_list = list(columns)
        print(f"  [OK] Found {len(col_list)} columns")
        for i, c in enumerate(col_list[:5]):
            print(f"       {i+1}. {c.column_name} ({c.type_name})")
        if len(col_list) > 5:
            print(f"       ... and {len(col_list) - 5} more")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_execute_direct(cursor):
    """Test SQLExecDirect."""
    print("\n" + "=" * 60)
    print("TEST: Execute Direct (SQLExecDirect)")
    print("=" * 60)
    try:
        cursor.execute("SELECT * FROM Parts")
        print(f"  [OK] Query executed")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_num_result_cols(cursor):
    """Test SQLNumResultCols."""
    print("\n" + "=" * 60)
    print("TEST: Get Column Count (SQLNumResultCols)")
    print("=" * 60)
    try:
        col_count = len(cursor.description)
        print(f"  [OK] Column count: {col_count}")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_describe_col(cursor):
    """Test SQLDescribeCol."""
    print("\n" + "=" * 60)
    print("TEST: Describe Columns (SQLDescribeCol)")
    print("=" * 60)
    try:
        for i, col in enumerate(cursor.description[:5]):
            print(f"  [OK] Col {i+1}: name={col[0]}, type={col[1]}, size={col[3]}, nullable={col[6]}")
        if len(cursor.description) > 5:
            print(f"       ... and {len(cursor.description) - 5} more")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_fetch(cursor):
    """Test SQLFetch."""
    print("\n" + "=" * 60)
    print("TEST: Fetch Rows (SQLFetch)")
    print("=" * 60)
    try:
        row = cursor.fetchone()
        if row:
            print(f"  [OK] Fetched row with {len(row)} columns")
            print(f"       First 3 values: {row[0]}, {row[1]}, {row[2]}")
            return True
        else:
            print(f"  [FAIL] No rows returned")
            return False
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_fetchall(cursor):
    """Test fetching all remaining rows."""
    print("\n" + "=" * 60)
    print("TEST: Fetch All (SQLFetch loop)")
    print("=" * 60)
    try:
        rows = cursor.fetchall()
        print(f"  [OK] Fetched {len(rows)} remaining rows")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_fetchmany(conn):
    """Test fetchmany with a fresh query."""
    print("\n" + "=" * 60)
    print("TEST: Fetch Many")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM Parts")
        rows = cursor.fetchmany(10)
        print(f"  [OK] Fetched {len(rows)} rows")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_rowcount(conn):
    """Test rowcount property."""
    print("\n" + "=" * 60)
    print("TEST: Row Count")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM Parts")
        print(f"  [INFO] rowcount = {cursor.rowcount}")
        print(f"  [OK] (rowcount may be -1 for SELECT)")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_column_attributes(conn):
    """Test SQLColAttribute for various attributes."""
    print("\n" + "=" * 60)
    print("TEST: Column Attributes (SQLColAttribute)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM Parts")

        # Check first column's description
        desc = cursor.description[0]
        print(f"  [OK] Column 1 description:")
        print(f"       name: {desc[0]}")
        print(f"       type_code: {desc[1]}")
        print(f"       display_size: {desc[2]}")
        print(f"       internal_size: {desc[3]}")
        print(f"       precision: {desc[4]}")
        print(f"       scale: {desc[5]}")
        print(f"       null_ok: {desc[6]}")

        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_getdata_types(conn):
    """Test SQLGetData with different data retrieval patterns."""
    print("\n" + "=" * 60)
    print("TEST: Get Data Types")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM Parts")
        row = cursor.fetchone()

        for i in range(min(5, len(row))):
            val = row[i]
            print(f"  [OK] Col {i+1}: type={type(val).__name__}, value={str(val)[:30]}")

        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_close_cursor(cursor):
    """Test SQLCloseCursor / SQLFreeStmt."""
    print("\n" + "=" * 60)
    print("TEST: Close Cursor (SQLCloseCursor)")
    print("=" * 60)
    try:
        cursor.close()
        print(f"  [OK] Cursor closed")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_multiple_queries(conn):
    """Test executing multiple queries in sequence."""
    print("\n" + "=" * 60)
    print("TEST: Multiple Queries")
    print("=" * 60)
    try:
        cursor = conn.cursor()

        # Query 1
        cursor.execute("SELECT * FROM Parts")
        row1 = cursor.fetchone()
        print(f"  [OK] Query 1: got row")

        # Query 2 (should close previous result set)
        cursor.execute("SELECT * FROM Parts")
        row2 = cursor.fetchone()
        print(f"  [OK] Query 2: got row")

        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_table_query(conn):
    """Test query with specific table."""
    print("\n" + "=" * 60)
    print("TEST: Query Specific Table")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM [capacitors#Walsin_C0201]")
        rows = cursor.fetchall()
        print(f"  [OK] Query [capacitors#Walsin_C0201]: {len(rows)} rows")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_get_type_info(conn):
    """Test SQLGetTypeInfo."""
    print("\n" + "=" * 60)
    print("TEST: Get Type Info (SQLGetTypeInfo)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        type_info = cursor.getTypeInfo()
        types = list(type_info)
        print(f"  [OK] Found {len(types)} types")
        for t in types[:3]:
            print(f"       {t}")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_statistics(conn):
    """Test SQLStatistics."""
    print("\n" + "=" * 60)
    print("TEST: Statistics (SQLStatistics)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        stats = cursor.statistics("Parts")
        stats_list = list(stats)
        print(f"  [OK] Got {len(stats_list)} statistics entries")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_primary_keys(conn):
    """Test SQLPrimaryKeys."""
    print("\n" + "=" * 60)
    print("TEST: Primary Keys (SQLPrimaryKeys)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        pks = cursor.primaryKeys("Parts")
        pk_list = list(pks)
        print(f"  [OK] Got {len(pk_list)} primary key entries")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_foreign_keys(conn):
    """Test SQLForeignKeys."""
    print("\n" + "=" * 60)
    print("TEST: Foreign Keys (SQLForeignKeys)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        fks = cursor.foreignKeys("Parts")
        fk_list = list(fks)
        print(f"  [OK] Got {len(fk_list)} foreign key entries")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_procedures(conn):
    """Test SQLProcedures."""
    print("\n" + "=" * 60)
    print("TEST: Procedures (SQLProcedures)")
    print("=" * 60)
    try:
        cursor = conn.cursor()
        procs = cursor.procedures()
        proc_list = list(procs)
        print(f"  [OK] Got {len(proc_list)} procedures")
        cursor.close()
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_disconnect(conn):
    """Test SQLDisconnect."""
    print("\n" + "=" * 60)
    print("TEST: Disconnect (SQLDisconnect)")
    print("=" * 60)
    try:
        conn.close()
        print(f"  [OK] Disconnected")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def main():
    print("\n")
    print("*" * 60)
    print("  ODBC DRIVER FUNCTION TEST SUITE")
    print("  Connection: " + CONNECTION_STRING)
    print("*" * 60)

    results = {}

    # Test connection
    conn = test_connect()
    results["connect"] = conn is not None

    if not conn:
        print("\n[FATAL] Could not connect. Aborting tests.")
        return 1

    # Test connection info
    test_connection_info(conn)

    # Test cursor
    cursor = test_cursor_create(conn)
    results["cursor_create"] = cursor is not None

    if cursor:
        # Catalog functions
        results["tables"] = test_tables(cursor)
        results["columns"] = test_columns(cursor)

        # Query execution
        results["execute_direct"] = test_execute_direct(cursor)

        if results["execute_direct"]:
            results["num_result_cols"] = test_num_result_cols(cursor)
            results["describe_col"] = test_describe_col(cursor)
            results["fetch"] = test_fetch(cursor)
            results["fetchall"] = test_fetchall(cursor)

        results["close_cursor"] = test_close_cursor(cursor)

    # Additional tests with fresh cursors
    results["fetchmany"] = test_fetchmany(conn)
    results["rowcount"] = test_rowcount(conn)
    results["column_attributes"] = test_column_attributes(conn)
    results["getdata_types"] = test_getdata_types(conn)
    results["multiple_queries"] = test_multiple_queries(conn)
    results["table_query"] = test_table_query(conn)

    # Optional catalog functions (may not be implemented)
    results["get_type_info"] = test_get_type_info(conn)
    results["statistics"] = test_statistics(conn)
    results["primary_keys"] = test_primary_keys(conn)
    results["foreign_keys"] = test_foreign_keys(conn)
    results["procedures"] = test_procedures(conn)

    # Disconnect
    results["disconnect"] = test_disconnect(conn)

    # Summary
    print("\n")
    print("*" * 60)
    print("  TEST SUMMARY")
    print("*" * 60)

    passed = sum(1 for v in results.values() if v)
    failed = sum(1 for v in results.values() if not v)

    for name, result in results.items():
        status = "[PASS]" if result else "[FAIL]"
        print(f"  {status} {name}")

    print(f"\n  Total: {passed} passed, {failed} failed")
    print("*" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
