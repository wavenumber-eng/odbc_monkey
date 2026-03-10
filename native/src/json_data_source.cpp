// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC

#include "json_data_source.h"
#include "logger.h"
#include <algorithm>
#include <fstream>

// Classification separator: replaces "/" in "category/table" classification
// Must be a character that Altium doesn't treat specially in table names
// Tested: ":" doesn't work (Altium truncates at colon), "#" works
#define CLASSIFICATION_SEPARATOR '#'

namespace OdbcMonkey
{

// Normalize column names for Altium compatibility
// Maps canonical lowercase names to Altium's expected display names
// Only two fields need mapping: description and value (Altium built-in fields)
static std::wstring normalizeColumnNameForAltium(const std::wstring& name)
{
    if (name == L"description")
        return L"Description";
    if (name == L"value")
        return L"Value";
    return name;
}

// Convert normalized column name back to JSON key for data lookup
// This reverses the normalizeColumnNameForAltium mapping
static std::string columnNameToJsonKey(const std::wstring& columnName)
{
    // Convert wide string to UTF-8 first
    std::string utf8Name;
    if (!columnName.empty())
    {
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, columnName.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0)
        {
            utf8Name.resize(utf8Len - 1);
            WideCharToMultiByte(CP_UTF8, 0, columnName.c_str(), -1, &utf8Name[0], utf8Len, nullptr, nullptr);
        }
    }

    // Reverse mapping for JSON lookup (canonical lowercase keys)
    if (utf8Name == "Description")
        return "description";
    if (utf8Name == "Value")
        return "value";
    return utf8Name;
}

JsonDataSource::JsonDataSource(const std::wstring& directoryPath, bool enableCacheRefresh)
    : m_directoryPath(directoryPath), m_cacheRefreshEnabled(enableCacheRefresh)
{
}

JsonDataSource::~JsonDataSource()
{
    // Stop file watcher if running
    if (m_fileWatcher)
    {
        m_fileWatcher->stop();
    }
}

void JsonDataSource::enableCacheRefresh(bool enable)
{
    // Only skip if state already matches AND file watcher state is consistent
    if (enable == m_cacheRefreshEnabled && (enable == (m_fileWatcher != nullptr)))
        return;

    m_cacheRefreshEnabled = enable;

    if (enable && !m_fileWatcher)
    {
        // Start file watcher
        m_fileWatcher =
            std::make_unique<FileWatcher>(m_directoryPath, [this](const std::wstring& filename, FileAction action)
                                          { onFileChanged(filename, action); });
        m_fileWatcher->start();
        LOG("JsonDataSource: Cache refresh enabled");
    }
    else if (!enable && m_fileWatcher)
    {
        // Stop file watcher
        m_fileWatcher->stop();
        m_fileWatcher.reset();
        LOG("JsonDataSource: Cache refresh disabled");
    }
}

void JsonDataSource::onFileChanged(const std::wstring& filename, FileAction action)
{
    LOGF("JsonDataSource::onFileChanged: %ls action=%d", filename.c_str(), static_cast<int>(action));

    switch (action)
    {
    case FileAction::Added:
        addFile(filename);
        break;
    case FileAction::Modified:
        reloadFile(filename);
        break;
    case FileAction::Removed:
        removeFile(filename);
        break;
    }
}

void JsonDataSource::addFile(const std::wstring& filename)
{
    namespace fs = std::filesystem;
    fs::path fullPath = fs::path(m_directoryPath) / filename;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (loadJsonFile(fullPath))
    {
        LOGF("JsonDataSource: Added file %ls", filename.c_str());
    }
}

void JsonDataSource::reloadFile(const std::wstring& filename)
{
    // Full rebuild under a single lock to avoid stale row indices
    // and race conditions between remove and add.
    reloadAll();
    LOGF("JsonDataSource: Reloaded after file change: %ls", filename.c_str());
}

void JsonDataSource::removeFile(const std::wstring& filename)
{
    // Full rebuild under a single lock. This avoids the stale row index
    // problem: erasing rows from vectors shifts indices for other files.
    // File watcher events are infrequent, so a full reload is acceptable.
    reloadAll();
    LOGF("JsonDataSource: Removed file %ls", filename.c_str());
}

void JsonDataSource::reloadAll()
{
    namespace fs = std::filesystem;
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_tables.clear();
    m_fileIndex.clear();

    try
    {
        fs::path dirPath(m_directoryPath);
        if (!fs::exists(dirPath))
            return;

        for (const auto& entry : fs::directory_iterator(dirPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                loadJsonFile(entry.path());
            }
        }
    }
    catch (const std::exception& ex)
    {
        LOGF("JsonDataSource::reloadAll error: %s", ex.what());
    }
}

std::wstring JsonDataSource::utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return L"";
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wideLen <= 0)
        return L"";
    std::wstring wide(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wideLen);
    return wide;
}

// Replace Unicode characters not in Windows-1252 with ASCII/cp1252 equivalents.
// This ensures data displays correctly in ODBC ANSI mode (Altium).
std::wstring JsonDataSource::sanitizeForAnsi(const std::wstring& value)
{
    std::wstring result;
    result.reserve(value.length());

    for (wchar_t c : value)
    {
        switch (c)
        {
        case L'\u2126': // Ω OHM SIGN
        case L'\u03A9': // Ω GREEK CAPITAL LETTER OMEGA
            result += L"Ohm";
            break;
        case L'\u03BC':          // μ GREEK SMALL LETTER MU
            result += L'\u00B5'; // µ MICRO SIGN (in cp1252)
            break;
        case L'\u03C9': // ω GREEK SMALL LETTER OMEGA
            result += L"ohm";
            break;
        case L'\u2103':          // ℃ DEGREE CELSIUS
            result += L'\u00B0'; // ° DEGREE SIGN
            result += L'C';
            break;
        case L'\u2109':          // ℉ DEGREE FAHRENHEIT
            result += L'\u00B0'; // ° DEGREE SIGN
            result += L'F';
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

std::string JsonDataSource::wideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
        return "";
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0)
        return "";
    std::string utf8(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Len, nullptr, nullptr);
    return utf8;
}

// Extract timestamp from UUIDv7 (first 48 bits = 12 hex chars)
// UUIDv7 format: xxxxxxxx-xxxx-7xxx-xxxx-xxxxxxxxxxxx
// First 12 hex chars (without hyphens) encode milliseconds since Unix epoch
int64_t JsonDataSource::extractTimestampFromUuidV7(const std::string& uuid)
{
    try
    {
        // Remove hyphens
        std::string hex;
        hex.reserve(32);
        for (char c : uuid)
        {
            if (c != '-')
                hex += c;
        }

        if (hex.length() >= 12)
        {
            // First 12 hex chars = 48 bits of timestamp
            return std::stoll(hex.substr(0, 12), nullptr, 16);
        }
    }
    catch (...)
    {
        // Invalid UUID format - return 0
    }
    return 0;
}

bool JsonDataSource::load()
{
    LOGF("JsonDataSource::load() from %ls", m_directoryPath.c_str());

    namespace fs = std::filesystem;

    try
    {
        fs::path dirPath(m_directoryPath);
        if (!fs::exists(dirPath))
        {
            m_lastError = L"Directory does not exist: " + m_directoryPath;
            return false;
        }

        // Take write lock during initial load
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        int fileCount = 0;
        for (const auto& entry : fs::directory_iterator(dirPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                if (loadJsonFile(entry.path()))
                {
                    fileCount++;
                }
            }
        }

        // Report tables loaded
        LOGF("JsonDataSource: Loaded %d JSON files into %zu tables", fileCount, m_tables.size());
        for (const auto& pair : m_tables)
        {
            LOGF("  Table '%ls': %zu columns, %zu rows", pair.first.c_str(), pair.second.columnNames.size(),
                 pair.second.rows.size());
        }

        lock.unlock();

        // Start file watcher if cache refresh enabled
        if (m_cacheRefreshEnabled)
        {
            enableCacheRefresh(true);
        }

        // Success if we have any tables
        return !m_tables.empty();
    }
    catch (const std::exception& ex)
    {
        m_lastError = utf8ToWide(ex.what());
        return false;
    }
}

bool JsonDataSource::loadJsonFile(const std::filesystem::path& path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOGF("Failed to open: %ls", path.wstring().c_str());
            return false;
        }

        json j = json::parse(file);
        file.close();

        // Get table name from "table" field or derive from filename
        std::wstring tableName;
        std::wstring filename = path.filename().wstring();

        // Handle different JSON formats:
        // 1. Part file format: {"versions": [{...}], "table": "tablename"}
        // 2. Array format: [{row1}, {row2}, ...]

        json dataArray;

        if (j.is_object() && j.contains("versions") && j["versions"].is_array())
        {
            // Part file format - extract the LATEST version based on UUIDv7 timestamp
            const auto& versions = j["versions"];
            if (versions.empty())
            {
                return false; // Skip empty files
            }

            // Find the latest version by UUIDv7 timestamp
            const json* latestVersion = nullptr;
            int64_t latestTimestamp = -1;

            for (const auto& ver : versions)
            {
                if (!ver.is_object())
                    continue;

                int64_t ts = 0;
                if (ver.contains("id"))
                {
                    std::string id = ver["id"].get<std::string>();
                    ts = extractTimestampFromUuidV7(id);
                }

                if (ts > latestTimestamp || latestVersion == nullptr)
                {
                    latestTimestamp = ts;
                    latestVersion = &ver;
                }
            }

            if (!latestVersion)
            {
                return false; // No valid version found
            }

            // Get table name from "classification" field
            // Classification format: "category/table" -> convert to "category#table" for ODBC
            // Single-level classification "category" stays as "category" (no #)
            // Missing or empty classification -> "orphanage"
            if (latestVersion->is_object() && latestVersion->contains("classification"))
            {
                std::string classification = (*latestVersion)["classification"].get<std::string>();
                // Check for empty or whitespace-only classification
                bool isEmpty =
                    classification.empty() || classification.find_first_not_of(" \t\n\r") == std::string::npos;
                if (isEmpty)
                {
                    tableName = L"orphanage";
                }
                else
                {
                    // Replace "/" with separator for hierarchical table naming
                    for (char& c : classification)
                    {
                        if (c == '/')
                            c = CLASSIFICATION_SEPARATOR;
                    }
                    tableName = utf8ToWide(classification);
                }
            }
            else
            {
                tableName = L"orphanage"; // Missing classification field
            }

            // Only use the latest version, not all versions
            dataArray = json::array();
            dataArray.push_back(*latestVersion);
        }
        else
        {
            // Unsupported format - only versioned format is supported
            return false;
        }

        if (dataArray.empty())
        {
            return false;
        }

        const auto& firstObj = dataArray[0];
        if (!firstObj.is_object())
        {
            return false;
        }

        // Get or create the table
        bool isNewTable = (m_tables.find(tableName) == m_tables.end());

        if (isNewTable)
        {
            JsonTable table;
            table.name = tableName;

            // Extract column names from first object
            for (const auto& item : firstObj.items())
            {
                table.columnNames.push_back(normalizeColumnNameForAltium(utf8ToWide(item.key())));
            }

            m_tables[tableName] = std::move(table);
        }

        JsonTable& table = m_tables[tableName];

        // Track rows added from this file
        std::vector<RowReference>& fileRefs = m_fileIndex[filename];

        // Add rows from all versions
        for (const auto& row : dataArray)
        {
            if (!row.is_object())
                continue;

            std::vector<std::wstring> rowData;
            for (const auto& colName : table.columnNames)
            {
                // Use reverse mapping to find the original JSON key
                std::string jsonKey = columnNameToJsonKey(colName);
                if (row.contains(jsonKey))
                {
                    const auto& val = row[jsonKey];
                    if (val.is_string())
                    {
                        // Store as UTF-16 - sanitization happens at output time if needed
                        rowData.push_back(utf8ToWide(val.get<std::string>()));
                    }
                    else if (val.is_number_integer())
                    {
                        rowData.push_back(std::to_wstring(val.get<int64_t>()));
                    }
                    else if (val.is_number_float())
                    {
                        rowData.push_back(std::to_wstring(val.get<double>()));
                    }
                    else if (val.is_boolean())
                    {
                        rowData.push_back(val.get<bool>() ? L"true" : L"false");
                    }
                    else if (val.is_null())
                    {
                        rowData.push_back(L"");
                    }
                    else
                    {
                        rowData.push_back(utf8ToWide(val.dump()));
                    }
                }
                else
                {
                    rowData.push_back(L"");
                }
            }

            // Track row index for this file
            fileRefs.push_back({tableName, table.rows.size()});

            table.rows.push_back(rowData);
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        LOGF("Exception loading %ls: %s", path.wstring().c_str(), ex.what());
        return false;
    }
}

std::vector<std::wstring> JsonDataSource::getTableNames() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::wstring> names;
    for (const auto& pair : m_tables)
    {
        names.push_back(pair.first);
    }
    return names;
}

const JsonTable* JsonDataSource::getTable(const std::wstring& name) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_tables.find(name);
    if (it != m_tables.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool JsonDataSource::parseSelectQuery(const std::wstring& sql, std::wstring& tableName, std::wstring& whereClause) const
{
    std::wstring upperSql = sql;
    std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::towupper);

    size_t fromPos = upperSql.find(L"FROM");
    if (fromPos == std::wstring::npos)
        return false;

    size_t tableStart = sql.find_first_not_of(L" \t\n\r", fromPos + 4);
    if (tableStart == std::wstring::npos)
        return false;

    size_t tableEnd;
    if (sql[tableStart] == L'[')
    {
        tableStart++;
        tableEnd = sql.find(L']', tableStart);
        if (tableEnd == std::wstring::npos)
            return false;
    }
    else
    {
        tableEnd = sql.find_first_of(L" \t\n\r", tableStart);
        if (tableEnd == std::wstring::npos)
            tableEnd = sql.length();
    }

    tableName = sql.substr(tableStart, tableEnd - tableStart);

    size_t wherePos = upperSql.find(L"WHERE", tableEnd);
    if (wherePos != std::wstring::npos)
    {
        whereClause = sql.substr(wherePos + 5);
        size_t start = whereClause.find_first_not_of(L" \t\n\r");
        if (start != std::wstring::npos)
        {
            whereClause = whereClause.substr(start);
        }
    }
    else
    {
        whereClause.clear();
    }

    return true;
}

bool JsonDataSource::matchesWhere(const JsonTable& table, const std::vector<std::wstring>& row,
                                  const std::wstring& whereClause) const
{
    if (whereClause.empty())
        return true;

    std::wstring upperWhere = whereClause;
    std::transform(upperWhere.begin(), upperWhere.end(), upperWhere.begin(), ::towupper);

    size_t pos = 0;
    bool anyMatch = false;
    bool hasConditions = false;

    while (pos < upperWhere.length())
    {
        size_t colStart = whereClause.find(L'[', pos);
        if (colStart == std::wstring::npos)
            break;

        size_t colEnd = whereClause.find(L']', colStart);
        if (colEnd == std::wstring::npos)
            break;

        std::wstring colName = whereClause.substr(colStart + 1, colEnd - colStart - 1);

        // Find the column index
        int colIndex = -1;
        for (size_t i = 0; i < table.columnNames.size(); i++)
        {
            if (_wcsicmp(table.columnNames[i].c_str(), colName.c_str()) == 0)
            {
                colIndex = (int)i;
                break;
            }
        }

        // Check for LIKE operator
        size_t likePos = upperWhere.find(L"LIKE", colEnd);
        // Check for = operator (equals)
        size_t eqPos = whereClause.find(L'=', colEnd);

        // Determine which operator comes first after the column
        bool isLike = false;
        bool isEquals = false;
        size_t opEnd = std::wstring::npos;

        if (likePos != std::wstring::npos && (eqPos == std::wstring::npos || likePos < eqPos))
        {
            isLike = true;
            opEnd = likePos + 4; // "LIKE" is 4 chars
        }
        else if (eqPos != std::wstring::npos && eqPos < colEnd + 10)
        { // = should be close to ]
            isEquals = true;
            opEnd = eqPos + 1;
        }

        if (!isLike && !isEquals)
        {
            pos = colEnd + 1;
            continue;
        }

        // Find the value in quotes
        size_t patStart = whereClause.find(L'\'', opEnd);
        if (patStart == std::wstring::npos)
        {
            pos = colEnd + 1;
            continue;
        }

        size_t patEnd = whereClause.find(L'\'', patStart + 1);
        if (patEnd == std::wstring::npos)
        {
            pos = colEnd + 1;
            continue;
        }

        std::wstring pattern = whereClause.substr(patStart + 1, patEnd - patStart - 1);
        hasConditions = true;

        if (colIndex >= 0 && colIndex < (int)row.size())
        {
            std::wstring value = row[colIndex];
            std::wstring upperValue = value;
            std::wstring upperPattern = pattern;
            std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(), ::towupper);
            std::transform(upperPattern.begin(), upperPattern.end(), upperPattern.begin(), ::towupper);

            bool matches = false;

            if (isEquals)
            {
                // Exact match for = operator
                matches = (upperValue == upperPattern);
            }
            else if (isLike)
            {
                // Pattern matching for LIKE operator
                if (upperPattern.length() >= 2 && upperPattern.front() == L'%' && upperPattern.back() == L'%')
                {
                    std::wstring search = upperPattern.substr(1, upperPattern.length() - 2);
                    matches = (upperValue.find(search) != std::wstring::npos);
                }
                else if (!upperPattern.empty() && upperPattern.front() == L'%')
                {
                    std::wstring search = upperPattern.substr(1);
                    matches = (upperValue.length() >= search.length() &&
                               upperValue.substr(upperValue.length() - search.length()) == search);
                }
                else if (!upperPattern.empty() && upperPattern.back() == L'%')
                {
                    std::wstring search = upperPattern.substr(0, upperPattern.length() - 1);
                    matches = (upperValue.find(search) == 0);
                }
                else
                {
                    matches = (upperValue == upperPattern);
                }
            }

            if (matches)
                anyMatch = true;
        }

        pos = patEnd + 1;
    }

    // For OR queries, any match is sufficient
    if (upperWhere.find(L" OR ") != std::wstring::npos)
    {
        return anyMatch;
    }

    // If we had conditions, return whether any matched; otherwise return true (no filter)
    return hasConditions ? anyMatch : true;
}

std::unique_ptr<JsonTable> JsonDataSource::executeSelect(const std::wstring& sql) const
{
    LOGF("executeSelect: %ls", sql.c_str());

    std::wstring tableName, whereClause;
    if (!parseSelectQuery(sql, tableName, whereClause))
    {
        LOGF("  Failed to parse SQL");
        return nullptr;
    }

    LOGF("  Table: '%ls', Where: '%ls'", tableName.c_str(), whereClause.c_str());

    // Take shared (read) lock for data access
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    // Handle "Parts" table - merged view of all tables
    if (_wcsicmp(tableName.c_str(), L"Parts") == 0)
    {
        auto result = std::make_unique<JsonTable>();
        result->name = L"Parts";

        std::vector<std::wstring> allColumns;
        for (const auto& pair : m_tables)
        {
            for (const auto& col : pair.second.columnNames)
            {
                bool found = false;
                for (const auto& existing : allColumns)
                {
                    if (_wcsicmp(existing.c_str(), col.c_str()) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    allColumns.push_back(col);
            }
        }
        result->columnNames = allColumns;

        for (const auto& pair : m_tables)
        {
            const auto& table = pair.second;

            for (const auto& row : table.rows)
            {
                if (!whereClause.empty() && !matchesWhere(table, row, whereClause))
                {
                    continue;
                }

                std::vector<std::wstring> mergedRow(allColumns.size());
                for (size_t i = 0; i < table.columnNames.size(); i++)
                {
                    for (size_t j = 0; j < allColumns.size(); j++)
                    {
                        if (_wcsicmp(table.columnNames[i].c_str(), allColumns[j].c_str()) == 0)
                        {
                            if (i < row.size())
                            {
                                mergedRow[j] = row[i];
                            }
                            break;
                        }
                    }
                }
                result->rows.push_back(mergedRow);
            }
        }

        LOGF("  Result: %zu columns, %zu rows", result->columnNames.size(), result->rows.size());
        return result;
    }

    const JsonTable* table = getTable(tableName);
    if (!table)
    {
        LOGF("  Table not found: '%ls'", tableName.c_str());
        return nullptr;
    }

    auto result = std::make_unique<JsonTable>();
    result->name = table->name;
    result->columnNames = table->columnNames;

    for (const auto& row : table->rows)
    {
        if (matchesWhere(*table, row, whereClause))
        {
            result->rows.push_back(row);
        }
    }

    LOGF("  Result: %zu columns, %zu rows", result->columnNames.size(), result->rows.size());
    return result;
}

} // namespace OdbcMonkey
