// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC

#include "file_watcher.h"
#include "logger.h"
#include <vector>

namespace OdbcMonkey
{

FileWatcher::FileWatcher(const std::wstring& directory, Callback callback)
    : m_directory(directory), m_callback(std::move(callback))
{
}

FileWatcher::~FileWatcher()
{
    stop();
}

bool FileWatcher::start()
{
    if (m_running.load())
    {
        return true; // Already running
    }

    // Open directory handle for monitoring
    m_dirHandle =
        CreateFileW(m_directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (m_dirHandle == INVALID_HANDLE_VALUE)
    {
        m_lastError = L"Failed to open directory: " + m_directory;
        LOGF("FileWatcher: Failed to open directory, error=%lu", GetLastError());
        return false;
    }

    // Create stop event
    m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_stopEvent)
    {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
        m_lastError = L"Failed to create stop event";
        return false;
    }

    m_running.store(true);

    // Start watcher thread
    m_thread = std::thread(&FileWatcher::watchThread, this);

    LOGF("FileWatcher: Started watching %ls", m_directory.c_str());
    return true;
}

void FileWatcher::stop()
{
    if (!m_running.load())
    {
        return;
    }

    m_running.store(false);

    // Signal stop event
    if (m_stopEvent)
    {
        SetEvent(m_stopEvent);
    }

    // Cancel any pending I/O
    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(m_dirHandle, nullptr);
    }

    // Wait for thread to exit
    if (m_thread.joinable())
    {
        m_thread.join();
    }

    // Clean up handles
    if (m_stopEvent)
    {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }
    if (m_dirHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
    }

    LOG("FileWatcher: Stopped");
}

void FileWatcher::watchThread()
{
    constexpr DWORD bufferSize = 64 * 1024; // 64KB buffer
    std::vector<BYTE> buffer(bufferSize);

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent)
    {
        LOGF("FileWatcher: Failed to create overlapped event, error=%lu", GetLastError());
        return;
    }

    HANDLE waitHandles[2] = {m_stopEvent, overlapped.hEvent};

    while (m_running.load())
    {
        ResetEvent(overlapped.hEvent);

        // Request notifications for file changes
        DWORD bytesReturned = 0;
        BOOL success = ReadDirectoryChangesW(m_dirHandle, buffer.data(), bufferSize,
                                             TRUE,                              // Watch subdirectories
                                             FILE_NOTIFY_CHANGE_FILE_NAME |     // Added/removed
                                                 FILE_NOTIFY_CHANGE_LAST_WRITE, // Modified
                                             &bytesReturned, &overlapped, nullptr);

        if (!success && GetLastError() != ERROR_IO_PENDING)
        {
            LOGF("FileWatcher: ReadDirectoryChangesW failed, error=%lu", GetLastError());
            break;
        }

        // Wait for either stop event or directory change
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0)
        {
            // Stop event signaled
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            // Directory change event
            if (GetOverlappedResult(m_dirHandle, &overlapped, &bytesReturned, FALSE))
            {
                if (bytesReturned > 0)
                {
                    const FILE_NOTIFY_INFORMATION* info =
                        reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data());

                    while (info)
                    {
                        processNotification(info);

                        if (info->NextEntryOffset == 0)
                        {
                            break;
                        }
                        info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const BYTE*>(info) +
                                                                                info->NextEntryOffset);
                    }
                }
            }
        }
        else
        {
            // Error
            LOGF("FileWatcher: WaitForMultipleObjects failed, error=%lu", GetLastError());
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
}

void FileWatcher::processNotification(const FILE_NOTIFY_INFORMATION* info)
{
    // Extract filename (not null-terminated, use length)
    std::wstring filename(info->FileName, info->FileNameLength / sizeof(WCHAR));

    // Only process .json files
    if (filename.size() < 5 || _wcsicmp(filename.c_str() + filename.size() - 5, L".json") != 0)
    {
        return;
    }

    FileAction action;
    const char* actionStr = "unknown";

    switch (info->Action)
    {
    case FILE_ACTION_ADDED:
        action = FileAction::Added;
        actionStr = "added";
        break;
    case FILE_ACTION_REMOVED:
        action = FileAction::Removed;
        actionStr = "removed";
        break;
    case FILE_ACTION_MODIFIED:
        action = FileAction::Modified;
        actionStr = "modified";
        break;
    case FILE_ACTION_RENAMED_OLD_NAME:
        action = FileAction::Removed;
        actionStr = "renamed_from";
        break;
    case FILE_ACTION_RENAMED_NEW_NAME:
        action = FileAction::Added;
        actionStr = "renamed_to";
        break;
    default:
        return; // Unknown action
    }

    LOGF("FileWatcher: %s %ls", actionStr, filename.c_str());

    // Invoke callback
    if (m_callback)
    {
        try
        {
            m_callback(filename, action);
        }
        catch (const std::exception& e)
        {
            LOGF("FileWatcher: Callback exception: %s", e.what());
        }
        catch (...)
        {
            LOG("FileWatcher: Callback threw unknown exception");
        }
    }
}

} // namespace OdbcMonkey
