// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// Comprehensive ODBC driver test suite for odbc-monkey
// Tests all ODBC functions used by Altium Designer

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>

// Test configuration
const wchar_t* DRIVER_NAME = L"odbc-monkey";

// Paths derived at runtime from executable location
// Executable is at: .../native/cmake-build/Release/odbc_test.exe
// Test data:        .../native/test/test_data
std::wstring g_testDataDir;
std::wstring g_fileWatcherTestDir;

void initPaths()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Walk up from exe location to native/
    // exe is at: .../native/cmake-build/Release/odbc_test.exe
    std::wstring path(exePath);
    for (int i = 0; i < 3; ++i)
    { // Go up 3 levels: Release -> cmake-build -> native
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            path = path.substr(0, pos);
    }
    std::wstring nativeDir = path;

    g_testDataDir = nativeDir + L"\\test\\test_data";
    g_fileWatcherTestDir = nativeDir + L"\\test\\file_watcher_data";
}

// Test result tracking
int g_testsRun = 0;
int g_testsPassed = 0;
int g_testsFailed = 0;

#define TEST_START(name) \
    printf("\n=== TEST: %s ===\n", name); \
    g_testsRun++;

#define TEST_PASS() \
    printf("  PASS\n"); \
    g_testsPassed++;

#define TEST_FAIL(msg) \
    printf("  FAIL: %s\n", msg); \
    g_testsFailed++;

#define ASSERT_SUCCESS(ret, msg) \
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) { \
        printf("  ERROR: %s (ret=%d)\n", msg, ret); \
        printDiag(SQL_HANDLE_STMT, hStmt); \
        TEST_FAIL(msg); \
        return false; \
    }

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        TEST_FAIL(msg); \
        return false; \
    }

// Helper to print ODBC diagnostics
void printDiag(SQLSMALLINT handleType, SQLHANDLE handle) {
    SQLWCHAR sqlState[6];
    SQLINTEGER nativeError;
    SQLWCHAR msgText[1024];
    SQLSMALLINT msgLen;
    SQLSMALLINT i = 1;

    while (SQLGetDiagRecW(handleType, handle, i, sqlState, &nativeError,
                          msgText, sizeof(msgText)/sizeof(SQLWCHAR), &msgLen) == SQL_SUCCESS) {
        wprintf(L"    [%s] %s (native=%d)\n", sqlState, msgText, nativeError);
        i++;
    }
}

// Helper to connect
bool connect(SQLHENV& hEnv, SQLHDBC& hDbc, SQLHSTMT& hStmt) {
    SQLRETURN ret;

    // Allocate environment
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (ret != SQL_SUCCESS) {
        printf("  Failed to allocate ENV handle\n");
        return false;
    }

    // Set ODBC version
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (ret != SQL_SUCCESS) {
        printf("  Failed to set ODBC version\n");
        return false;
    }

    // Allocate connection
    ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    if (ret != SQL_SUCCESS) {
        printf("  Failed to allocate DBC handle\n");
        return false;
    }

    // Build connection string
    std::wstring connStr = L"DRIVER={";
    connStr += DRIVER_NAME;
    connStr += L"};DataSource=";
    connStr += g_testDataDir;

    SQLWCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen;

    // Connect
    ret = SQLDriverConnectW(hDbc, NULL, (SQLWCHAR*)connStr.c_str(), SQL_NTS,
                            outConnStr, sizeof(outConnStr)/sizeof(SQLWCHAR),
                            &outConnStrLen, SQL_DRIVER_NOPROMPT);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        printf("  Failed to connect\n");
        printDiag(SQL_HANDLE_DBC, hDbc);
        return false;
    }

    // Allocate statement
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
    if (ret != SQL_SUCCESS) {
        printf("  Failed to allocate STMT handle\n");
        return false;
    }

    return true;
}

// Helper to disconnect
void disconnect(SQLHENV hEnv, SQLHDBC hDbc, SQLHSTMT hStmt) {
    if (hStmt) SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    if (hDbc) {
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    }
    if (hEnv) SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
}

// Helper to connect to a specific data directory for file watcher tests
bool connectToDataDir(SQLHENV& hEnv, SQLHDBC& hDbc, SQLHSTMT& hStmt, const wchar_t* dataDir) {
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (ret != SQL_SUCCESS) return false;

    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (ret != SQL_SUCCESS) return false;

    ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    if (ret != SQL_SUCCESS) return false;

    std::wstring connStr = L"DRIVER={";
    connStr += DRIVER_NAME;
    connStr += L"};DataSource=";
    connStr += dataDir;

    SQLWCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen;

    ret = SQLDriverConnectW(hDbc, NULL, (SQLWCHAR*)connStr.c_str(), SQL_NTS,
                            outConnStr, sizeof(outConnStr)/sizeof(SQLWCHAR),
                            &outConnStrLen, SQL_DRIVER_NOPROMPT);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        printf("  Failed to connect\n");
        printDiag(SQL_HANDLE_DBC, hDbc);
        return false;
    }

    ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
    if (ret != SQL_SUCCESS) return false;

    return true;
}

// Helper to write a JSON file for file watcher tests
bool writeJsonFile(const std::wstring& path, const char* content) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return false;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return true;
}

// Helper to delete a file
bool deleteFile(const std::wstring& path) {
    return DeleteFileW(path.c_str()) != 0;
}

// Helper to create directory if not exists
bool ensureDirectory(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;  // Already exists
    }
    return CreateDirectoryW(path.c_str(), NULL) != 0;
}

//=============================================================================
// CONNECTION TESTS
//=============================================================================

bool test_connection_basic() {
    TEST_START("Connection - Basic Connect/Disconnect");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;

    bool result = connect(hEnv, hDbc, hStmt);
    disconnect(hEnv, hDbc, hStmt);

    if (result) {
        TEST_PASS();
    } else {
        TEST_FAIL("Connection failed");
    }
    return result;
}

bool test_connection_multiple() {
    TEST_START("Connection - Multiple Connections");

    SQLHENV hEnv1 = NULL, hEnv2 = NULL;
    SQLHDBC hDbc1 = NULL, hDbc2 = NULL;
    SQLHSTMT hStmt1 = NULL, hStmt2 = NULL;

    bool result1 = connect(hEnv1, hDbc1, hStmt1);
    bool result2 = connect(hEnv2, hDbc2, hStmt2);

    disconnect(hEnv1, hDbc1, hStmt1);
    disconnect(hEnv2, hDbc2, hStmt2);

    if (result1 && result2) {
        TEST_PASS();
        return true;
    } else {
        TEST_FAIL("Multiple connections failed");
        return false;
    }
}

//=============================================================================
// ATTRIBUTE TESTS
//=============================================================================

bool test_env_attributes() {
    TEST_START("Attributes - SQLSetEnvAttr/SQLGetEnvAttr");

    SQLHENV hEnv = NULL;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    ASSERT_TRUE(ret == SQL_SUCCESS, "Allocate ENV failed");

    // Set ODBC version
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    ASSERT_TRUE(ret == SQL_SUCCESS, "SetEnvAttr ODBC_VERSION failed");

    // Get ODBC version back
    SQLINTEGER version;
    ret = SQLGetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, &version, sizeof(version), NULL);
    ASSERT_TRUE(ret == SQL_SUCCESS, "GetEnvAttr ODBC_VERSION failed");
    ASSERT_TRUE(version == SQL_OV_ODBC3, "Wrong ODBC version returned");

    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    TEST_PASS();
    return true;
}

bool test_stmt_attributes() {
    TEST_START("Attributes - SQLSetStmtAttr/SQLGetStmtAttr");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Test SQL_ATTR_QUERY_TIMEOUT
    ret = SQLSetStmtAttr(hStmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);
    printf("  SQLSetStmtAttr(QUERY_TIMEOUT=30): ret=%d\n", ret);

    SQLULEN timeout = 0;
    ret = SQLGetStmtAttr(hStmt, SQL_ATTR_QUERY_TIMEOUT, &timeout, sizeof(timeout), NULL);
    printf("  SQLGetStmtAttr(QUERY_TIMEOUT): ret=%d, value=%llu\n", ret, (unsigned long long)timeout);

    // Test SQL_ATTR_MAX_ROWS
    ret = SQLSetStmtAttr(hStmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)100, 0);
    printf("  SQLSetStmtAttr(MAX_ROWS=100): ret=%d\n", ret);

    // Test SQL_ATTR_ROW_ARRAY_SIZE
    ret = SQLSetStmtAttr(hStmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)10, 0);
    printf("  SQLSetStmtAttr(ROW_ARRAY_SIZE=10): ret=%d\n", ret);

    // Test descriptor attributes (critical for ODBC 3.x)
    SQLHANDLE descHandle = NULL;
    ret = SQLGetStmtAttr(hStmt, SQL_ATTR_APP_ROW_DESC, &descHandle, sizeof(descHandle), NULL);
    printf("  SQLGetStmtAttr(APP_ROW_DESC): ret=%d, handle=%p\n", ret, descHandle);
    ASSERT_TRUE(ret == SQL_SUCCESS, "GetStmtAttr APP_ROW_DESC failed");
    ASSERT_TRUE(descHandle != NULL, "APP_ROW_DESC handle is NULL");

    ret = SQLGetStmtAttr(hStmt, SQL_ATTR_IMP_ROW_DESC, &descHandle, sizeof(descHandle), NULL);
    printf("  SQLGetStmtAttr(IMP_ROW_DESC): ret=%d, handle=%p\n", ret, descHandle);
    ASSERT_TRUE(ret == SQL_SUCCESS, "GetStmtAttr IMP_ROW_DESC failed");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// METADATA TESTS
//=============================================================================

bool test_sql_tables() {
    TEST_START("Metadata - SQLTables");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLTables failed");

    int tableCount = 0;
    SQLWCHAR tableName[256];
    SQLLEN tableNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
        wprintf(L"  Table: %s\n", tableName);
        tableCount++;
    }

    printf("  Found %d tables\n", tableCount);
    ASSERT_TRUE(tableCount > 0, "No tables found");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_columns() {
    TEST_START("Metadata - SQLColumns");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLColumnsW(hStmt, NULL, 0, NULL, 0, (SQLWCHAR*)L"Parts", SQL_NTS, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLColumns failed");

    int colCount = 0;
    SQLWCHAR colName[256];
    SQLLEN colNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 4, SQL_C_WCHAR, colName, sizeof(colName), &colNameLen);
        if (colCount < 10) {
            wprintf(L"  Column: %s\n", colName);
        }
        colCount++;
    }

    printf("  Found %d columns\n", colCount);
    ASSERT_TRUE(colCount > 0, "No columns found");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_gettypeinfo() {
    TEST_START("Metadata - SQLGetTypeInfo");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLGetTypeInfoW(hStmt, SQL_ALL_TYPES);
    printf("  SQLGetTypeInfo ret=%d\n", ret);

    // Even if no types returned, function should succeed
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        printDiag(SQL_HANDLE_STMT, hStmt);
    }

    int typeCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        typeCount++;
    }
    printf("  Found %d types\n", typeCount);

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_describecol() {
    TEST_START("Metadata - SQLDescribeCol");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    SQLSMALLINT numCols;
    ret = SQLNumResultCols(hStmt, &numCols);
    ASSERT_SUCCESS(ret, "NumResultCols failed");
    printf("  Number of columns: %d\n", numCols);

    for (SQLSMALLINT i = 1; i <= numCols && i <= 5; i++) {
        SQLWCHAR colName[256];
        SQLSMALLINT colNameLen;
        SQLSMALLINT dataType;
        SQLULEN colSize;
        SQLSMALLINT decDigits;
        SQLSMALLINT nullable;

        ret = SQLDescribeColW(hStmt, i, colName, sizeof(colName)/sizeof(SQLWCHAR),
                              &colNameLen, &dataType, &colSize, &decDigits, &nullable);
        if (ret == SQL_SUCCESS) {
            wprintf(L"  Col %d: %s (type=%d, size=%llu)\n", i, colName, dataType, (unsigned long long)colSize);
        }
    }

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// QUERY TESTS
//=============================================================================

bool test_sql_execdirect() {
    TEST_START("Query - SQLExecDirect");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        rowCount++;
        if (rowCount >= 100) break; // Limit for test
    }

    printf("  Fetched %d rows\n", rowCount);
    ASSERT_TRUE(rowCount > 0, "No rows fetched");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_prepare_execute() {
    TEST_START("Query - SQLPrepare/SQLExecute");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLPrepareW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "Prepare failed");

    ret = SQLExecute(hStmt);
    ASSERT_SUCCESS(ret, "Execute failed");

    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        rowCount++;
        if (rowCount >= 10) break;
    }

    printf("  Fetched %d rows\n", rowCount);
    ASSERT_TRUE(rowCount > 0, "No rows fetched");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_getdata() {
    TEST_START("Query - SQLGetData");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    ret = SQLFetch(hStmt);
    ASSERT_SUCCESS(ret, "First fetch failed");

    // Get first column data
    SQLWCHAR buffer[1024];
    SQLLEN indicator;

    ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        wprintf(L"  Column 1 value: %s\n", buffer);
    }

    // Get multiple columns
    SQLSMALLINT numCols;
    SQLNumResultCols(hStmt, &numCols);

    printf("  Reading first 5 columns from first row:\n");
    for (SQLSMALLINT i = 1; i <= numCols && i <= 5; i++) {
        ret = SQLGetData(hStmt, i, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            if (indicator == SQL_NULL_DATA) {
                printf("    Col %d: NULL\n", i);
            } else {
                wprintf(L"    Col %d: %s\n", i, buffer);
            }
        }
    }

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_where_clause() {
    TEST_START("Query - WHERE clause");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Test LIKE with wildcard
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts WHERE [Description] LIKE '%CAP%'", SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        printf("  WHERE clause query returned: %d (may be expected if no matches)\n", ret);
    }

    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        rowCount++;
        if (rowCount >= 10) break;
    }

    printf("  Fetched %d rows matching WHERE clause\n", rowCount);

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// CURSOR TESTS
//=============================================================================

bool test_sql_numresultcols() {
    TEST_START("Cursor - SQLNumResultCols");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    SQLSMALLINT numCols;
    ret = SQLNumResultCols(hStmt, &numCols);
    ASSERT_SUCCESS(ret, "NumResultCols failed");

    printf("  Number of columns: %d\n", numCols);
    ASSERT_TRUE(numCols > 0, "Column count is 0");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_rowcount() {
    TEST_START("Cursor - SQLRowCount");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    SQLLEN rowCount;
    ret = SQLRowCount(hStmt, &rowCount);
    printf("  SQLRowCount ret=%d, count=%lld\n", ret, (long long)rowCount);

    // For SELECT, row count may be -1 or 0 until fetched
    // This is expected behavior

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_closecursor() {
    TEST_START("Cursor - SQLCloseCursor/SQLFreeStmt");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // First query
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "First ExecDirect failed");

    ret = SQLFetch(hStmt);
    printf("  First fetch: ret=%d\n", ret);

    // Close cursor
    ret = SQLCloseCursor(hStmt);
    printf("  SQLCloseCursor: ret=%d\n", ret);

    // Alternative: SQLFreeStmt with SQL_CLOSE
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "Second ExecDirect failed");

    ret = SQLFreeStmt(hStmt, SQL_CLOSE);
    printf("  SQLFreeStmt(SQL_CLOSE): ret=%d\n", ret);

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// INFO TESTS
//=============================================================================

bool test_sql_getinfo() {
    TEST_START("Info - SQLGetInfo");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    SQLWCHAR buffer[256];
    SQLSMALLINT bufLen;

    // Driver name
    ret = SQLGetInfoW(hDbc, SQL_DRIVER_NAME, buffer, sizeof(buffer), &bufLen);
    if (ret == SQL_SUCCESS) {
        wprintf(L"  SQL_DRIVER_NAME: %s\n", buffer);
    }

    // Driver version
    ret = SQLGetInfoW(hDbc, SQL_DRIVER_VER, buffer, sizeof(buffer), &bufLen);
    if (ret == SQL_SUCCESS) {
        wprintf(L"  SQL_DRIVER_VER: %s\n", buffer);
    }

    // ODBC version
    ret = SQLGetInfoW(hDbc, SQL_DRIVER_ODBC_VER, buffer, sizeof(buffer), &bufLen);
    if (ret == SQL_SUCCESS) {
        wprintf(L"  SQL_DRIVER_ODBC_VER: %s\n", buffer);
    }

    // DBMS name
    ret = SQLGetInfoW(hDbc, SQL_DBMS_NAME, buffer, sizeof(buffer), &bufLen);
    if (ret == SQL_SUCCESS) {
        wprintf(L"  SQL_DBMS_NAME: %s\n", buffer);
    }

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_getfunctions() {
    TEST_START("Info - SQLGetFunctions");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Check specific functions
    SQLUSMALLINT supported;

    ret = SQLGetFunctions(hDbc, SQL_API_SQLTABLES, &supported);
    printf("  SQLTables: %s\n", supported ? "supported" : "NOT supported");

    ret = SQLGetFunctions(hDbc, SQL_API_SQLCOLUMNS, &supported);
    printf("  SQLColumns: %s\n", supported ? "supported" : "NOT supported");

    ret = SQLGetFunctions(hDbc, SQL_API_SQLGETTYPEINFO, &supported);
    printf("  SQLGetTypeInfo: %s\n", supported ? "supported" : "NOT supported");

    ret = SQLGetFunctions(hDbc, SQL_API_SQLEXECDIRECT, &supported);
    printf("  SQLExecDirect: %s\n", supported ? "supported" : "NOT supported");

    ret = SQLGetFunctions(hDbc, SQL_API_SQLFETCH, &supported);
    printf("  SQLFetch: %s\n", supported ? "supported" : "NOT supported");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// CLASSIFICATION TABLE TESTS
//=============================================================================

bool test_classification_tables() {
    TEST_START("Classification - Tables use category#table format");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLTables failed");

    int totalTables = 0;
    int classificationTables = 0;  // Tables with : (classification format)
    std::vector<std::wstring> sampleTables;

    SQLWCHAR tableName[256];
    SQLLEN tableNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
        totalTables++;

        // Check if table name contains ':' (classification format: category#table)
        std::wstring name(tableName);
        if (name.find(L'#') != std::wstring::npos) {
            classificationTables++;
            if (sampleTables.size() < 5) {
                sampleTables.push_back(name);
            }
        }
    }

    printf("  Total tables: %d\n", totalTables);
    printf("  Classification tables (with #): %d\n", classificationTables);
    printf("  Sample classification tables:\n");
    for (const auto& t : sampleTables) {
        wprintf(L"    - %s\n", t.c_str());
    }

    // Expect majority of tables to be classification-based
    ASSERT_TRUE(classificationTables > 0, "No classification tables found (tables should use category#table format)");
    ASSERT_TRUE(classificationTables >= totalTables / 2, "Less than half of tables use classification format");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_classification_query() {
    TEST_START("Classification - Query specific category#table");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // First, get a classification table name to query
    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLTables failed");

    std::wstring classificationTable;
    SQLWCHAR tableName[256];
    SQLLEN tableNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
        std::wstring name(tableName);
        if (name.find(L'#') != std::wstring::npos && name != L"Parts") {
            classificationTable = name;
            break;
        }
    }

    if (classificationTable.empty()) {
        TEST_FAIL("No classification table found to test");
        disconnect(hEnv, hDbc, hStmt);
        return false;
    }

    wprintf(L"  Testing table: %s\n", classificationTable.c_str());

    // Close the tables result set
    SQLCloseCursor(hStmt);

    // Query the classification table using bracket notation
    std::wstring sql = L"SELECT * FROM [" + classificationTable + L"]";
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)sql.c_str(), SQL_NTS);
    ASSERT_SUCCESS(ret, "Query to classification table failed");

    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        rowCount++;
        if (rowCount >= 10) break;  // Limit for test
    }

    printf("  Fetched %d rows from classification table\n", rowCount);
    ASSERT_TRUE(rowCount > 0, "No rows found in classification table");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_classification_columns() {
    TEST_START("Classification - SQLColumns on category#table");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // First, get a classification table name
    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLTables failed");

    std::wstring classificationTable;
    SQLWCHAR tableName[256];
    SQLLEN tableNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
        std::wstring name(tableName);
        if (name.find(L'#') != std::wstring::npos && name != L"Parts") {
            classificationTable = name;
            break;
        }
    }

    if (classificationTable.empty()) {
        TEST_FAIL("No classification table found to test");
        disconnect(hEnv, hDbc, hStmt);
        return false;
    }

    wprintf(L"  Testing columns for: %s\n", classificationTable.c_str());

    // Close the tables result set
    SQLCloseCursor(hStmt);

    // Get columns for the classification table
    ret = SQLColumnsW(hStmt, NULL, 0, NULL, 0, (SQLWCHAR*)classificationTable.c_str(), SQL_NTS, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLColumns failed for classification table");

    int colCount = 0;
    SQLWCHAR colName[256];
    SQLLEN colNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 4, SQL_C_WCHAR, colName, sizeof(colName), &colNameLen);
        if (colCount < 5) {
            wprintf(L"    Column: %s\n", colName);
        }
        colCount++;
    }

    printf("  Found %d columns\n", colCount);
    ASSERT_TRUE(colCount > 0, "No columns found for classification table");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_classification_part_lookup() {
    TEST_START("Classification - Lookup part from specific table");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // First, get a classification table and find a part in it
    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_SUCCESS(ret, "SQLTables failed");

    std::wstring classificationTable;
    SQLWCHAR tableName[256];
    SQLLEN tableNameLen;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
        std::wstring name(tableName);
        if (name.find(L'#') != std::wstring::npos && name != L"Parts") {
            classificationTable = name;
            break;
        }
    }

    if (classificationTable.empty()) {
        TEST_FAIL("No classification table found");
        disconnect(hEnv, hDbc, hStmt);
        return false;
    }

    SQLCloseCursor(hStmt);

    // Get a part number from that table
    std::wstring sql = L"SELECT * FROM [" + classificationTable + L"]";
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)sql.c_str(), SQL_NTS);
    ASSERT_SUCCESS(ret, "Query failed");

    std::wstring mpn;
    ret = SQLFetch(hStmt);
    if (ret == SQL_SUCCESS) {
        SQLWCHAR buffer[1024];
        SQLLEN indicator;

        // Try to find MPN column
        SQLSMALLINT numCols;
        SQLNumResultCols(hStmt, &numCols);

        for (SQLSMALLINT i = 1; i <= numCols; i++) {
            SQLWCHAR colName[256];
            SQLSMALLINT colNameLen;
            SQLSMALLINT dataType;
            SQLULEN colSize;
            SQLSMALLINT decDigits, nullable;

            SQLDescribeColW(hStmt, i, colName, sizeof(colName)/sizeof(SQLWCHAR),
                            &colNameLen, &dataType, &colSize, &decDigits, &nullable);

            if (wcscmp(colName, L"Manufacturer Part Number") == 0 || wcscmp(colName, L"MPN") == 0) {
                SQLGetData(hStmt, i, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
                if (indicator != SQL_NULL_DATA) {
                    mpn = buffer;
                }
                break;
            }
        }
    }

    if (!mpn.empty()) {
        wprintf(L"  Found part: %s in table %s\n", mpn.c_str(), classificationTable.c_str());

        // Now do a lookup for this specific part using LIKE
        SQLCloseCursor(hStmt);
        sql = L"SELECT * FROM [" + classificationTable + L"] WHERE [Manufacturer Part Number] LIKE '" + mpn + L"'";
        ret = SQLExecDirectW(hStmt, (SQLWCHAR*)sql.c_str(), SQL_NTS);
        ASSERT_SUCCESS(ret, "Lookup query failed");

        int found = 0;
        while (SQLFetch(hStmt) == SQL_SUCCESS) {
            found++;
        }
        printf("  Lookup returned %d row(s)\n", found);
        ASSERT_TRUE(found > 0, "Part lookup returned no results");
    } else {
        printf("  No MPN column found, skipping lookup test\n");
    }

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// ALTIUM ACCESS PATTERN TESTS
//=============================================================================

bool test_altium_bind_fetch_pattern() {
    TEST_START("Altium - SQLBindCol + SQLFetch Pattern");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Execute a query
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [capacitors#Walsin_C0201]", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Get column count
    SQLSMALLINT numCols;
    ret = SQLNumResultCols(hStmt, &numCols);
    ASSERT_SUCCESS(ret, "NumResultCols failed");
    printf("  Found %d columns\n", numCols);

    // Bind buffers for first 5 columns (Altium typically binds many columns)
    const int MAX_BIND_COLS = 5;
    SQLWCHAR buffers[MAX_BIND_COLS][512];
    SQLLEN indicators[MAX_BIND_COLS];

    for (int i = 0; i < MAX_BIND_COLS && i < numCols; i++) {
        ret = SQLBindCol(hStmt, i + 1, SQL_C_WCHAR, buffers[i], sizeof(buffers[i]), &indicators[i]);
        printf("  SQLBindCol(col=%d): ret=%d\n", i + 1, ret);
        ASSERT_SUCCESS(ret, "SQLBindCol failed");
    }

    // Fetch rows and validate data is populated
    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS && rowCount < 3) {
        printf("  Row %d:\n", rowCount + 1);
        for (int i = 0; i < MAX_BIND_COLS && i < numCols; i++) {
            if (indicators[i] == SQL_NULL_DATA) {
                printf("    Col %d: NULL\n", i + 1);
            } else {
                // Check for "4444" pattern - this would indicate a bug
                std::wstring value(buffers[i]);
                if (value.find(L"4444") != std::wstring::npos) {
                    printf("    Col %d: WARNING - Contains '4444' pattern: %ls\n", i + 1, buffers[i]);
                } else {
                    wprintf(L"    Col %d: %s\n", i + 1, buffers[i]);
                }
            }
        }
        rowCount++;
    }

    printf("  Fetched %d rows via SQLBindCol/SQLFetch\n", rowCount);
    ASSERT_TRUE(rowCount > 0, "No rows fetched with bound columns");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_altium_colattribute_pattern() {
    TEST_START("Altium - SQLColAttributeW Fields Pattern");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Execute a query
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    SQLSMALLINT numCols;
    ret = SQLNumResultCols(hStmt, &numCols);
    ASSERT_SUCCESS(ret, "NumResultCols failed");

    // Test field identifiers that Altium requests (from log analysis)
    // Field 11 = SQL_COLUMN_COUNT
    // Field 15 = SQL_COLUMN_NULLABLE (SQL_DESC_NULLABLE)
    // Field 16 = SQL_COLUMN_LENGTH (SQL_DESC_LENGTH)
    // Field 17 = SQL_COLUMN_PRECISION (SQL_DESC_PRECISION)
    // Field 1011 = SQL_DESC_NAME
    // Field 1212 = Unknown (probably SQL Server specific)

    const SQLUSMALLINT testFields[] = {11, 15, 16, 17, 1011, 1212};
    const char* fieldNames[] = {"SQL_COLUMN_COUNT(11)", "SQL_COLUMN_NULLABLE(15)",
                                 "SQL_COLUMN_LENGTH(16)", "SQL_COLUMN_PRECISION(17)",
                                 "SQL_DESC_NAME(1011)", "Unknown(1212)"};

    printf("  Testing column attributes for column 1:\n");
    for (int f = 0; f < sizeof(testFields)/sizeof(testFields[0]); f++) {
        SQLWCHAR charAttr[256] = {0};
        SQLLEN numAttr = 0;
        SQLSMALLINT strLen = 0;

        ret = SQLColAttributeW(hStmt, 1, testFields[f], charAttr, sizeof(charAttr), &strLen, &numAttr);

        if (testFields[f] == 1011 || testFields[f] == 1) {
            // String attribute
            wprintf(L"    %s: ret=%d, value='%s'\n", (wchar_t*)fieldNames[f], ret, charAttr);
        } else {
            // Numeric attribute
            printf("    %s: ret=%d, value=%lld\n", fieldNames[f], ret, (long long)numAttr);
        }
    }

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_altium_prepare_without_execute() {
    TEST_START("Altium - SQLPrepare without SQLExecute (metadata only)");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // This is Altium's pattern: prepare a query to get metadata, then free without executing
    ret = SQLPrepareW(hStmt, (SQLWCHAR*)L"SELECT * FROM [capacitors#Murata_C0603]", SQL_NTS);
    ASSERT_SUCCESS(ret, "Prepare failed");

    // Get number of result columns (Altium does this)
    SQLSMALLINT numCols;
    ret = SQLNumResultCols(hStmt, &numCols);
    printf("  SQLNumResultCols after Prepare (no Execute): ret=%d, cols=%d\n", ret, numCols);

    // Altium also queries descriptor attributes
    SQLHANDLE descHandle = NULL;
    ret = SQLGetStmtAttr(hStmt, SQL_ATTR_APP_ROW_DESC, &descHandle, sizeof(descHandle), NULL);
    printf("  SQLGetStmtAttr(APP_ROW_DESC): ret=%d, handle=%p\n", ret, descHandle);

    ret = SQLGetStmtAttr(hStmt, SQL_ATTR_IMP_ROW_DESC, &descHandle, sizeof(descHandle), NULL);
    printf("  SQLGetStmtAttr(IMP_ROW_DESC): ret=%d, handle=%p\n", ret, descHandle);

    // Close statement without executing (this is what Altium does)
    ret = SQLFreeStmt(hStmt, SQL_CLOSE);
    printf("  SQLFreeStmt(SQL_CLOSE): ret=%d\n", ret);

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_altium_data_validation() {
    TEST_START("Altium - Data Validation Against JSON");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    // Query a known table from local test data and validate expected data
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [capacitors#Walsin_C0201]", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Get column names
    SQLSMALLINT numCols;
    SQLNumResultCols(hStmt, &numCols);
    printf("  Validating table with %d columns\n", numCols);

    // Look for key columns by name
    int mpnCol = -1;
    int mfgCol = -1;
    int descCol = -1;

    for (SQLSMALLINT i = 1; i <= numCols; i++) {
        SQLWCHAR colName[256];
        SQLSMALLINT colNameLen;
        SQLSMALLINT dataType;
        SQLULEN colSize;
        SQLSMALLINT decDigits, nullable;

        SQLDescribeColW(hStmt, i, colName, sizeof(colName)/sizeof(SQLWCHAR),
                        &colNameLen, &dataType, &colSize, &decDigits, &nullable);

        if (wcscmp(colName, L"mpn") == 0) mpnCol = i;
        else if (wcscmp(colName, L"manufacturer") == 0) mfgCol = i;
        else if (wcscmp(colName, L"Description") == 0) descCol = i;
    }

    printf("  Key columns: MPN=%d, Manufacturer=%d, Description=%d\n", mpnCol, mfgCol, descCol);

    // Validate first few rows have non-garbage data
    int validRows = 0;
    int invalidRows = 0;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS && validRows + invalidRows < 10) {
        SQLWCHAR buffer[1024];
        SQLLEN indicator;
        bool rowValid = true;

        if (mpnCol > 0) {
            SQLGetData(hStmt, mpnCol, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
            if (indicator != SQL_NULL_DATA) {
                std::wstring value(buffer);
                // Check for suspicious patterns
                if (value.find(L"4444") != std::wstring::npos ||
                    value.find(L"????") != std::wstring::npos ||
                    value.length() == 0) {
                    printf("  WARNING: Suspicious MPN value: '%ls'\n", buffer);
                    rowValid = false;
                }
            }
        }

        if (rowValid) {
            validRows++;
        } else {
            invalidRows++;
        }
    }

    printf("  Valid rows: %d, Invalid rows: %d\n", validRows, invalidRows);
    ASSERT_TRUE(validRows > 0, "No valid rows found");
    ASSERT_TRUE(invalidRows == 0, "Found rows with invalid/garbage data");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_sql_c_wchar_binding() {
    TEST_START("Altium - SQL_C_WCHAR (-8) Type Binding");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Bind using SQL_C_WCHAR (-8) which is what Altium uses
    SQLWCHAR buffer1[512] = {0};
    SQLWCHAR buffer2[512] = {0};
    SQLLEN ind1 = 0, ind2 = 0;

    // SQL_C_WCHAR = SQL_WCHAR = -8
    ret = SQLBindCol(hStmt, 1, SQL_C_WCHAR, buffer1, sizeof(buffer1), &ind1);
    printf("  SQLBindCol(col=1, type=SQL_C_WCHAR(-8)): ret=%d\n", ret);
    ASSERT_SUCCESS(ret, "SQLBindCol col 1 failed");

    ret = SQLBindCol(hStmt, 2, SQL_C_WCHAR, buffer2, sizeof(buffer2), &ind2);
    printf("  SQLBindCol(col=2, type=SQL_C_WCHAR(-8)): ret=%d\n", ret);
    ASSERT_SUCCESS(ret, "SQLBindCol col 2 failed");

    // Fetch first row
    ret = SQLFetch(hStmt);
    ASSERT_SUCCESS(ret, "SQLFetch failed");

    wprintf(L"  Col 1 value: '%s' (len=%lld)\n", buffer1, (long long)ind1);
    wprintf(L"  Col 2 value: '%s' (len=%lld)\n", buffer2, (long long)ind2);

    // Verify data is not garbage
    bool col1Valid = (ind1 != SQL_NULL_DATA && wcslen(buffer1) > 0);
    bool col2Valid = (ind2 != SQL_NULL_DATA && wcslen(buffer2) > 0);

    // Check for "4" pattern which would indicate wrong data
    if (wcsstr(buffer1, L"4444") != NULL || wcsstr(buffer2, L"4444") != NULL) {
        printf("  ERROR: Found '4444' pattern in bound data!\n");
        TEST_FAIL("Bound data contains suspicious '4444' pattern");
        disconnect(hEnv, hDbc, hStmt);
        return false;
    }

    printf("  Data validation: Col1=%s, Col2=%s\n",
           col1Valid ? "valid" : "INVALID",
           col2Valid ? "valid" : "INVALID");

    ASSERT_TRUE(col1Valid, "Column 1 data is invalid");
    ASSERT_TRUE(col2Valid, "Column 2 data is invalid");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// UNICODE TESTS
//=============================================================================

bool test_unicode_data() {
    TEST_START("Unicode - Data Retrieval");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Find Description column
    SQLSMALLINT numCols;
    SQLNumResultCols(hStmt, &numCols);

    int descCol = -1;
    for (SQLSMALLINT i = 1; i <= numCols; i++) {
        SQLWCHAR colName[256];
        SQLSMALLINT colNameLen;
        SQLSMALLINT dataType;
        SQLULEN colSize;
        SQLSMALLINT decDigits, nullable;

        SQLDescribeColW(hStmt, i, colName, sizeof(colName)/sizeof(SQLWCHAR),
                        &colNameLen, &dataType, &colSize, &decDigits, &nullable);
        if (wcscmp(colName, L"Description") == 0) {
            descCol = i;
            break;
        }
    }

    printf("  Description column: %d\n", descCol);

    // Look for Unicode characters in data
    int rowCount = 0;
    bool foundUnicode = false;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS && rowCount < 1000) {
        SQLWCHAR buffer[1024];
        SQLLEN indicator;

        if (descCol > 0) {
            SQLGetData(hStmt, descCol, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
            if (indicator != SQL_NULL_DATA) {
                // Check for special characters
                for (int i = 0; buffer[i] != 0; i++) {
                    if (buffer[i] > 127) {
                        if (!foundUnicode) {
                            wprintf(L"  Found Unicode: %s\n", buffer);
                            foundUnicode = true;
                        }
                        break;
                    }
                }
            }
        }
        rowCount++;
    }

    printf("  Checked %d rows, Unicode found: %s\n", rowCount, foundUnicode ? "yes" : "no");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

//=============================================================================
// TEST DATA SUBSET TESTS (deterministic with known data)
//=============================================================================

bool test_where_equals() {
    TEST_START("TestData - WHERE with = operator");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    // Query for exact match on cad-reference
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts WHERE [cad-reference] = 'LM317HVT/NOPB'", SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        printf("  WHERE = query returned: %d\n", ret);
        printDiag(SQL_HANDLE_STMT, hStmt);
    }

    int rowCount = 0;
    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        rowCount++;
    }

    printf("  Found %d rows with exact match\n", rowCount);
    ASSERT_TRUE(rowCount > 0, "No rows found with WHERE = clause");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_unicode_symbols() {
    TEST_START("TestData - Unicode symbols (ohm, micro, degree)");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    // Query the capacitor which has Unicode characters
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Find the Value column and look for Unicode symbols
    SQLSMALLINT numCols;
    SQLNumResultCols(hStmt, &numCols);

    int valueCol = -1;
    int opTempCol = -1;
    for (SQLSMALLINT i = 1; i <= numCols; i++) {
        SQLWCHAR colName[256];
        SQLSMALLINT colNameLen;
        SQLSMALLINT dataType;
        SQLULEN colSize;
        SQLSMALLINT decDigits, nullable;

        SQLDescribeColW(hStmt, i, colName, sizeof(colName)/sizeof(SQLWCHAR),
                        &colNameLen, &dataType, &colSize, &decDigits, &nullable);
        if (wcscmp(colName, L"Value") == 0) valueCol = i;
        if (wcscmp(colName, L"Operating Temperature") == 0) opTempCol = i;
    }

    printf("  Value column: %d, Operating Temperature column: %d\n", valueCol, opTempCol);

    bool foundMicro = false;
    bool foundDegree = false;
    bool foundOhm = false;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        SQLWCHAR buffer[1024];
        SQLLEN indicator;

        if (valueCol > 0) {
            SQLGetData(hStmt, valueCol, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
            if (indicator != SQL_NULL_DATA) {
                // Check for micro symbol (U+00B5 or U+03BC)
                for (int i = 0; buffer[i] != 0; i++) {
                    if (buffer[i] == L'\u00B5' || buffer[i] == L'\u03BC') {
                        foundMicro = true;
                        wprintf(L"  Found micro in Value: %s\n", buffer);
                    }
                    if (buffer[i] == L'\u03A9' || buffer[i] == L'\u2126') {
                        foundOhm = true;
                        wprintf(L"  Found ohm in Value: %s\n", buffer);
                    }
                }
            }
        }

        if (opTempCol > 0) {
            SQLGetData(hStmt, opTempCol, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
            if (indicator != SQL_NULL_DATA) {
                // Check for degree symbol (U+00B0)
                for (int i = 0; buffer[i] != 0; i++) {
                    if (buffer[i] == L'\u00B0') {
                        foundDegree = true;
                        wprintf(L"  Found degree in Operating Temperature: %s\n", buffer);
                    }
                }
            }
        }
    }

    printf("  Unicode symbols found: micro=%s, degree=%s, ohm=%s\n",
           foundMicro ? "yes" : "no",
           foundDegree ? "yes" : "no",
           foundOhm ? "yes" : "no");

    // At least one should be found in our test data
    ASSERT_TRUE(foundMicro || foundDegree || foundOhm, "No Unicode symbols found in test data");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_empty_values() {
    TEST_START("TestData - Empty string values");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    // Query the edge cases table which has empty values
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [test#edge_cases]", SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        // Table might not exist, try Parts
        SQLCloseCursor(hStmt);
        ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    }
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    // Look for rows with empty values
    int rowsWithEmpty = 0;
    int totalRows = 0;

    while ((ret = SQLFetch(hStmt)) == SQL_SUCCESS) {
        totalRows++;
        SQLWCHAR buffer[1024];
        SQLLEN indicator;

        // Check column 1 (typically Value or similar)
        SQLGetData(hStmt, 1, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
        if (indicator == SQL_NULL_DATA || (indicator != SQL_NULL_DATA && wcslen(buffer) == 0)) {
            rowsWithEmpty++;
        }
    }

    printf("  Total rows: %d, Rows with empty values: %d\n", totalRows, rowsWithEmpty);
    ASSERT_TRUE(totalRows > 0, "No rows found in test data");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_buffer_truncation() {
    TEST_START("TestData - Buffer truncation handling");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "ExecDirect failed");

    ret = SQLFetch(hStmt);
    ASSERT_SUCCESS(ret, "Fetch failed");

    // Try reading into a small buffer
    SQLWCHAR smallBuffer[10];  // Intentionally small
    SQLLEN indicator;

    ret = SQLGetData(hStmt, 1, SQL_C_WCHAR, smallBuffer, sizeof(smallBuffer), &indicator);
    printf("  SQLGetData with small buffer: ret=%d, indicator=%lld\n", ret, (long long)indicator);

    // Should return SQL_SUCCESS_WITH_INFO if data was truncated
    if (ret == SQL_SUCCESS_WITH_INFO) {
        printf("  Data truncated as expected (indicator shows full length: %lld bytes)\n", (long long)indicator);
    } else if (ret == SQL_SUCCESS) {
        printf("  Data fit in buffer (length: %lld bytes)\n", (long long)indicator);
    }

    // The function should still succeed (either SQL_SUCCESS or SQL_SUCCESS_WITH_INFO)
    ASSERT_TRUE(ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO, "GetData failed with small buffer");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_case_insensitive_columns() {
    TEST_START("TestData - Case-insensitive column names");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    // Try different case variations
    const wchar_t* queries[] = {
        L"SELECT [Manufacturer] FROM Parts",
        L"SELECT [MANUFACTURER] FROM Parts",
        L"SELECT [manufacturer] FROM Parts"
    };

    int successCount = 0;
    for (int i = 0; i < 3; i++) {
        ret = SQLExecDirectW(hStmt, (SQLWCHAR*)queries[i], SQL_NTS);
        wprintf(L"  Query: %s - ret=%d\n", queries[i], ret);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            successCount++;
            // Fetch one row to confirm
            ret = SQLFetch(hStmt);
            if (ret == SQL_SUCCESS) {
                SQLWCHAR buffer[256];
                SQLLEN indicator;
                SQLGetData(hStmt, 1, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
                wprintf(L"    Value: %s\n", buffer);
            }
        }
        SQLCloseCursor(hStmt);
    }

    printf("  %d of 3 case variations succeeded\n", successCount);
    // At least the exact case should work
    ASSERT_TRUE(successCount >= 1, "No column name case variations worked");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_statement_reuse() {
    TEST_START("TestData - Statement handle reuse");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    SQLRETURN ret;

    if (!connect(hEnv, hDbc, hStmt)) {
        TEST_FAIL("Connection to test data failed");
        return false;
    }

    // Execute first query
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "First ExecDirect failed");

    int count1 = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) count1++;
    printf("  First query: %d rows\n", count1);

    // Close cursor and reuse
    ret = SQLCloseCursor(hStmt);
    printf("  SQLCloseCursor: ret=%d\n", ret);

    // Execute second query on same statement
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "Second ExecDirect failed");

    int count2 = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) count2++;
    printf("  Second query: %d rows\n", count2);

    // Both queries should return same count
    ASSERT_TRUE(count1 == count2, "Row counts differ between queries");
    ASSERT_TRUE(count1 > 0, "No rows returned");

    // Try with SQLFreeStmt(SQL_CLOSE) instead
    ret = SQLFreeStmt(hStmt, SQL_CLOSE);
    printf("  SQLFreeStmt(SQL_CLOSE): ret=%d\n", ret);

    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM Parts", SQL_NTS);
    ASSERT_SUCCESS(ret, "Third ExecDirect failed");

    int count3 = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) count3++;
    printf("  Third query: %d rows\n", count3);

    ASSERT_TRUE(count3 == count1, "Row count differs after FreeStmt");

    disconnect(hEnv, hDbc, hStmt);
    TEST_PASS();
    return true;
}

bool test_testdata_connection() {
    TEST_START("TestData - Connect to test data folder");

    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;

    bool result = connect(hEnv, hDbc, hStmt);
    if (result) {
        // Verify we can query
        SQLRETURN ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
        if (ret == SQL_SUCCESS) {
            int tableCount = 0;
            SQLWCHAR tableName[256];
            SQLLEN tableNameLen;
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLGetData(hStmt, 3, SQL_C_WCHAR, tableName, sizeof(tableName), &tableNameLen);
                wprintf(L"  Table: %s\n", tableName);
                tableCount++;
            }
            printf("  Found %d tables in test data\n", tableCount);
        }
    }

    disconnect(hEnv, hDbc, hStmt);

    if (result) {
        TEST_PASS();
    } else {
        TEST_FAIL("Connection to test data failed");
    }
    return result;
}

//=============================================================================
// FILE WATCHER TESTS (CacheRefresh feature)
//=============================================================================

bool test_filewatcher_add_file() {
    TEST_START("FileWatcher - Add new JSON file updates cache");

    // Setup: create test directory and initial file
    if (!ensureDirectory(g_fileWatcherTestDir.c_str())) {
        TEST_FAIL("Failed to create test directory");
        return false;
    }

    // Create initial file using versions format (like the real data)
    // Note: Use lowercase "value" - driver normalizes to "Value" for ODBC
    std::wstring initialFile = std::wstring(g_fileWatcherTestDir.c_str()) + L"\\initial_fw.json";
    const char* initialContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000001",
            "classification": "fwtest/initial",
            "cad-reference": "INIT-001",
            "value": "Initial"
        }]
    })";
    if (!writeJsonFile(initialFile, initialContent)) {
        TEST_FAIL("Failed to write initial file");
        return false;
    }

    // Connect with CacheRefresh enabled
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;

    if (!connectToDataDir(hEnv, hDbc, hStmt, g_fileWatcherTestDir.c_str())) {
        deleteFile(initialFile);
        TEST_FAIL("Connection with CacheRefresh failed");
        return false;
    }

    // Small delay to ensure file watcher thread is ready
    Sleep(100);

    // Query initial table count
    SQLRETURN ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    int initialTableCount = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) initialTableCount++;
    SQLCloseCursor(hStmt);
    printf("  Initial table count: %d\n", initialTableCount);

    // Add a new JSON file
    std::wstring newFile = std::wstring(g_fileWatcherTestDir.c_str()) + L"\\added_fw.json";
    const char* addedContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000002",
            "classification": "fwtest/added",
            "cad-reference": "ADD-001",
            "value": "Added Part"
        }]
    })";
    if (!writeJsonFile(newFile, addedContent)) {
        disconnect(hEnv, hDbc, hStmt);
        deleteFile(initialFile);
        TEST_FAIL("Failed to write added file");
        return false;
    }

    // Wait for file watcher to pick up the change
    printf("  Waiting for file watcher...\n");
    Sleep(500);  // 500ms should be enough

    // Query table count again
    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    int newTableCount = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) newTableCount++;
    SQLCloseCursor(hStmt);
    printf("  New table count: %d\n", newTableCount);

    // Query the new table to verify data (classification "fwtest/added" -> "fwtest#added")
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [fwtest#added]", SQL_NTS);
    int rowCount = 0;
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        while (SQLFetch(hStmt) == SQL_SUCCESS) rowCount++;
    }
    printf("  Rows in added table: %d\n", rowCount);

    // Cleanup
    disconnect(hEnv, hDbc, hStmt);
    deleteFile(initialFile);
    deleteFile(newFile);

    // Verify new table was added
    if (newTableCount > initialTableCount && rowCount > 0) {
        TEST_PASS();
        return true;
    } else {
        TEST_FAIL("New file was not detected by file watcher");
        return false;
    }
}

bool test_filewatcher_modify_file() {
    TEST_START("FileWatcher - Modify JSON file updates cache");

    // Setup
    if (!ensureDirectory(g_fileWatcherTestDir.c_str())) {
        TEST_FAIL("Failed to create test directory");
        return false;
    }

    // Create initial file with one row using versions format
    // Note: Use lowercase "value" - driver normalizes to "Value" for ODBC
    std::wstring testFile = std::wstring(g_fileWatcherTestDir.c_str()) + L"\\modtest_fw.json";
    const char* initialContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000010",
            "classification": "fwtest/modtest",
            "cad-reference": "MOD-001",
            "value": "Version1"
        }]
    })";
    if (!writeJsonFile(testFile, initialContent)) {
        TEST_FAIL("Failed to write test file");
        return false;
    }

    // Wait for file to be fully written/flushed before connecting
    Sleep(100);

    // Connect with CacheRefresh
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;

    if (!connectToDataDir(hEnv, hDbc, hStmt, g_fileWatcherTestDir.c_str())) {
        deleteFile(testFile);
        TEST_FAIL("Connection failed");
        return false;
    }

    // Query initial data
    SQLRETURN ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [fwtest#modtest]", SQL_NTS);
    std::wstring initialValue;
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        SQLRETURN fetchRet = SQLFetch(hStmt);
        if (fetchRet == SQL_SUCCESS) {
            SQLWCHAR buffer[256];
            SQLLEN indicator;
            // Find Value column
            SQLSMALLINT numCols;
            SQLNumResultCols(hStmt, &numCols);
            printf("  Found %d columns: ", numCols);
            for (SQLSMALLINT i = 1; i <= numCols; i++) {
                SQLWCHAR colName[256];
                SQLSMALLINT colNameLen, dataType;
                SQLULEN colSize;
                SQLSMALLINT decDigits, nullable;
                SQLDescribeColW(hStmt, i, colName, 256, &colNameLen, &dataType, &colSize, &decDigits, &nullable);
                wprintf(L"%s ", colName);
                if (_wcsicmp(colName, L"Value") == 0) {
                    SQLGetData(hStmt, i, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
                    initialValue = buffer;
                }
            }
            printf("\n");
        } else {
            printf("  SQLFetch returned: %d (no rows)\n", fetchRet);
        }
    } else {
        printf("  SQLExecDirect returned: %d (table may not exist)\n", ret);
    }
    SQLCloseCursor(hStmt);
    wprintf(L"  Initial value: %s\n", initialValue.c_str());

    // Modify the file
    const char* modifiedContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000010",
            "classification": "fwtest/modtest",
            "cad-reference": "MOD-001",
            "value": "Version2-Modified"
        }]
    })";
    if (!writeJsonFile(testFile, modifiedContent)) {
        disconnect(hEnv, hDbc, hStmt);
        deleteFile(testFile);
        TEST_FAIL("Failed to modify file");
        return false;
    }

    // Wait for file watcher with retry loop (file notifications can be delayed)
    printf("  Waiting for file watcher...\n");
    std::wstring modifiedValue;
    const int maxRetries = 10;
    const int retryDelayMs = 200;

    for (int attempt = 0; attempt < maxRetries; attempt++) {
        Sleep(retryDelayMs);

        // Query modified data
        ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [fwtest#modtest]", SQL_NTS);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLWCHAR buffer[256];
                SQLLEN indicator;
                SQLSMALLINT numCols;
                SQLNumResultCols(hStmt, &numCols);
                for (SQLSMALLINT i = 1; i <= numCols; i++) {
                    SQLWCHAR colName[256];
                    SQLSMALLINT colNameLen, dataType;
                    SQLULEN colSize;
                    SQLSMALLINT decDigits, nullable;
                    SQLDescribeColW(hStmt, i, colName, 256, &colNameLen, &dataType, &colSize, &decDigits, &nullable);
                    if (wcscmp(colName, L"Value") == 0) {
                        SQLGetData(hStmt, i, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator);
                        modifiedValue = buffer;
                        break;
                    }
                }
            }
        }
        SQLCloseCursor(hStmt);

        // Check if modification was detected
        if (modifiedValue.find(L"Modified") != std::wstring::npos) {
            wprintf(L"  Modified value: %s (detected on attempt %d)\n", modifiedValue.c_str(), attempt + 1);
            break;
        }
    }

    // Cleanup
    disconnect(hEnv, hDbc, hStmt);
    deleteFile(testFile);

    // Verify value changed
    if (modifiedValue != initialValue && modifiedValue.find(L"Modified") != std::wstring::npos) {
        TEST_PASS();
        return true;
    } else {
        wprintf(L"  Final value: %s (after %d attempts, %dms total)\n", modifiedValue.c_str(), maxRetries, maxRetries * retryDelayMs);
        TEST_FAIL("File modification was not detected");
        return false;
    }
}

bool test_filewatcher_delete_file() {
    TEST_START("FileWatcher - Delete JSON file updates cache");

    // Setup
    if (!ensureDirectory(g_fileWatcherTestDir.c_str())) {
        TEST_FAIL("Failed to create test directory");
        return false;
    }

    // Create two files using versions format
    std::wstring keepFile = std::wstring(g_fileWatcherTestDir.c_str()) + L"\\keep_fw.json";
    std::wstring deleteTestFile = std::wstring(g_fileWatcherTestDir.c_str()) + L"\\todelete_fw.json";

    const char* keepContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000020",
            "classification": "fwtest/keep",
            "cad-reference": "KEEP-001",
            "value": "Keep This"
        }]
    })";
    const char* deleteContent = R"({
        "versions": [{
            "id": "01940000-0000-7000-0000-000000000021",
            "classification": "fwtest/todelete",
            "cad-reference": "DEL-001",
            "value": "Delete This"
        }]
    })";

    if (!writeJsonFile(keepFile, keepContent) || !writeJsonFile(deleteTestFile, deleteContent)) {
        TEST_FAIL("Failed to write test files");
        return false;
    }

    // Connect with CacheRefresh
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLHSTMT hStmt = NULL;

    if (!connectToDataDir(hEnv, hDbc, hStmt, g_fileWatcherTestDir.c_str())) {
        deleteFile(keepFile);
        deleteFile(deleteTestFile);
        TEST_FAIL("Connection failed");
        return false;
    }

    // Query initial table count
    SQLRETURN ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    int initialTableCount = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) initialTableCount++;
    SQLCloseCursor(hStmt);
    printf("  Initial table count: %d\n", initialTableCount);

    // Verify todelete table exists
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [fwtest#todelete]", SQL_NTS);
    bool todeleteExisted = (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
    SQLCloseCursor(hStmt);
    printf("  todelete table exists: %s\n", todeleteExisted ? "yes" : "no");

    // Delete the file
    if (!deleteFile(deleteTestFile)) {
        disconnect(hEnv, hDbc, hStmt);
        deleteFile(keepFile);
        TEST_FAIL("Failed to delete test file");
        return false;
    }

    // Wait for file watcher
    printf("  Waiting for file watcher...\n");
    Sleep(500);

    // Query table count again
    ret = SQLTablesW(hStmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    int newTableCount = 0;
    while (SQLFetch(hStmt) == SQL_SUCCESS) newTableCount++;
    SQLCloseCursor(hStmt);
    printf("  New table count: %d\n", newTableCount);

    // Verify todelete table is gone
    ret = SQLExecDirectW(hStmt, (SQLWCHAR*)L"SELECT * FROM [fwtest#todelete]", SQL_NTS);
    bool todeleteStillExists = (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
    if (todeleteStillExists) {
        int rowCount = 0;
        while (SQLFetch(hStmt) == SQL_SUCCESS) rowCount++;
        todeleteStillExists = (rowCount > 0);
    }
    SQLCloseCursor(hStmt);
    printf("  todelete table still exists: %s\n", todeleteStillExists ? "yes" : "no");

    // Cleanup
    disconnect(hEnv, hDbc, hStmt);
    deleteFile(keepFile);

    // Verify table was removed
    if (todeleteExisted && !todeleteStillExists) {
        TEST_PASS();
        return true;
    } else {
        TEST_FAIL("File deletion was not detected");
        return false;
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main() {
    // Initialize paths based on executable location
    initPaths();

    printf("========================================\n");
    printf(" odbc-monkey Test Suite\n");
    printf("========================================\n");
    printf("Driver: %ls\n", DRIVER_NAME);
    printf("DataSource: %ls\n", g_testDataDir.c_str());

    // Connection tests
    test_connection_basic();
    test_connection_multiple();

    // Attribute tests
    test_env_attributes();
    test_stmt_attributes();

    // Metadata tests
    test_sql_tables();
    test_sql_columns();
    test_sql_gettypeinfo();
    test_sql_describecol();

    // Query tests
    test_sql_execdirect();
    test_sql_prepare_execute();
    test_sql_getdata();
    test_sql_where_clause();

    // Cursor tests
    test_sql_numresultcols();
    test_sql_rowcount();
    test_sql_closecursor();

    // Info tests
    test_sql_getinfo();
    test_sql_getfunctions();

    // Classification table tests (category#table format)
    test_classification_tables();
    test_classification_query();
    test_classification_columns();
    test_classification_part_lookup();

    // Altium access pattern tests (critical for DbLib integration)
    test_altium_bind_fetch_pattern();
    test_altium_colattribute_pattern();
    test_altium_prepare_without_execute();
    test_altium_data_validation();
    test_sql_c_wchar_binding();

    // Unicode tests
    test_unicode_data();

    // Deterministic tests with known data
    test_testdata_connection();
    test_where_equals();
    test_unicode_symbols();
    test_empty_values();
    test_buffer_truncation();
    test_case_insensitive_columns();
    test_statement_reuse();

    // File watcher tests (always enabled)
    printf("\n--- File Watcher Tests ---\n");
    printf("FileWatcherTestDir: %ls\n", g_fileWatcherTestDir.c_str());
    test_filewatcher_add_file();
    test_filewatcher_modify_file();
    test_filewatcher_delete_file();

    // Summary
    printf("\n========================================\n");
    printf(" TEST SUMMARY\n");
    printf("========================================\n");
    printf("Total:  %d\n", g_testsRun);
    printf("Passed: %d\n", g_testsPassed);
    printf("Failed: %d\n", g_testsFailed);
    printf("========================================\n");

    return g_testsFailed > 0 ? 1 : 0;
}
