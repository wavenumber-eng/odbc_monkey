// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Wavenumber LLC

#include "logger.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

namespace OdbcMonkey
{

// Get default log path based on DLL location
// DLL is at: <repo>/tools/odbc_monkey/bin/odbc-monkey_XX.dll
// Log path: <repo>/tools/odbc_monkey/debug/odbc_native.log
static std::wstring getDefaultLogPath()
{
    wchar_t dllPath[MAX_PATH];
    HMODULE hModule = NULL;
    // Get handle to this DLL using address of this function
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&getDefaultLogPath), &hModule);
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);

    std::wstring path(dllPath);
    // Go up from bin/ to odbc_monkey/
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        path = path.substr(0, pos); // Remove filename -> bin/
    }
    pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        path = path.substr(0, pos); // Remove bin -> odbc_monkey/
    }
    return path + L"\\debug\\odbc_native.log";
}

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger()
{
    m_logPath = getDefaultLogPath();

    // Initialize Winsock for UDP
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
    {
        m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_udpSocket != INVALID_SOCKET)
        {
            // Set up destination address (localhost:9999)
            m_udpAddr.sin_family = AF_INET;
            m_udpAddr.sin_port = htons(static_cast<u_short>(m_udpPort));
            m_udpAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            m_udpInitialized = true;
        }
    }
}

Logger::~Logger()
{
    if (m_file.is_open())
    {
        m_file.close();
    }
    if (m_udpSocket != INVALID_SOCKET)
    {
        closesocket(m_udpSocket);
        // Skip WSACleanup() here. This destructor runs during DLL unload
        // (static singleton teardown) where Winsock may already be torn down.
        // The OS reclaims all socket resources on process exit regardless.
    }
}

void Logger::setLogPath(const std::wstring& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logPath = path;
    m_initialized = false;
}

void Logger::setUdpPort(int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_udpPort = port;
    if (m_udpInitialized && port > 0)
    {
        m_udpAddr.sin_port = htons(static_cast<u_short>(port));
    }
}

void Logger::setMaxLogSize(size_t bytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxLogSize = bytes;
}

void Logger::setFileLogging(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileLoggingEnabled = enabled;
}

void Logger::ensureInitialized()
{
    if (m_initialized)
        return;

    std::wstring dir = m_logPath.substr(0, m_logPath.find_last_of(L"\\/"));
    CreateDirectoryW(dir.c_str(), nullptr);

    m_file.open(m_logPath, std::ios::out | std::ios::app);
    m_initialized = true;

    // Get current file size
    if (m_file.is_open())
    {
        m_file.seekp(0, std::ios::end);
        m_currentSize = static_cast<size_t>(m_file.tellp());
    }
}

void Logger::checkRotate()
{
    if (m_currentSize < m_maxLogSize)
        return;

    // Close current file
    m_file.close();

    // Rotate files: .log.2 -> .log.3, .log.1 -> .log.2, .log -> .log.1
    for (int i = MAX_LOG_FILES - 1; i >= 0; --i)
    {
        std::wstring oldPath = (i == 0) ? m_logPath : m_logPath + L"." + std::to_wstring(i);
        std::wstring newPath = m_logPath + L"." + std::to_wstring(i + 1);

        // Delete the oldest file if it exists
        if (i == MAX_LOG_FILES - 1)
        {
            DeleteFileW(newPath.c_str());
        }

        // Rename the file
        MoveFileW(oldPath.c_str(), newPath.c_str());
    }

    // Open new log file
    m_file.open(m_logPath, std::ios::out | std::ios::trunc);
    m_currentSize = 0;
}

void Logger::sendUdp(const std::string& message)
{
    if (!m_udpInitialized || m_udpPort == 0)
        return;

    // Fire and forget - don't care if it fails
    sendto(m_udpSocket, message.c_str(), static_cast<int>(message.size()), 0, reinterpret_cast<sockaddr*>(&m_udpAddr),
           sizeof(m_udpAddr));
}

void Logger::log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << " ["
        << GetCurrentThreadId() << "] " << message;

    std::string formatted = oss.str();

    // Write to file (if enabled)
    if (m_fileLoggingEnabled)
    {
        ensureInitialized();
        if (m_file.is_open())
        {
            m_file << formatted << std::endl;
            m_file.flush();
            m_currentSize += formatted.size() + 1; // +1 for newline
            checkRotate();
        }
    }

    // Send via UDP
    sendUdp(formatted);
}

void Logger::log(const std::wstring& message)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    log(utf8);
}

void Logger::logf(const char* format, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(std::string(buffer));
}

} // namespace OdbcMonkey
