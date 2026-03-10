// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// JSON Data Source for odbc-monkey driver
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "file_watcher.h"

namespace OdbcMonkey
{

// Represents a table loaded from JSON
struct JsonTable
{
    std::wstring name;
    std::vector<std::wstring> columnNames;
    std::vector<std::vector<std::wstring>> rows;
};

// Tracks which rows came from which file for incremental updates
struct RowReference
{
    std::wstring tableName;
    size_t rowIndex;
};

class JsonDataSource
{
  public:
    // Constructor with optional cache refresh
    explicit JsonDataSource(const std::wstring& directoryPath, bool enableCacheRefresh = false);
    ~JsonDataSource();

    bool load();
    std::vector<std::wstring> getTableNames() const;
    const JsonTable* getTable(const std::wstring& name) const;
    std::unique_ptr<JsonTable> executeSelect(const std::wstring& sql) const;
    const std::wstring& getLastError() const { return m_lastError; }

    // Unicode-to-ANSI character mapping for ODBC ANSI mode output
    static std::wstring sanitizeForAnsi(const std::wstring& value);

    // Cache refresh control
    bool isCacheRefreshEnabled() const { return m_cacheRefreshEnabled; }
    void enableCacheRefresh(bool enable);

  private:
    bool loadJsonFile(const std::filesystem::path& path);
    bool parseSelectQuery(const std::wstring& sql, std::wstring& tableName, std::wstring& whereClause) const;
    bool matchesWhere(const JsonTable& table, const std::vector<std::wstring>& row,
                      const std::wstring& whereClause) const;

    // File change handlers
    void onFileChanged(const std::wstring& filename, FileAction action);
    void reloadFile(const std::wstring& filename);
    void removeFile(const std::wstring& filename);
    void addFile(const std::wstring& filename);
    void reloadAll();

    static std::wstring utf8ToWide(const std::string& utf8);
    static std::string wideToUtf8(const std::wstring& wide);
    static int64_t extractTimestampFromUuidV7(const std::string& uuid);

    std::wstring m_directoryPath;
    std::map<std::wstring, JsonTable> m_tables;
    std::wstring m_lastError;

    // Cache refresh support
    bool m_cacheRefreshEnabled = false;
    std::unique_ptr<FileWatcher> m_fileWatcher;
    mutable std::shared_mutex m_mutex; // Protects m_tables and m_fileIndex

    // Track which file owns which rows (filename -> list of row refs)
    std::unordered_map<std::wstring, std::vector<RowReference>> m_fileIndex;
};

} // namespace OdbcMonkey
