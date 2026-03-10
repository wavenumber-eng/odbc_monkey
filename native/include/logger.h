// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC
//
// Logger for odbc-monkey driver
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>

#include <fstream>
#include <mutex>
#include <string>

namespace OdbcMonkey
{

class Logger
{
  public:
    static Logger& instance();

    void setLogPath(const std::wstring& path);
    void setUdpPort(int port);         // 0 = disabled (default: 9999)
    void setMaxLogSize(size_t bytes);  // Default: 5 MB
    void setFileLogging(bool enabled); // Default: true
    void log(const std::string& message);
    void log(const std::wstring& message);
    void logf(const char* format, ...);

  private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void ensureInitialized();
    void sendUdp(const std::string& message);
    void checkRotate();

    std::wstring m_logPath;
    std::ofstream m_file;
    std::mutex m_mutex;
    bool m_initialized = false;
    size_t m_currentSize = 0;

    // Log rotation settings
    size_t m_maxLogSize = 5 * 1024 * 1024;  // 5 MB default
    static constexpr int MAX_LOG_FILES = 3; // Keep 3 backup files
    bool m_fileLoggingEnabled = true;

    // UDP broadcast
    SOCKET m_udpSocket = INVALID_SOCKET;
    sockaddr_in m_udpAddr = {};
    int m_udpPort = 9999;
    bool m_udpInitialized = false;
};

} // namespace OdbcMonkey

// Convenience macros
#define LOG(msg) OdbcMonkey::Logger::instance().log(msg)
#define LOGF(...) OdbcMonkey::Logger::instance().logf(__VA_ARGS__)
