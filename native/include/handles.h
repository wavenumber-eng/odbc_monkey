// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// ODBC Handle structures for odbc-monkey driver
#pragma once

#include "json_data_source.h"
#include "odbc_types.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace OdbcMonkey
{

struct ConnectionHandle;
struct StatementHandle;
struct DescriptorHandle;

#define DESC_MAGIC 0x44455343 // DESC

// Descriptor types
enum DescType
{
    DESC_APP_ROW,
    DESC_APP_PARAM,
    DESC_IMP_ROW,
    DESC_IMP_PARAM
};

struct DescriptorHandle
{
    SQLUINTEGER magic = DESC_MAGIC;
    StatementHandle* stmt = nullptr;
    DescType type;

    bool isValid() const { return magic == DESC_MAGIC; }
};

struct EnvironmentHandle
{
    SQLUINTEGER magic = ENV_MAGIC;
    SQLINTEGER odbcVersion = SQL_OV_ODBC3;
    std::vector<ConnectionHandle*> connections;
    std::wstring lastError;
    std::wstring sqlState = L"00000";

    bool isValid() const { return magic == ENV_MAGIC; }
};

struct ConnectionHandle
{
    SQLUINTEGER magic = DBC_MAGIC;
    EnvironmentHandle* env = nullptr;
    std::unique_ptr<JsonDataSource> dataSource;
    std::vector<StatementHandle*> statements;
    bool connected = false;
    std::wstring dataSourcePath;
    std::wstring lastError;
    std::wstring sqlState = L"00000";

    bool isValid() const { return magic == DBC_MAGIC; }
};

struct ColumnInfo
{
    std::wstring name;
    SQLSMALLINT dataType = SQL_WVARCHAR;
    SQLULEN columnSize = 255;
    SQLSMALLINT decimalDigits = 0;
    SQLSMALLINT nullable = SQL_NULLABLE;
};

// Column binding for SQLBindCol - stores application buffer info
struct ColumnBinding
{
    SQLUSMALLINT columnNumber = 0;
    SQLSMALLINT targetType = SQL_C_DEFAULT;
    SQLPOINTER targetValue = nullptr;
    SQLLEN bufferLength = 0;
    SQLLEN* strLenOrInd = nullptr;
};

struct StatementHandle
{
    SQLUINTEGER magic = STMT_MAGIC;
    ConnectionHandle* dbc = nullptr;

    std::unique_ptr<JsonTable> resultSet;
    std::vector<ColumnInfo> columns;
    size_t currentRow = 0;
    bool hasResultSet = false;
    bool isCatalogQuery = false;

    std::wstring lastError;
    std::wstring sqlState = L"00000";

    SQLULEN queryTimeout = 0;
    SQLULEN maxRows = 0;
    SQLULEN rowArraySize = 1;

    // For SQLPrepare/SQLExecute
    std::wstring preparedSql;
    bool isPrepared = false;

    // Column bindings from SQLBindCol - key is column number (1-based)
    std::map<SQLUSMALLINT, ColumnBinding> columnBindings;

    // Implicit descriptors (automatically allocated with statement)
    DescriptorHandle appRowDesc{DESC_MAGIC, nullptr, DESC_APP_ROW};
    DescriptorHandle appParamDesc{DESC_MAGIC, nullptr, DESC_APP_PARAM};
    DescriptorHandle impRowDesc{DESC_MAGIC, nullptr, DESC_IMP_ROW};
    DescriptorHandle impParamDesc{DESC_MAGIC, nullptr, DESC_IMP_PARAM};

    bool isValid() const { return magic == STMT_MAGIC; }

    void initDescriptors()
    {
        appRowDesc.stmt = this;
        appParamDesc.stmt = this;
        impRowDesc.stmt = this;
        impParamDesc.stmt = this;
    }

    void reset()
    {
        resultSet.reset();
        columns.clear();
        currentRow = 0;
        hasResultSet = false;
        isCatalogQuery = false;
        lastError.clear();
        sqlState = L"00000";
        // Don't reset preparedSql/isPrepared - that stays until new Prepare
    }

    void resetAll()
    {
        reset();
        preparedSql.clear();
        isPrepared = false;
        columnBindings.clear();
    }

    void clearBindings() { columnBindings.clear(); }
};

inline EnvironmentHandle* validateEnv(SQLHANDLE handle)
{
    if (!handle)
        return nullptr;
    auto* env = static_cast<EnvironmentHandle*>(handle);
    return env->isValid() ? env : nullptr;
}

inline ConnectionHandle* validateDbc(SQLHANDLE handle)
{
    if (!handle)
        return nullptr;
    auto* dbc = static_cast<ConnectionHandle*>(handle);
    return dbc->isValid() ? dbc : nullptr;
}

inline StatementHandle* validateStmt(SQLHANDLE handle)
{
    if (!handle)
        return nullptr;
    auto* stmt = static_cast<StatementHandle*>(handle);
    return stmt->isValid() ? stmt : nullptr;
}

} // namespace OdbcMonkey
