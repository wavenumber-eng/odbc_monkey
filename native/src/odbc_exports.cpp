// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// ODBC Exports for odbc-monkey driver
// Pure native C++ - no .NET dependency

#include "handles.h"
#include "logger.h"
#include "odbc_types.h"
#include <algorithm>
#include <cstring>
#include <cwctype>

using namespace OdbcMonkey;

// Get path to the DLL's parent directory (odbc_monkey/bin/)
// Used to derive default paths relative to the repo structure
static std::wstring getDllDirectory()
{
    wchar_t dllPath[MAX_PATH];
    HMODULE hModule = NULL;
    // Get handle to this DLL
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&getDllDirectory), &hModule);
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);

    std::wstring path(dllPath);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        path = path.substr(0, pos);
    }
    return path;
}

// Forward declarations for ANSI-to-Unicode wrapper functions
extern "C" __declspec(dllexport) SQLRETURN SQL_API
SQLDriverConnectW(SQLHDBC ConnectionHandle, SQLHWND WindowHandle, SQLWCHAR* InConnectionString,
                  SQLSMALLINT StringLength1, SQLWCHAR* OutConnectionString, SQLSMALLINT BufferLength,
                  SQLSMALLINT* StringLength2Ptr, SQLUSMALLINT DriverCompletion);

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT StatementHandle, SQLWCHAR* StatementText,
                                                                  SQLINTEGER TextLength);

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLPrepareW(SQLHSTMT StatementHandle, SQLWCHAR* StatementText,
                                                               SQLINTEGER TextLength);

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLTablesW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                              SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                              SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                              SQLSMALLINT NameLength3, SQLWCHAR* TableType,
                                                              SQLSMALLINT NameLength4);

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLColumnsW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                               SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                               SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                               SQLSMALLINT NameLength3, SQLWCHAR* ColumnName,
                                                               SQLSMALLINT NameLength4);

// Helper to parse connection string
static std::wstring parseConnectionString(const std::wstring& connStr, const std::wstring& key)
{
    std::wstring upperConnStr = connStr;
    std::wstring upperKey = key + L"=";
    std::transform(upperConnStr.begin(), upperConnStr.end(), upperConnStr.begin(), ::towupper);
    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);

    size_t pos = upperConnStr.find(upperKey);
    if (pos == std::wstring::npos)
        return L"";

    size_t valueStart = pos + upperKey.length();
    size_t valueEnd = connStr.find(L';', valueStart);
    if (valueEnd == std::wstring::npos)
        valueEnd = connStr.length();

    return connStr.substr(valueStart, valueEnd - valueStart);
}

// ============================================================================
// Handle Management
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle,
                                                                  SQLHANDLE* OutputHandle)
{
    LOGF("SQLAllocHandle: type=%d, input=0x%p", HandleType, InputHandle);

    if (!OutputHandle)
    {
        LOG("  ERROR: OutputHandle is null");
        return SQL_ERROR;
    }

    switch (HandleType)
    {
    case SQL_HANDLE_ENV:
    {
        auto* env = new EnvironmentHandle();
        *OutputHandle = env;
        LOGF("  Created ENV handle: 0x%p", env);
        return SQL_SUCCESS;
    }

    case SQL_HANDLE_DBC:
    {
        auto* env = validateEnv(InputHandle);
        if (!env)
        {
            LOG("  ERROR: Invalid environment handle");
            return SQL_INVALID_HANDLE;
        }
        auto* dbc = new ConnectionHandle();
        dbc->env = env;
        env->connections.push_back(dbc);
        *OutputHandle = dbc;
        LOGF("  Created DBC handle: 0x%p", dbc);
        return SQL_SUCCESS;
    }

    case SQL_HANDLE_STMT:
    {
        auto* dbc = validateDbc(InputHandle);
        if (!dbc)
        {
            LOG("  ERROR: Invalid connection handle");
            return SQL_INVALID_HANDLE;
        }
        auto* stmt = new StatementHandle();
        stmt->dbc = dbc;
        stmt->initDescriptors(); // Initialize implicit descriptor handles
        dbc->statements.push_back(stmt);
        *OutputHandle = stmt;
        LOGF("  Created STMT handle: 0x%p", stmt);
        return SQL_SUCCESS;
    }

    default:
        LOGF("  ERROR: Unknown handle type %d", HandleType);
        return SQL_ERROR;
    }
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
    LOGF("SQLFreeHandle: type=%d, handle=0x%p", HandleType, Handle);

    switch (HandleType)
    {
    case SQL_HANDLE_ENV:
    {
        auto* env = validateEnv(Handle);
        if (!env)
            return SQL_INVALID_HANDLE;
        if (!env->connections.empty())
        {
            LOGF("  WARNING: Freeing ENV with %zu outstanding connections", env->connections.size());
        }
        env->magic = 0;
        delete env;
        return SQL_SUCCESS;
    }

    case SQL_HANDLE_DBC:
    {
        auto* dbc = validateDbc(Handle);
        if (!dbc)
            return SQL_INVALID_HANDLE;
        if (!dbc->statements.empty())
        {
            LOGF("  WARNING: Freeing DBC with %zu outstanding statements", dbc->statements.size());
        }
        if (dbc->env)
        {
            auto& conns = dbc->env->connections;
            conns.erase(std::remove(conns.begin(), conns.end(), dbc), conns.end());
        }
        dbc->magic = 0;
        delete dbc;
        return SQL_SUCCESS;
    }

    case SQL_HANDLE_STMT:
    {
        auto* stmt = validateStmt(Handle);
        if (!stmt)
            return SQL_INVALID_HANDLE;
        if (stmt->dbc)
        {
            auto& stmts = stmt->dbc->statements;
            stmts.erase(std::remove(stmts.begin(), stmts.end(), stmt), stmts.end());
        }
        stmt->magic = 0;
        delete stmt;
        return SQL_SUCCESS;
    }

    default:
        return SQL_ERROR;
    }
}

// ============================================================================
// Environment Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLSetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
                                                                 SQLPOINTER Value, SQLINTEGER StringLength)
{
    LOGF("SQLSetEnvAttr: env=0x%p, attr=%d, value=0x%p", EnvironmentHandle, Attribute, Value);

    auto* env = validateEnv(EnvironmentHandle);
    if (!env)
        return SQL_INVALID_HANDLE;

    switch (Attribute)
    {
    case SQL_ATTR_ODBC_VERSION:
        env->odbcVersion = (SQLINTEGER)(intptr_t)Value;
        LOGF("  Set ODBC version to %d", env->odbcVersion);
        return SQL_SUCCESS;

    case SQL_ATTR_CONNECTION_POOLING:
    case SQL_ATTR_CP_MATCH:
    case SQL_ATTR_OUTPUT_NTS:
        return SQL_SUCCESS;

    default:
        LOGF("  Unhandled attribute %d", Attribute);
        return SQL_SUCCESS;
    }
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
                                                                 SQLPOINTER Value, SQLINTEGER BufferLength,
                                                                 SQLINTEGER* StringLength)
{
    LOGF("SQLGetEnvAttr: attr=%d", Attribute);

    auto* env = validateEnv(EnvironmentHandle);
    if (!env)
        return SQL_INVALID_HANDLE;

    switch (Attribute)
    {
    case SQL_ATTR_ODBC_VERSION:
        if (Value)
            *(SQLINTEGER*)Value = env->odbcVersion;
        return SQL_SUCCESS;
    default:
        return SQL_SUCCESS;
    }
}

// ============================================================================
// Connection Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLConnect(SQLHDBC ConnectionHandle, SQLCHAR* ServerName,
                                                              SQLSMALLINT NameLength1, SQLCHAR* UserName,
                                                              SQLSMALLINT NameLength2, SQLCHAR* Authentication,
                                                              SQLSMALLINT NameLength3)
{
    LOG("SQLConnect (ANSI) - use SQLDriverConnect instead");
    return SQL_ERROR;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLConnectW(SQLHDBC ConnectionHandle, SQLWCHAR* ServerName,
                                                               SQLSMALLINT NameLength1, SQLWCHAR* UserName,
                                                               SQLSMALLINT NameLength2, SQLWCHAR* Authentication,
                                                               SQLSMALLINT NameLength3)
{
    LOG("SQLConnectW - use SQLDriverConnectW instead");
    return SQL_ERROR;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API
SQLDriverConnect(SQLHDBC ConnectionHandle, SQLHWND WindowHandle, SQLCHAR* InConnectionString, SQLSMALLINT StringLength1,
                 SQLCHAR* OutConnectionString, SQLSMALLINT BufferLength, SQLSMALLINT* StringLength2Ptr,
                 SQLUSMALLINT DriverCompletion)
{
    LOG("SQLDriverConnect (ANSI) - converting to Unicode");

    if (!InConnectionString)
        return SQL_ERROR;

    int len = (StringLength1 == SQL_NTS) ? (int)strlen((char*)InConnectionString) : StringLength1;
    int wideLen = MultiByteToWideChar(CP_ACP, 0, (char*)InConnectionString, len, nullptr, 0);
    std::wstring wideConnStr(wideLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, (char*)InConnectionString, len, &wideConnStr[0], wideLen);

    wchar_t outBuf[1024] = {0};
    SQLSMALLINT outLen = 0;

    SQLRETURN ret = SQLDriverConnectW(ConnectionHandle, WindowHandle, (SQLWCHAR*)wideConnStr.c_str(), SQL_NTS,
                                      (SQLWCHAR*)outBuf, 1024, &outLen, DriverCompletion);

    if (OutConnectionString && BufferLength > 0)
    {
        int ansiLen =
            WideCharToMultiByte(CP_ACP, 0, outBuf, -1, (char*)OutConnectionString, BufferLength, nullptr, nullptr);
        if (StringLength2Ptr)
            *StringLength2Ptr = (SQLSMALLINT)(ansiLen - 1);
    }

    return ret;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API
SQLDriverConnectW(SQLHDBC ConnectionHandle, SQLHWND WindowHandle, SQLWCHAR* InConnectionString,
                  SQLSMALLINT StringLength1, SQLWCHAR* OutConnectionString, SQLSMALLINT BufferLength,
                  SQLSMALLINT* StringLength2Ptr, SQLUSMALLINT DriverCompletion)
{
    LOGF("SQLDriverConnectW: dbc=0x%p", ConnectionHandle);

    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;

    if (!InConnectionString)
    {
        dbc->lastError = L"Connection string is null";
        dbc->sqlState = L"HY000";
        return SQL_ERROR;
    }

    std::wstring connStr(InConnectionString);
    LOGF("  Connection string: %ls", connStr.c_str());

    std::wstring dataSource = parseConnectionString(connStr, L"DataSource");
    if (dataSource.empty())
    {
        dataSource = parseConnectionString(connStr, L"DBQ");
    }
    if (dataSource.empty())
    {
        dbc->lastError =
            L"DataSource not specified in connection string. Use: DRIVER={odbc-monkey};DataSource=C:\\path\\to\\json";
        dbc->sqlState = L"HY000";
        LOG("  ERROR: DataSource not specified in connection string");
        return SQL_ERROR;
    }

    LOGF("  Data source path: %ls", dataSource.c_str());

    // Parse LogPort option (0 = disabled, default 9999)
    std::wstring logPortStr = parseConnectionString(connStr, L"LogPort");
    if (!logPortStr.empty())
    {
        try
        {
            int logPort = std::stoi(logPortStr);
            Logger::instance().setUdpPort(logPort);
            LOGF("  Log port: %d", logPort);
        }
        catch (...)
        {
            LOGF("  WARNING: Invalid LogPort value, ignoring");
        }
    }

    // Parse LogFile option (0 = disabled, 1 = enabled, default 1)
    std::wstring logFileStr = parseConnectionString(connStr, L"LogFile");
    if (!logFileStr.empty())
    {
        bool enabled = (logFileStr != L"0" && logFileStr != L"false" && logFileStr != L"no");
        Logger::instance().setFileLogging(enabled);
        LOGF("  File logging: %s", enabled ? "enabled" : "disabled");
    }

    // Parse LogMaxSize option (in MB, default 5)
    std::wstring logMaxSizeStr = parseConnectionString(connStr, L"LogMaxSize");
    if (!logMaxSizeStr.empty())
    {
        try
        {
            size_t maxSizeMB = std::stoul(logMaxSizeStr);
            Logger::instance().setMaxLogSize(maxSizeMB * 1024 * 1024);
            LOGF("  Log max size: %zu MB", maxSizeMB);
        }
        catch (...)
        {
            LOGF("  WARNING: Invalid LogMaxSize value, ignoring");
        }
    }

    // Parse LogPath option (custom log file path)
    std::wstring logPath = parseConnectionString(connStr, L"LogPath");
    if (!logPath.empty())
    {
        Logger::instance().setLogPath(logPath);
        LOGF("  Log path: %ls", logPath.c_str());
    }

    // File watcher is always enabled for live updates
    dbc->dataSource = std::make_unique<JsonDataSource>(dataSource, true);
    if (!dbc->dataSource->load())
    {
        dbc->lastError = L"Failed to load data source: " + dbc->dataSource->getLastError();
        dbc->sqlState = L"HY000";
        LOGF("  ERROR: %ls", dbc->lastError.c_str());
        return SQL_ERROR;
    }

    dbc->connected = true;
    dbc->dataSourcePath = dataSource;

    if (OutConnectionString && BufferLength > 0)
    {
        wcsncpy_s(OutConnectionString, BufferLength, connStr.c_str(), _TRUNCATE);
        if (StringLength2Ptr)
            *StringLength2Ptr = (SQLSMALLINT)connStr.length();
    }

    LOG("  Connected successfully");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLDisconnect(SQLHDBC ConnectionHandle)
{
    LOGF("SQLDisconnect: dbc=0x%p", ConnectionHandle);

    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;

    dbc->connected = false;
    dbc->dataSource.reset();

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLSetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                                                     SQLPOINTER Value, SQLINTEGER StringLength)
{
    LOGF("SQLSetConnectAttr: attr=%d", Attribute);
    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLSetConnectAttrW(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                                                      SQLPOINTER Value, SQLINTEGER StringLength)
{
    LOGF("SQLSetConnectAttrW: attr=%d", Attribute);
    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                                                     SQLPOINTER Value, SQLINTEGER BufferLength,
                                                                     SQLINTEGER* StringLength)
{
    LOGF("SQLGetConnectAttr: attr=%d", Attribute);
    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetConnectAttrW(SQLHDBC ConnectionHandle, SQLINTEGER Attribute,
                                                                      SQLPOINTER Value, SQLINTEGER BufferLength,
                                                                      SQLINTEGER* StringLength)
{
    LOGF("SQLGetConnectAttrW: attr=%d", Attribute);
    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;
    return SQL_SUCCESS;
}

// ============================================================================
// Statement Attribute Functions - THE CRITICAL FIX
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT StatementHandle, SQLINTEGER Attribute,
                                                                  SQLPOINTER Value, SQLINTEGER StringLength)
{
    LOGF("SQLSetStmtAttr: stmt=0x%p, attr=%d, value=0x%p, strlen=%d", StatementHandle, Attribute, Value, StringLength);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
    {
        LOG("  ERROR: Invalid statement handle");
        return SQL_INVALID_HANDLE;
    }

    switch (Attribute)
    {
    case SQL_ATTR_QUERY_TIMEOUT:
        stmt->queryTimeout = (SQLULEN)(uintptr_t)Value;
        break;
    case SQL_ATTR_MAX_ROWS:
        stmt->maxRows = (SQLULEN)(uintptr_t)Value;
        break;
    case SQL_ATTR_ROW_ARRAY_SIZE:
        stmt->rowArraySize = (SQLULEN)(uintptr_t)Value;
        break;
    default:
        LOGF("  Unhandled attribute %d - returning SUCCESS", Attribute);
        break;
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLSetStmtAttrW(SQLHSTMT StatementHandle, SQLINTEGER Attribute,
                                                                   SQLPOINTER Value, SQLINTEGER StringLength)
{
    LOGF("SQLSetStmtAttrW: stmt=0x%p, attr=%d, value=0x%p", StatementHandle, Attribute, Value);

    return SQLSetStmtAttr(StatementHandle, Attribute, Value, StringLength);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetStmtAttr(SQLHSTMT StatementHandle, SQLINTEGER Attribute,
                                                                  SQLPOINTER Value, SQLINTEGER BufferLength,
                                                                  SQLINTEGER* StringLength)
{
    LOGF("SQLGetStmtAttr: stmt=0x%p, attr=%d", StatementHandle, Attribute);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    // Initialize StringLength if provided
    if (StringLength)
        *StringLength = 0;

    switch (Attribute)
    {
    case SQL_ATTR_QUERY_TIMEOUT:
        if (Value)
            *(SQLULEN*)Value = stmt->queryTimeout;
        if (StringLength)
            *StringLength = sizeof(SQLULEN);
        break;
    case SQL_ATTR_MAX_ROWS:
        if (Value)
            *(SQLULEN*)Value = stmt->maxRows;
        if (StringLength)
            *StringLength = sizeof(SQLULEN);
        break;
    case SQL_ATTR_ROW_ARRAY_SIZE:
        if (Value)
            *(SQLULEN*)Value = stmt->rowArraySize;
        if (StringLength)
            *StringLength = sizeof(SQLULEN);
        break;
    case SQL_ATTR_CURSOR_TYPE:
        if (Value)
            *(SQLULEN*)Value = SQL_CURSOR_FORWARD_ONLY;
        if (StringLength)
            *StringLength = sizeof(SQLULEN);
        break;
    case SQL_ATTR_CONCURRENCY:
        if (Value)
            *(SQLULEN*)Value = SQL_CONCUR_READ_ONLY;
        if (StringLength)
            *StringLength = sizeof(SQLULEN);
        break;
    // Descriptor handle attributes - return our implicit descriptor handles
    case SQL_ATTR_APP_ROW_DESC: // 10010
        if (Value)
            *(SQLHANDLE*)Value = &stmt->appRowDesc;
        if (StringLength)
            *StringLength = sizeof(SQLHANDLE);
        LOGF("  Returning appRowDesc: 0x%p", &stmt->appRowDesc);
        break;
    case SQL_ATTR_APP_PARAM_DESC: // 10011
        if (Value)
            *(SQLHANDLE*)Value = &stmt->appParamDesc;
        if (StringLength)
            *StringLength = sizeof(SQLHANDLE);
        LOGF("  Returning appParamDesc: 0x%p", &stmt->appParamDesc);
        break;
    case SQL_ATTR_IMP_ROW_DESC: // 10012
        if (Value)
            *(SQLHANDLE*)Value = &stmt->impRowDesc;
        if (StringLength)
            *StringLength = sizeof(SQLHANDLE);
        LOGF("  Returning impRowDesc: 0x%p", &stmt->impRowDesc);
        break;
    case SQL_ATTR_IMP_PARAM_DESC: // 10013
        if (Value)
            *(SQLHANDLE*)Value = &stmt->impParamDesc;
        if (StringLength)
            *StringLength = sizeof(SQLHANDLE);
        LOGF("  Returning impParamDesc: 0x%p", &stmt->impParamDesc);
        break;
    default:
        // For unknown attributes, return a safe default value
        LOGF("  Unknown attribute %d, returning 0", Attribute);
        if (Value && BufferLength > 0)
        {
            memset(Value, 0, BufferLength);
        }
        else if (Value)
        {
            *(SQLULEN*)Value = 0;
        }
        break;
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetStmtAttrW(SQLHSTMT StatementHandle, SQLINTEGER Attribute,
                                                                   SQLPOINTER Value, SQLINTEGER BufferLength,
                                                                   SQLINTEGER* StringLength)
{
    return SQLGetStmtAttr(StatementHandle, Attribute, Value, BufferLength, StringLength);
}

// ============================================================================
// Query Execution
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLPrepare(SQLHSTMT StatementHandle, SQLCHAR* StatementText,
                                                              SQLINTEGER TextLength)
{
    LOG("SQLPrepare (ANSI) - converting to Unicode");

    if (!StatementText)
        return SQL_ERROR;

    int len = (TextLength == SQL_NTS) ? (int)strlen((char*)StatementText) : TextLength;
    int wideLen = MultiByteToWideChar(CP_ACP, 0, (char*)StatementText, len, nullptr, 0);
    std::wstring wideSql(wideLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, (char*)StatementText, len, &wideSql[0], wideLen);

    return SQLPrepareW(StatementHandle, (SQLWCHAR*)wideSql.c_str(), SQL_NTS);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLPrepareW(SQLHSTMT StatementHandle, SQLWCHAR* StatementText,
                                                               SQLINTEGER TextLength)
{
    LOGF("SQLPrepareW: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!StatementText)
    {
        stmt->lastError = L"Statement text is null";
        stmt->sqlState = L"HY009";
        return SQL_ERROR;
    }

    // Store the SQL for later execution
    if (TextLength == SQL_NTS)
    {
        stmt->preparedSql = StatementText;
    }
    else
    {
        stmt->preparedSql = std::wstring(StatementText, TextLength);
    }
    stmt->isPrepared = true;
    stmt->reset();

    LOGF("  Prepared SQL: %ls", stmt->preparedSql.c_str());
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLExecute(SQLHSTMT StatementHandle)
{
    LOGF("SQLExecute: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->isPrepared || stmt->preparedSql.empty())
    {
        stmt->lastError = L"No statement has been prepared";
        stmt->sqlState = L"HY010";
        return SQL_ERROR;
    }

    // Execute the prepared SQL using SQLExecDirectW
    return SQLExecDirectW(StatementHandle, (SQLWCHAR*)stmt->preparedSql.c_str(), SQL_NTS);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLExecDirect(SQLHSTMT StatementHandle, SQLCHAR* StatementText,
                                                                 SQLINTEGER TextLength)
{
    LOG("SQLExecDirect (ANSI) - converting to Unicode");

    if (!StatementText)
        return SQL_ERROR;

    int len = (TextLength == SQL_NTS) ? (int)strlen((char*)StatementText) : TextLength;
    int wideLen = MultiByteToWideChar(CP_ACP, 0, (char*)StatementText, len, nullptr, 0);
    std::wstring wideSql(wideLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, (char*)StatementText, len, &wideSql[0], wideLen);

    return SQLExecDirectW(StatementHandle, (SQLWCHAR*)wideSql.c_str(), SQL_NTS);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT StatementHandle, SQLWCHAR* StatementText,
                                                                  SQLINTEGER TextLength)
{
    LOGF("SQLExecDirectW: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->dbc || !stmt->dbc->dataSource)
    {
        stmt->lastError = L"Not connected";
        stmt->sqlState = L"08003";
        return SQL_ERROR;
    }

    std::wstring sql = (TextLength == SQL_NTS) ? std::wstring(StatementText) : std::wstring(StatementText, TextLength);
    LOGF("  SQL: %ls", sql.c_str());

    stmt->reset();

    auto result = stmt->dbc->dataSource->executeSelect(sql);
    if (!result)
    {
        stmt->lastError = L"Query execution failed";
        stmt->sqlState = L"42000";
        return SQL_ERROR;
    }

    stmt->resultSet = std::move(result);
    stmt->hasResultSet = true;
    stmt->currentRow = 0;

    // Build column info - always use SQL_WVARCHAR for Altium compatibility
    for (const auto& colName : stmt->resultSet->columnNames)
    {
        ColumnInfo col;
        col.name = colName;
        col.dataType = SQL_WVARCHAR;
        col.columnSize = 255;
        stmt->columns.push_back(col);
    }

    LOGF("  Result: %zu columns, %zu rows", stmt->columns.size(), stmt->resultSet->rows.size());

    return SQL_SUCCESS;
}

// ============================================================================
// Result Set Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLNumResultCols(SQLHSTMT StatementHandle, SQLSMALLINT* ColumnCount)
{
    LOGF("SQLNumResultCols: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (ColumnCount)
    {
        *ColumnCount = (SQLSMALLINT)stmt->columns.size();
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLRowCount(SQLHSTMT StatementHandle, SQLLEN* RowCount)
{
    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (RowCount)
    {
        *RowCount = stmt->resultSet ? (SQLLEN)stmt->resultSet->rows.size() : 0;
    }

    return SQL_SUCCESS;
}

// Helper function to populate a bound column buffer
static void populateBoundBuffer(const ColumnBinding& binding, const std::wstring& value)
{
    if (!binding.targetValue)
        return;

    if (binding.targetType == SQL_C_WCHAR || binding.targetType == SQL_C_DEFAULT)
    {
        // Unicode (UTF-16) output
        if (binding.bufferLength > 0)
        {
            size_t maxChars = (binding.bufferLength / sizeof(wchar_t)) - 1;
            size_t copyLen = (std::min)(value.length(), maxChars);
            wcsncpy_s((wchar_t*)binding.targetValue, binding.bufferLength / sizeof(wchar_t), value.c_str(), copyLen);
        }
        if (binding.strLenOrInd)
        {
            *binding.strLenOrInd = (SQLLEN)(value.length() * sizeof(wchar_t));
        }
    }
    else
    {
        // ANSI output (SQL_C_CHAR or other) - sanitize Unicode first
        std::wstring sanitized = JsonDataSource::sanitizeForAnsi(value);
        if (binding.bufferLength > 0)
        {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, sanitized.c_str(), -1, (char*)binding.targetValue,
                                              (int)binding.bufferLength, nullptr, nullptr);
            if (binding.strLenOrInd)
            {
                *binding.strLenOrInd = ansiLen > 0 ? ansiLen - 1 : 0;
            }
        }
        else if (binding.strLenOrInd)
        {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, sanitized.c_str(), -1, nullptr, 0, nullptr, nullptr);
            *binding.strLenOrInd = ansiLen > 0 ? ansiLen - 1 : 0;
        }
    }
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLFetch(SQLHSTMT StatementHandle)
{
    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->resultSet || stmt->currentRow >= stmt->resultSet->rows.size())
    {
        return SQL_NO_DATA;
    }

    // Get the current row data
    const auto& row = stmt->resultSet->rows[stmt->currentRow];

    // Populate all bound columns
    for (const auto& bindingPair : stmt->columnBindings)
    {
        SQLUSMALLINT colNum = bindingPair.first;
        const ColumnBinding& binding = bindingPair.second;

        // Column numbers are 1-based, array is 0-based
        if (colNum >= 1 && colNum <= row.size())
        {
            const std::wstring& value = row[colNum - 1];
            populateBoundBuffer(binding, value);
        }
        else if (binding.strLenOrInd)
        {
            // Column out of range - set to NULL
            *binding.strLenOrInd = SQL_NULL_DATA;
        }
    }

    stmt->currentRow++;
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLDescribeCol(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber,
                                                                  SQLCHAR* ColumnName, SQLSMALLINT BufferLength,
                                                                  SQLSMALLINT* NameLength, SQLSMALLINT* DataType,
                                                                  SQLULEN* ColumnSize, SQLSMALLINT* DecimalDigits,
                                                                  SQLSMALLINT* Nullable)
{
    LOGF("SQLDescribeCol: col=%d", ColumnNumber);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (ColumnNumber < 1 || ColumnNumber > stmt->columns.size())
    {
        return SQL_ERROR;
    }

    const auto& col = stmt->columns[ColumnNumber - 1];

    if (ColumnName && BufferLength > 0)
    {
        int ansiLen =
            WideCharToMultiByte(CP_ACP, 0, col.name.c_str(), -1, (char*)ColumnName, BufferLength, nullptr, nullptr);
        if (NameLength)
            *NameLength = (SQLSMALLINT)(ansiLen - 1);
    }

    if (DataType)
        *DataType = col.dataType;
    if (ColumnSize)
        *ColumnSize = col.columnSize;
    if (DecimalDigits)
        *DecimalDigits = col.decimalDigits;
    if (Nullable)
        *Nullable = col.nullable;

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLDescribeColW(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber,
                                                                   SQLWCHAR* ColumnName, SQLSMALLINT BufferLength,
                                                                   SQLSMALLINT* NameLength, SQLSMALLINT* DataType,
                                                                   SQLULEN* ColumnSize, SQLSMALLINT* DecimalDigits,
                                                                   SQLSMALLINT* Nullable)
{
    LOGF("SQLDescribeColW: col=%d", ColumnNumber);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (ColumnNumber < 1 || ColumnNumber > stmt->columns.size())
    {
        return SQL_ERROR;
    }

    const auto& col = stmt->columns[ColumnNumber - 1];

    if (ColumnName && BufferLength > 0)
    {
        wcsncpy_s(ColumnName, BufferLength, col.name.c_str(), _TRUNCATE);
        if (NameLength)
            *NameLength = (SQLSMALLINT)col.name.length();
    }

    if (DataType)
        *DataType = col.dataType;
    if (ColumnSize)
        *ColumnSize = col.columnSize;
    if (DecimalDigits)
        *DecimalDigits = col.decimalDigits;
    if (Nullable)
        *Nullable = col.nullable;

    return SQL_SUCCESS;
}

// Helper to return an ANSI string via SQLColAttribute
static void returnColAttributeStringA(const std::wstring& str, SQLPOINTER CharacterAttribute, SQLSMALLINT BufferLength,
                                      SQLSMALLINT* StringLength)
{
    // Calculate ANSI length
    int ansiLen = WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (StringLength)
        *StringLength = (SQLSMALLINT)(ansiLen > 0 ? ansiLen - 1 : 0);

    // Copy string if buffer provided
    if (CharacterAttribute && BufferLength > 0)
    {
        WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, (char*)CharacterAttribute, BufferLength, nullptr, nullptr);
    }
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLColAttribute(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber,
                                                                   SQLUSMALLINT FieldIdentifier,
                                                                   SQLPOINTER CharacterAttribute,
                                                                   SQLSMALLINT BufferLength, SQLSMALLINT* StringLength,
                                                                   SQLLEN* NumericAttribute)
{
    LOGF("SQLColAttribute: col=%d, field=%d", ColumnNumber, FieldIdentifier);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (ColumnNumber < 1 || ColumnNumber > stmt->columns.size())
    {
        return SQL_ERROR;
    }

    const auto& col = stmt->columns[ColumnNumber - 1];

    switch (FieldIdentifier)
    {
    // String attributes
    case SQL_DESC_NAME:
    case SQL_COLUMN_NAME:
        returnColAttributeStringA(col.name, CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_LABEL:
        returnColAttributeStringA(col.name, CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_TABLE_NAME:
#if SQL_DESC_TABLE_NAME != SQL_COLUMN_TABLE_NAME
    case SQL_DESC_TABLE_NAME:
#endif
        if (stmt->resultSet)
        {
            returnColAttributeStringA(stmt->resultSet->name, CharacterAttribute, BufferLength, StringLength);
        }
        else
        {
            returnColAttributeStringA(L"", CharacterAttribute, BufferLength, StringLength);
        }
        break;

    case SQL_COLUMN_OWNER_NAME:
#if SQL_DESC_SCHEMA_NAME != SQL_COLUMN_OWNER_NAME
    case SQL_DESC_SCHEMA_NAME:
#endif
    case SQL_COLUMN_QUALIFIER_NAME:
#if SQL_DESC_CATALOG_NAME != SQL_COLUMN_QUALIFIER_NAME
    case SQL_DESC_CATALOG_NAME:
#endif
        returnColAttributeStringA(L"", CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_TYPE_NAME:
#if SQL_DESC_TYPE_NAME != SQL_COLUMN_TYPE_NAME
    case SQL_DESC_TYPE_NAME:
#endif
        returnColAttributeStringA(L"VARCHAR", CharacterAttribute, BufferLength, StringLength);
        break;

    // Numeric attributes
    case SQL_DESC_TYPE:
    case SQL_COLUMN_TYPE:
        if (NumericAttribute)
            *NumericAttribute = col.dataType;
        break;

    case SQL_DESC_LENGTH:
    case SQL_COLUMN_LENGTH:
    case SQL_DESC_OCTET_LENGTH:
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_COLUMN_PRECISION:
    case SQL_DESC_PRECISION:
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_COLUMN_SCALE:
    case SQL_DESC_SCALE:
        if (NumericAttribute)
            *NumericAttribute = 0;
        break;

    case SQL_COLUMN_DISPLAY_SIZE:
#if SQL_DESC_DISPLAY_SIZE != SQL_COLUMN_DISPLAY_SIZE
    case SQL_DESC_DISPLAY_SIZE:
#endif
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_DESC_NULLABLE:
    case SQL_COLUMN_NULLABLE:
        if (NumericAttribute)
            *NumericAttribute = col.nullable;
        break;

    case SQL_COLUMN_UNSIGNED:
#if SQL_DESC_UNSIGNED != SQL_COLUMN_UNSIGNED
    case SQL_DESC_UNSIGNED:
#endif
    case SQL_COLUMN_MONEY:
#if SQL_DESC_FIXED_PREC_SCALE != SQL_COLUMN_MONEY
    case SQL_DESC_FIXED_PREC_SCALE:
#endif
    case SQL_COLUMN_AUTO_INCREMENT:
#if SQL_DESC_AUTO_UNIQUE_VALUE != SQL_COLUMN_AUTO_INCREMENT
    case SQL_DESC_AUTO_UNIQUE_VALUE:
#endif
    case SQL_COLUMN_CASE_SENSITIVE:
#if SQL_DESC_CASE_SENSITIVE != SQL_COLUMN_CASE_SENSITIVE
    case SQL_DESC_CASE_SENSITIVE:
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_FALSE;
        break;

    case SQL_COLUMN_UPDATABLE:
#if SQL_DESC_UPDATABLE != SQL_COLUMN_UPDATABLE
    case SQL_DESC_UPDATABLE:
#endif
        if (NumericAttribute)
            *NumericAttribute = 0;
        break;

    case SQL_COLUMN_SEARCHABLE:
#if SQL_DESC_SEARCHABLE != SQL_COLUMN_SEARCHABLE
    case SQL_DESC_SEARCHABLE:
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_PRED_SEARCHABLE;
        break;

    case SQL_DESC_COUNT:
    case SQL_COLUMN_COUNT:
        if (NumericAttribute)
            *NumericAttribute = (SQLLEN)stmt->columns.size();
        break;

    case SQL_DESC_UNNAMED:
        if (NumericAttribute)
            *NumericAttribute = SQL_NAMED;
        break;

    default:
        if (NumericAttribute)
            *NumericAttribute = 0;
        if (CharacterAttribute && BufferLength > 0)
        {
            ((char*)CharacterAttribute)[0] = '\0';
        }
        if (StringLength)
            *StringLength = 0;
        break;
    }

    return SQL_SUCCESS;
}

// Helper to return a string via SQLColAttribute
static void returnColAttributeString(const std::wstring& str, SQLPOINTER CharacterAttribute, SQLSMALLINT BufferLength,
                                     SQLSMALLINT* StringLength)
{
    // Always set StringLength (in bytes for W variant)
    SQLSMALLINT byteLen = (SQLSMALLINT)(str.length() * sizeof(wchar_t));
    if (StringLength)
        *StringLength = byteLen;

    // Copy string if buffer provided
    if (CharacterAttribute && BufferLength > 0)
    {
        size_t maxChars = BufferLength / sizeof(wchar_t);
        if (maxChars > 0)
        {
            wcsncpy_s((wchar_t*)CharacterAttribute, maxChars, str.c_str(), _TRUNCATE);
        }
    }
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLColAttributeW(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber,
                                                                    SQLUSMALLINT FieldIdentifier,
                                                                    SQLPOINTER CharacterAttribute,
                                                                    SQLSMALLINT BufferLength, SQLSMALLINT* StringLength,
                                                                    SQLLEN* NumericAttribute)
{
    LOGF("SQLColAttributeW: col=%d, field=%d", ColumnNumber, FieldIdentifier);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (ColumnNumber < 1 || ColumnNumber > stmt->columns.size())
    {
        LOGF("  ERROR: Column %d out of range (have %zu)", ColumnNumber, stmt->columns.size());
        return SQL_ERROR;
    }

    const auto& col = stmt->columns[ColumnNumber - 1];

    switch (FieldIdentifier)
    {
    // String attributes
    case SQL_DESC_NAME:   // 1011 - ODBC 3.x column name
    case SQL_COLUMN_NAME: // 1 - ODBC 2.x column name
        LOGF("  Returning column name: %ls", col.name.c_str());
        returnColAttributeString(col.name, CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_LABEL: // 18 - Column label (same as name for us)
        returnColAttributeString(col.name, CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_TABLE_NAME: // 15 - Table name
#if SQL_DESC_TABLE_NAME != SQL_COLUMN_TABLE_NAME
    case SQL_DESC_TABLE_NAME:   // 1026 - ODBC 3.x table name
#endif
        // Return the table name from result set if available
        if (stmt->resultSet)
        {
            returnColAttributeString(stmt->resultSet->name, CharacterAttribute, BufferLength, StringLength);
        }
        else
        {
            returnColAttributeString(L"", CharacterAttribute, BufferLength, StringLength);
        }
        break;

    case SQL_COLUMN_OWNER_NAME: // 16 - Schema/owner name (empty for us)
#if SQL_DESC_SCHEMA_NAME != SQL_COLUMN_OWNER_NAME
    case SQL_DESC_SCHEMA_NAME:  // 1025 - ODBC 3.x schema name
#endif
        returnColAttributeString(L"", CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_QUALIFIER_NAME: // 17 - Catalog/qualifier name (empty for us)
#if SQL_DESC_CATALOG_NAME != SQL_COLUMN_QUALIFIER_NAME
    case SQL_DESC_CATALOG_NAME:     // 1024 - ODBC 3.x catalog name
#endif
        returnColAttributeString(L"", CharacterAttribute, BufferLength, StringLength);
        break;

    case SQL_COLUMN_TYPE_NAME: // 14 - Type name as string
#if SQL_DESC_TYPE_NAME != SQL_COLUMN_TYPE_NAME
    case SQL_DESC_TYPE_NAME:   // 1030 - ODBC 3.x type name
#endif
        returnColAttributeString(L"WVARCHAR", CharacterAttribute, BufferLength, StringLength);
        break;

    // Numeric attributes
    case SQL_DESC_TYPE:   // 1002
    case SQL_COLUMN_TYPE: // 2
        if (NumericAttribute)
            *NumericAttribute = col.dataType;
        break;

    case SQL_DESC_LENGTH:       // 1003
    case SQL_COLUMN_LENGTH:     // 3
    case SQL_DESC_OCTET_LENGTH: // 1013
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_COLUMN_PRECISION: // 4
    case SQL_DESC_PRECISION:   // 1005
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_COLUMN_SCALE: // 5
    case SQL_DESC_SCALE:   // 1006
        if (NumericAttribute)
            *NumericAttribute = 0;
        break;

    case SQL_COLUMN_DISPLAY_SIZE: // 6
#if SQL_DESC_DISPLAY_SIZE != SQL_COLUMN_DISPLAY_SIZE
    case SQL_DESC_DISPLAY_SIZE:   // 1032
#endif
        if (NumericAttribute)
            *NumericAttribute = col.columnSize;
        break;

    case SQL_DESC_NULLABLE:   // 1008
    case SQL_COLUMN_NULLABLE: // 7
        if (NumericAttribute)
            *NumericAttribute = col.nullable;
        break;

    case SQL_COLUMN_UNSIGNED: // 8
#if SQL_DESC_UNSIGNED != SQL_COLUMN_UNSIGNED
    case SQL_DESC_UNSIGNED:   // 1033
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_FALSE;
        break;

    case SQL_COLUMN_MONEY:          // 9
#if SQL_DESC_FIXED_PREC_SCALE != SQL_COLUMN_MONEY
    case SQL_DESC_FIXED_PREC_SCALE: // 1034
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_FALSE;
        break;

    case SQL_COLUMN_UPDATABLE: // 10
#if SQL_DESC_UPDATABLE != SQL_COLUMN_UPDATABLE
    case SQL_DESC_UPDATABLE:   // 1035
#endif
        if (NumericAttribute)
            *NumericAttribute = 0; // SQL_ATTR_READONLY
        break;

    case SQL_COLUMN_AUTO_INCREMENT:  // 11
#if SQL_DESC_AUTO_UNIQUE_VALUE != SQL_COLUMN_AUTO_INCREMENT
    case SQL_DESC_AUTO_UNIQUE_VALUE: // 1036
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_FALSE;
        break;

    case SQL_COLUMN_CASE_SENSITIVE: // 12
#if SQL_DESC_CASE_SENSITIVE != SQL_COLUMN_CASE_SENSITIVE
    case SQL_DESC_CASE_SENSITIVE:   // 1037
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_FALSE;
        break;

    case SQL_COLUMN_SEARCHABLE: // 13
#if SQL_DESC_SEARCHABLE != SQL_COLUMN_SEARCHABLE
    case SQL_DESC_SEARCHABLE:   // 1038
#endif
        if (NumericAttribute)
            *NumericAttribute = SQL_PRED_SEARCHABLE;
        break;

    case SQL_DESC_COUNT:   // 1001 - Number of columns
    case SQL_COLUMN_COUNT: // 0
        if (NumericAttribute)
            *NumericAttribute = (SQLLEN)stmt->columns.size();
        break;

    case SQL_DESC_UNNAMED: // 1012
        if (NumericAttribute)
            *NumericAttribute = SQL_NAMED;
        break;

    default:
        // Unknown attribute - return 0 for numeric, empty string for character
        LOGF("  Unknown field %d, returning defaults", FieldIdentifier);
        if (NumericAttribute)
            *NumericAttribute = 0;
        // Also clear character attribute to avoid garbage
        if (CharacterAttribute && BufferLength > 0)
        {
            ((wchar_t*)CharacterAttribute)[0] = L'\0';
        }
        if (StringLength)
            *StringLength = 0;
        break;
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetData(SQLHSTMT StatementHandle, SQLUSMALLINT Col_or_Param_Num,
                                                              SQLSMALLINT TargetType, SQLPOINTER TargetValue,
                                                              SQLLEN BufferLength, SQLLEN* StrLen_or_Ind)
{
    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->resultSet || stmt->currentRow == 0 || stmt->currentRow > stmt->resultSet->rows.size())
    {
        return SQL_ERROR;
    }

    if (Col_or_Param_Num < 1 || Col_or_Param_Num > stmt->resultSet->columnNames.size())
    {
        return SQL_ERROR;
    }

    const auto& row = stmt->resultSet->rows[stmt->currentRow - 1];
    const std::wstring& value = row[Col_or_Param_Num - 1];

    if (TargetType == SQL_C_WCHAR || TargetType == SQL_C_DEFAULT)
    {
        // Return UTF-16 (what Altium expects)
        if (TargetValue && BufferLength > 0)
        {
            size_t maxChars = (BufferLength / sizeof(wchar_t)) - 1;
            size_t copyLen = (std::min)(value.length(), maxChars);
            wcsncpy_s((wchar_t*)TargetValue, BufferLength / sizeof(wchar_t), value.c_str(), copyLen);
        }
        if (StrLen_or_Ind)
            *StrLen_or_Ind = (SQLLEN)(value.length() * sizeof(wchar_t));
    }
    else
    {
        // Return ANSI - sanitize Unicode first
        std::wstring sanitized = JsonDataSource::sanitizeForAnsi(value);
        if (TargetValue && BufferLength > 0)
        {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, sanitized.c_str(), -1, (char*)TargetValue, (int)BufferLength,
                                              nullptr, nullptr);
            if (StrLen_or_Ind)
                *StrLen_or_Ind = ansiLen - 1;
        }
        else if (StrLen_or_Ind)
        {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, sanitized.c_str(), -1, nullptr, 0, nullptr, nullptr);
            *StrLen_or_Ind = ansiLen - 1;
        }
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetDataW(SQLHSTMT StatementHandle, SQLUSMALLINT Col_or_Param_Num,
                                                               SQLSMALLINT TargetType, SQLPOINTER TargetValue,
                                                               SQLLEN BufferLength, SQLLEN* StrLen_or_Ind)
{
    return SQLGetData(StatementHandle, Col_or_Param_Num, TargetType, TargetValue, BufferLength, StrLen_or_Ind);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLBindCol(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber,
                                                              SQLSMALLINT TargetType, SQLPOINTER TargetValue,
                                                              SQLLEN BufferLength, SQLLEN* StrLen_or_Ind)
{
    LOGF("SQLBindCol: stmt=0x%p, col=%d, type=%d, buf=0x%p, len=%lld", StatementHandle, ColumnNumber, TargetType,
         TargetValue, (long long)BufferLength);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    // Column 0 is for bookmarks - we don't support that
    if (ColumnNumber == 0)
    {
        LOG("  Column 0 (bookmark) not supported");
        return SQL_ERROR;
    }

    // If TargetValue is NULL, unbind the column
    if (TargetValue == nullptr)
    {
        stmt->columnBindings.erase(ColumnNumber);
        LOGF("  Unbound column %d", ColumnNumber);
        return SQL_SUCCESS;
    }

    // Store the binding
    ColumnBinding binding;
    binding.columnNumber = ColumnNumber;
    binding.targetType = TargetType;
    binding.targetValue = TargetValue;
    binding.bufferLength = BufferLength;
    binding.strLenOrInd = StrLen_or_Ind;

    stmt->columnBindings[ColumnNumber] = binding;
    LOGF("  Bound column %d: type=%d, bufLen=%lld", ColumnNumber, TargetType, (long long)BufferLength);

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLCloseCursor(SQLHSTMT StatementHandle)
{
    LOGF("SQLCloseCursor: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    stmt->reset();
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option)
{
    LOGF("SQLFreeStmt: option=%d", Option);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    switch (Option)
    {
    case SQL_CLOSE:
        stmt->reset();
        break;
    case SQL_DROP:
        return SQLFreeHandle(SQL_HANDLE_STMT, StatementHandle);
    case SQL_UNBIND:
        stmt->clearBindings();
        LOG("  Cleared all column bindings");
        break;
    case SQL_RESET_PARAMS:
        // We don't support parameters, nothing to reset
        break;
    }

    return SQL_SUCCESS;
}

// ============================================================================
// Catalog Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLTables(SQLHSTMT StatementHandle, SQLCHAR* CatalogName,
                                                             SQLSMALLINT NameLength1, SQLCHAR* SchemaName,
                                                             SQLSMALLINT NameLength2, SQLCHAR* TableName,
                                                             SQLSMALLINT NameLength3, SQLCHAR* TableType,
                                                             SQLSMALLINT NameLength4)
{
    LOG("SQLTables (ANSI)");
    return SQLTablesW(StatementHandle, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLTablesW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                              SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                              SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                              SQLSMALLINT NameLength3, SQLWCHAR* TableType,
                                                              SQLSMALLINT NameLength4)
{
    LOGF("SQLTablesW: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->dbc || !stmt->dbc->dataSource)
    {
        return SQL_ERROR;
    }

    stmt->reset();

    auto result = std::make_unique<JsonTable>();
    result->name = L"Tables";
    result->columnNames = {L"TABLE_CAT", L"TABLE_SCHEM", L"TABLE_NAME", L"TABLE_TYPE", L"REMARKS"};

    result->rows.push_back({L"", L"", L"Parts", L"TABLE", L"Merged view of all tables"});

    for (const auto& tableName : stmt->dbc->dataSource->getTableNames())
    {
        result->rows.push_back({L"", L"", tableName, L"TABLE", L""});
    }

    stmt->resultSet = std::move(result);
    stmt->hasResultSet = true;
    stmt->isCatalogQuery = true;
    stmt->currentRow = 0;

    for (const auto& colName : stmt->resultSet->columnNames)
    {
        ColumnInfo col;
        col.name = colName;
        col.dataType = SQL_WVARCHAR;
        col.columnSize = 128;
        stmt->columns.push_back(col);
    }

    LOGF("  Found %zu tables", stmt->resultSet->rows.size());
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLColumns(SQLHSTMT StatementHandle, SQLCHAR* CatalogName,
                                                              SQLSMALLINT NameLength1, SQLCHAR* SchemaName,
                                                              SQLSMALLINT NameLength2, SQLCHAR* TableName,
                                                              SQLSMALLINT NameLength3, SQLCHAR* ColumnName,
                                                              SQLSMALLINT NameLength4)
{
    LOG("SQLColumns (ANSI) - delegating to SQLColumnsW");

    // Convert TableName to wide if provided
    std::wstring wideTable;
    SQLWCHAR* wideTablePtr = nullptr;
    SQLSMALLINT wideTableLen = 0;

    if (TableName && NameLength3 != 0)
    {
        int len = (NameLength3 == SQL_NTS) ? (int)strlen((char*)TableName) : NameLength3;
        int wideLen = MultiByteToWideChar(CP_ACP, 0, (char*)TableName, len, nullptr, 0);
        wideTable.resize(wideLen);
        MultiByteToWideChar(CP_ACP, 0, (char*)TableName, len, &wideTable[0], wideLen);
        wideTablePtr = (SQLWCHAR*)wideTable.c_str();
        wideTableLen = (SQLSMALLINT)wideTable.length();
    }

    return SQLColumnsW(StatementHandle, nullptr, 0, nullptr, 0, wideTablePtr, wideTableLen, nullptr, 0);
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLColumnsW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                               SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                               SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                               SQLSMALLINT NameLength3, SQLWCHAR* ColumnName,
                                                               SQLSMALLINT NameLength4)
{
    LOGF("SQLColumnsW: stmt=0x%p", StatementHandle);

    auto* stmt = validateStmt(StatementHandle);
    if (!stmt)
        return SQL_INVALID_HANDLE;

    if (!stmt->dbc || !stmt->dbc->dataSource)
    {
        return SQL_ERROR;
    }

    stmt->reset();

    // Get table name if provided
    std::wstring targetTable;
    if (TableName && NameLength3 != 0)
    {
        targetTable = (NameLength3 == SQL_NTS) ? TableName : std::wstring(TableName, NameLength3);
    }

    LOGF("  Table filter: %ls", targetTable.empty() ? L"(all)" : targetTable.c_str());

    // Create result set for SQLColumns output
    // ODBC SQLColumns result set has 18 columns (standard)
    auto result = std::make_unique<JsonTable>();
    result->name = L"Columns";
    result->columnNames = {
        L"TABLE_CAT",         // 1
        L"TABLE_SCHEM",       // 2
        L"TABLE_NAME",        // 3
        L"COLUMN_NAME",       // 4
        L"DATA_TYPE",         // 5
        L"TYPE_NAME",         // 6
        L"COLUMN_SIZE",       // 7
        L"BUFFER_LENGTH",     // 8
        L"DECIMAL_DIGITS",    // 9
        L"NUM_PREC_RADIX",    // 10
        L"NULLABLE",          // 11
        L"REMARKS",           // 12
        L"COLUMN_DEF",        // 13
        L"SQL_DATA_TYPE",     // 14
        L"SQL_DATETIME_SUB",  // 15
        L"CHAR_OCTET_LENGTH", // 16
        L"ORDINAL_POSITION",  // 17
        L"IS_NULLABLE"        // 18
    };

    // Get column names from the table
    // For "Parts" table, execute a query to get column names
    std::vector<std::wstring> columnNames;

    if (targetTable.empty() || _wcsicmp(targetTable.c_str(), L"Parts") == 0)
    {
        // Get columns from Parts (merged) table
        auto queryResult = stmt->dbc->dataSource->executeSelect(L"SELECT * FROM Parts");
        if (queryResult)
        {
            columnNames = queryResult->columnNames;
        }
    }
    else
    {
        // Get columns from specific table via executeSelect (thread-safe copy)
        std::wstring query = L"SELECT * FROM [" + targetTable + L"]";
        auto queryResult = stmt->dbc->dataSource->executeSelect(query);
        if (queryResult)
        {
            columnNames = queryResult->columnNames;
        }
    }

    // Build result rows - one row per column
    int ordinal = 1;
    for (const auto& colName : columnNames)
    {
        std::vector<std::wstring> row;
        row.push_back(L"");                                          // TABLE_CAT
        row.push_back(L"");                                          // TABLE_SCHEM
        row.push_back(targetTable.empty() ? L"Parts" : targetTable); // TABLE_NAME
        row.push_back(colName);                                      // COLUMN_NAME
        row.push_back(std::to_wstring(SQL_WVARCHAR));                // DATA_TYPE (-9)
        row.push_back(L"WVARCHAR");                                  // TYPE_NAME
        row.push_back(L"255");                                       // COLUMN_SIZE
        row.push_back(L"510");                                       // BUFFER_LENGTH (255 * 2 for UTF-16)
        row.push_back(L"0");                                         // DECIMAL_DIGITS
        row.push_back(L"");                                          // NUM_PREC_RADIX
        row.push_back(std::to_wstring(SQL_NULLABLE));                // NULLABLE (1)
        row.push_back(L"");                                          // REMARKS
        row.push_back(L"");                                          // COLUMN_DEF
        row.push_back(std::to_wstring(SQL_WVARCHAR));                // SQL_DATA_TYPE
        row.push_back(L"");                                          // SQL_DATETIME_SUB
        row.push_back(L"510");                                       // CHAR_OCTET_LENGTH
        row.push_back(std::to_wstring(ordinal));                     // ORDINAL_POSITION
        row.push_back(L"YES");                                       // IS_NULLABLE

        result->rows.push_back(row);
        ordinal++;
    }

    // Set up column info for the result set BEFORE moving
    for (const auto& colName : result->columnNames)
    {
        ColumnInfo col;
        col.name = colName;
        col.dataType = SQL_WVARCHAR;
        col.columnSize = 128;
        stmt->columns.push_back(col);
    }

    size_t columnCount = result->rows.size();
    stmt->resultSet = std::move(result);
    stmt->hasResultSet = true;
    stmt->isCatalogQuery = true;
    stmt->currentRow = 0;

    LOGF("  Returning %zu columns for table", columnCount);
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetTypeInfo(SQLHSTMT StatementHandle, SQLSMALLINT DataType)
{
    LOG("SQLGetTypeInfo");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetTypeInfoW(SQLHSTMT StatementHandle, SQLSMALLINT DataType)
{
    LOG("SQLGetTypeInfoW");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLStatistics(SQLHSTMT StatementHandle, SQLCHAR* CatalogName,
                                                                 SQLSMALLINT NameLength1, SQLCHAR* SchemaName,
                                                                 SQLSMALLINT NameLength2, SQLCHAR* TableName,
                                                                 SQLSMALLINT NameLength3, SQLUSMALLINT Unique,
                                                                 SQLUSMALLINT Reserved)
{
    LOG("SQLStatistics");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLStatisticsW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                                  SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                                  SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                                  SQLSMALLINT NameLength3, SQLUSMALLINT Unique,
                                                                  SQLUSMALLINT Reserved)
{
    LOG("SQLStatisticsW");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLPrimaryKeys(SQLHSTMT StatementHandle, SQLCHAR* CatalogName,
                                                                  SQLSMALLINT NameLength1, SQLCHAR* SchemaName,
                                                                  SQLSMALLINT NameLength2, SQLCHAR* TableName,
                                                                  SQLSMALLINT NameLength3)
{
    LOG("SQLPrimaryKeys");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLPrimaryKeysW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                                   SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                                   SQLSMALLINT NameLength2, SQLWCHAR* TableName,
                                                                   SQLSMALLINT NameLength3)
{
    LOG("SQLPrimaryKeysW");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLForeignKeys(SQLHSTMT StatementHandle, SQLCHAR* PKCatalogName,
                                                                  SQLSMALLINT NameLength1, SQLCHAR* PKSchemaName,
                                                                  SQLSMALLINT NameLength2, SQLCHAR* PKTableName,
                                                                  SQLSMALLINT NameLength3, SQLCHAR* FKCatalogName,
                                                                  SQLSMALLINT NameLength4, SQLCHAR* FKSchemaName,
                                                                  SQLSMALLINT NameLength5, SQLCHAR* FKTableName,
                                                                  SQLSMALLINT NameLength6)
{
    LOG("SQLForeignKeys");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLForeignKeysW(SQLHSTMT StatementHandle, SQLWCHAR* PKCatalogName,
                                                                   SQLSMALLINT NameLength1, SQLWCHAR* PKSchemaName,
                                                                   SQLSMALLINT NameLength2, SQLWCHAR* PKTableName,
                                                                   SQLSMALLINT NameLength3, SQLWCHAR* FKCatalogName,
                                                                   SQLSMALLINT NameLength4, SQLWCHAR* FKSchemaName,
                                                                   SQLSMALLINT NameLength5, SQLWCHAR* FKTableName,
                                                                   SQLSMALLINT NameLength6)
{
    LOG("SQLForeignKeysW");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLProcedures(SQLHSTMT StatementHandle, SQLCHAR* CatalogName,
                                                                 SQLSMALLINT NameLength1, SQLCHAR* SchemaName,
                                                                 SQLSMALLINT NameLength2, SQLCHAR* ProcName,
                                                                 SQLSMALLINT NameLength3)
{
    LOG("SQLProcedures");
    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLProceduresW(SQLHSTMT StatementHandle, SQLWCHAR* CatalogName,
                                                                  SQLSMALLINT NameLength1, SQLWCHAR* SchemaName,
                                                                  SQLSMALLINT NameLength2, SQLWCHAR* ProcName,
                                                                  SQLSMALLINT NameLength3)
{
    LOG("SQLProceduresW");
    return SQL_SUCCESS;
}

// ============================================================================
// Info Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetInfo(SQLHDBC ConnectionHandle, SQLUSMALLINT InfoType,
                                                              SQLPOINTER InfoValue, SQLSMALLINT BufferLength,
                                                              SQLSMALLINT* StringLength)
{
    LOGF("SQLGetInfo: type=%d", InfoType);

    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;

    switch (InfoType)
    {
    case SQL_DRIVER_NAME:
        if (InfoValue && BufferLength > 0)
        {
            strncpy_s((char*)InfoValue, BufferLength, "odbc-monkey.dll", _TRUNCATE);
            if (StringLength)
                *StringLength = 15;
        }
        break;
    case SQL_DRIVER_VER:
        if (InfoValue && BufferLength > 0)
        {
            strncpy_s((char*)InfoValue, BufferLength, ODBCMONKEY_DRIVER_VER, _TRUNCATE);
            if (StringLength)
                *StringLength = (SQLSMALLINT)strlen(ODBCMONKEY_DRIVER_VER);
        }
        break;
    case SQL_DRIVER_ODBC_VER:
        if (InfoValue && BufferLength > 0)
        {
            strncpy_s((char*)InfoValue, BufferLength, ODBCMONKEY_ODBC_VER, _TRUNCATE);
            if (StringLength)
                *StringLength = (SQLSMALLINT)strlen(ODBCMONKEY_ODBC_VER);
        }
        break;
    case SQL_DBMS_NAME:
        if (InfoValue && BufferLength > 0)
        {
            strncpy_s((char*)InfoValue, BufferLength, "OdbcMonkey JSON Database", _TRUNCATE);
            if (StringLength)
                *StringLength = 24;
        }
        break;
    case SQL_DBMS_VER:
        if (InfoValue && BufferLength > 0)
        {
            strncpy_s((char*)InfoValue, BufferLength, ODBCMONKEY_DRIVER_VER, _TRUNCATE);
            if (StringLength)
                *StringLength = (SQLSMALLINT)strlen(ODBCMONKEY_DRIVER_VER);
        }
        break;
    default:
        if (InfoValue)
            memset(InfoValue, 0, BufferLength);
        if (StringLength)
            *StringLength = 0;
        break;
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetInfoW(SQLHDBC ConnectionHandle, SQLUSMALLINT InfoType,
                                                               SQLPOINTER InfoValue, SQLSMALLINT BufferLength,
                                                               SQLSMALLINT* StringLength)
{
    LOGF("SQLGetInfoW: type=%d", InfoType);

    auto* dbc = validateDbc(ConnectionHandle);
    if (!dbc)
        return SQL_INVALID_HANDLE;

    switch (InfoType)
    {
    case SQL_DRIVER_NAME:
        if (InfoValue && BufferLength > 0)
        {
            wcsncpy_s((wchar_t*)InfoValue, BufferLength / sizeof(wchar_t), L"odbc-monkey.dll", _TRUNCATE);
            if (StringLength)
                *StringLength = 15 * sizeof(wchar_t);
        }
        break;
    case SQL_DRIVER_VER:
        if (InfoValue && BufferLength > 0)
        {
            wcsncpy_s((wchar_t*)InfoValue, BufferLength / sizeof(wchar_t), L"01.01.0000", _TRUNCATE);
            if (StringLength)
                *StringLength = 10 * sizeof(wchar_t);
        }
        break;
    case SQL_DRIVER_ODBC_VER:
        if (InfoValue && BufferLength > 0)
        {
            wcsncpy_s((wchar_t*)InfoValue, BufferLength / sizeof(wchar_t), L"03.80", _TRUNCATE);
            if (StringLength)
                *StringLength = 5 * sizeof(wchar_t);
        }
        break;
    case SQL_DBMS_NAME:
        if (InfoValue && BufferLength > 0)
        {
            wcsncpy_s((wchar_t*)InfoValue, BufferLength / sizeof(wchar_t), L"OdbcMonkey JSON Database", _TRUNCATE);
            if (StringLength)
                *StringLength = 24 * sizeof(wchar_t);
        }
        break;
    default:
        if (InfoValue)
            memset(InfoValue, 0, BufferLength);
        if (StringLength)
            *StringLength = 0;
        break;
    }

    return SQL_SUCCESS;
}

// Helper to set a bit in ODBC3 function bitmap
static inline void setFunctionBit(SQLUSMALLINT* bitmap, SQLUSMALLINT func)
{
    // SQL_API_ODBC3_ALL_FUNCTIONS uses a bitmap where each bit represents a function
    // The bitmap is 250 USHORTs, each with 16 bits
    bitmap[func >> 4] |= (1 << (func & 0xF));
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetFunctions(SQLHDBC ConnectionHandle, SQLUSMALLINT FunctionId,
                                                                   SQLUSMALLINT* Supported)
{
    LOGF("SQLGetFunctions: func=%d", FunctionId);

    if (!Supported)
        return SQL_ERROR;

    // List of supported functions
    static const SQLUSMALLINT supportedFuncs[] = {
        SQL_API_SQLALLOCHANDLE,
        SQL_API_SQLFREEHANDLE,
        SQL_API_SQLCONNECT,
        SQL_API_SQLDRIVERCONNECT,
        SQL_API_SQLDISCONNECT,
        SQL_API_SQLEXECDIRECT,
        SQL_API_SQLEXECUTE,
        SQL_API_SQLFETCH,
        SQL_API_SQLGETDATA,
        SQL_API_SQLDESCRIBECOL,
        SQL_API_SQLCOLATTRIBUTE,
        SQL_API_SQLNUMRESULTCOLS,
        SQL_API_SQLROWCOUNT,
        SQL_API_SQLTABLES,
        SQL_API_SQLCOLUMNS,
        SQL_API_SQLGETINFO,
        SQL_API_SQLGETFUNCTIONS,
        SQL_API_SQLSETSTMTATTR,
        SQL_API_SQLGETSTMTATTR,
        SQL_API_SQLSETCONNECTATTR,
        SQL_API_SQLGETCONNECTATTR,
        SQL_API_SQLSETENVATTR,
        SQL_API_SQLGETENVATTR,
        SQL_API_SQLCLOSECURSOR,
        SQL_API_SQLFREESTMT,
        SQL_API_SQLBINDCOL,
        SQL_API_SQLGETDIAGREC,
        SQL_API_SQLPREPARE,
        SQL_API_SQLGETTYPEINFO,
        0 // terminator
    };

    if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS)
    {
        // Driver Manager wants a bitmap of all supported functions
        // Initialize to zeros
        memset(Supported, 0, SQL_API_ODBC3_ALL_FUNCTIONS_SIZE * sizeof(SQLUSMALLINT));

        // Set bits for supported functions
        for (int i = 0; supportedFuncs[i] != 0; i++)
        {
            setFunctionBit(Supported, supportedFuncs[i]);
        }

        LOGF("  Returned ODBC3 function bitmap");
        return SQL_SUCCESS;
    }

    // Single function query
    *Supported = SQL_FALSE;
    for (int i = 0; supportedFuncs[i] != 0; i++)
    {
        if (supportedFuncs[i] == FunctionId)
        {
            *Supported = SQL_TRUE;
            break;
        }
    }

    return SQL_SUCCESS;
}

// ============================================================================
// Diagnostic Functions
// ============================================================================

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT HandleType, SQLHANDLE Handle,
                                                                 SQLSMALLINT RecNumber, SQLCHAR* Sqlstate,
                                                                 SQLINTEGER* NativeError, SQLCHAR* MessageText,
                                                                 SQLSMALLINT BufferLength, SQLSMALLINT* TextLength)
{
    LOGF("SQLGetDiagRec: type=%d, rec=%d", HandleType, RecNumber);

    if (RecNumber != 1)
        return SQL_NO_DATA;

    std::wstring sqlState = L"00000";
    std::wstring message;

    switch (HandleType)
    {
    case SQL_HANDLE_ENV:
    {
        auto* env = validateEnv(Handle);
        if (env)
        {
            sqlState = env->sqlState;
            message = env->lastError;
        }
        break;
    }
    case SQL_HANDLE_DBC:
    {
        auto* dbc = validateDbc(Handle);
        if (dbc)
        {
            sqlState = dbc->sqlState;
            message = dbc->lastError;
        }
        break;
    }
    case SQL_HANDLE_STMT:
    {
        auto* stmt = validateStmt(Handle);
        if (stmt)
        {
            sqlState = stmt->sqlState;
            message = stmt->lastError;
        }
        break;
    }
    }

    if (message.empty())
        return SQL_NO_DATA;

    if (Sqlstate)
    {
        WideCharToMultiByte(CP_ACP, 0, sqlState.c_str(), -1, (char*)Sqlstate, 6, nullptr, nullptr);
    }

    if (NativeError)
        *NativeError = 0;

    if (MessageText && BufferLength > 0)
    {
        int len =
            WideCharToMultiByte(CP_ACP, 0, message.c_str(), -1, (char*)MessageText, BufferLength, nullptr, nullptr);
        if (TextLength)
            *TextLength = (SQLSMALLINT)(len - 1);
    }

    return SQL_SUCCESS;
}

extern "C" __declspec(dllexport) SQLRETURN SQL_API SQLGetDiagRecW(SQLSMALLINT HandleType, SQLHANDLE Handle,
                                                                  SQLSMALLINT RecNumber, SQLWCHAR* Sqlstate,
                                                                  SQLINTEGER* NativeError, SQLWCHAR* MessageText,
                                                                  SQLSMALLINT BufferLength, SQLSMALLINT* TextLength)
{
    LOGF("SQLGetDiagRecW: type=%d, rec=%d", HandleType, RecNumber);

    if (RecNumber != 1)
        return SQL_NO_DATA;

    std::wstring sqlState = L"00000";
    std::wstring message;

    switch (HandleType)
    {
    case SQL_HANDLE_ENV:
    {
        auto* env = validateEnv(Handle);
        if (env)
        {
            sqlState = env->sqlState;
            message = env->lastError;
        }
        break;
    }
    case SQL_HANDLE_DBC:
    {
        auto* dbc = validateDbc(Handle);
        if (dbc)
        {
            sqlState = dbc->sqlState;
            message = dbc->lastError;
        }
        break;
    }
    case SQL_HANDLE_STMT:
    {
        auto* stmt = validateStmt(Handle);
        if (stmt)
        {
            sqlState = stmt->sqlState;
            message = stmt->lastError;
        }
        break;
    }
    }

    if (message.empty())
        return SQL_NO_DATA;

    if (Sqlstate)
        wcsncpy_s(Sqlstate, 6, sqlState.c_str(), _TRUNCATE);
    if (NativeError)
        *NativeError = 0;

    if (TextLength)
        *TextLength = (SQLSMALLINT)message.length();

    if (MessageText && BufferLength > 0)
    {
        wcsncpy_s(MessageText, BufferLength, message.c_str(), _TRUNCATE);
        if ((SQLSMALLINT)message.length() >= BufferLength)
            return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}
