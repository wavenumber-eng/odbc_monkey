// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// File Watcher for odbc-monkey driver
// Uses ReadDirectoryChangesW to monitor JSON file changes
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace OdbcMonkey
{

enum class FileAction
{
    Added,
    Removed,
    Modified
};

class FileWatcher
{
  public:
    using Callback = std::function<void(const std::wstring& filename, FileAction action)>;

    // Create a file watcher for the given directory
    // callback is invoked for each file change (from a background thread)
    FileWatcher(const std::wstring& directory, Callback callback);
    ~FileWatcher();

    // Start watching (creates background thread)
    bool start();

    // Stop watching (blocks until thread exits)
    void stop();

    // Check if watcher is running
    bool isRunning() const { return m_running.load(); }

    // Get last error message
    const std::wstring& getLastError() const { return m_lastError; }

  private:
    void watchThread();
    void processNotification(const FILE_NOTIFY_INFORMATION* info);

    std::wstring m_directory;
    Callback m_callback;

    HANDLE m_dirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_stopEvent = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::wstring m_lastError;
};

} // namespace OdbcMonkey
