// SPDX-License-Identifier: MIT
// Copyright (c) 2025 cpp_dbc Project
// High-Performance Lock-Free Ring Buffer Logger

#pragma once

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER

#include <atomic>
#include <thread>
#include <array>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <string_view>

namespace cpp_dbc::debug
{

/**
 * @brief High-performance lock-free ring buffer logger for debugging race conditions
 *
 * Design goals:
 * - Lock-free write path (only atomic operations + snprintf)
 * - Ring buffer with automatic overwrite when full
 * - Asynchronous disk I/O in separate thread
 * - Minimal overhead (~50-100ns per log entry)
 * - Thread-safe without mutexes
 *
 * Usage:
 *   CP_DEBUG("Connection released: conn=%p, available=%zu", conn.get(), count);
 *
 * Output format:
 *   [Context] [HH:mm:ss.mmm] [tid:XXXXX] [file:line] message
 *
 * Note: This logger is designed for debugging Helgrind/ThreadSanitizer issues
 * where normal stdout/stderr logging introduces synchronization that hides bugs.
 */
class HighPerfLogger
{
public:
    static constexpr size_t BUFFER_SIZE = 10000;  ///< Ring buffer size
    static constexpr size_t MSG_SIZE = 1024;      ///< Max message size per entry

    /**
     * @brief Single log entry in the ring buffer
     */
    struct LogEntry
    {
        char message[MSG_SIZE];          ///< Pre-allocated message buffer // NOSONAR(cpp:S5945) — fixed-size char array is intentional: zero-allocation ring buffer; std::string would allocate on every log call
        size_t length{0};                ///< Actual message length
        std::atomic<bool> ready{false};  ///< True when entry is ready to write to disk
    };

    /**
     * @brief Get singleton instance
     */
    static HighPerfLogger& getInstance()
    {
        static HighPerfLogger instance; // NOSONAR(cpp:S6018) — Meyers singleton (local static); S6018 applies to namespace-scope globals, not local statics
        return instance;
    }

    /**
     * @brief Log a message (lock-free, thread-safe)
     *
     * @param context Context name (e.g., "ConnectionPool")
     * @param file Source file name (__FILE__)
     * @param line Source line number (__LINE__)
     * @param message Message string to log
     */
    void log(const char* context, const char* file, int line,
             std::string_view message);

    /**
     * @brief Shutdown logger and flush remaining logs
     */
    void shutdown();

    /**
     * @brief Get total number of logs lost due to buffer overruns
     */
    size_t getOverrunCount() const
    {
        return m_overrunCount.load(std::memory_order_relaxed);
    }

    // Prevent copying
    HighPerfLogger(const HighPerfLogger&) = delete;
    HighPerfLogger& operator=(const HighPerfLogger&) = delete;

private:
    HighPerfLogger();
    ~HighPerfLogger();

    /**
     * @brief Writer thread function (async disk I/O)
     */
    void writerThreadFunc(std::stop_token stopToken);

    std::array<LogEntry, BUFFER_SIZE> m_buffer;  ///< Ring buffer of log entries

    // Ring buffer indices (advance infinitely, wrap via % BUFFER_SIZE)
    std::atomic<size_t> m_writeIndex{0};  ///< Producer index (writers)
    std::atomic<size_t> m_readIndex{0};   ///< Consumer index (disk writer thread)

    std::atomic<size_t> m_overrunCount{0};  ///< Count of logs lost to overruns
    std::atomic<bool> m_running{true};      ///< Running flag

    std::jthread m_writerThread;           ///< Async writer thread (C++20)
    std::ofstream m_logFile;               ///< Log file stream
    std::filesystem::path m_logPath;       ///< Log directory path
    std::filesystem::path m_logFilePath;   ///< Full path to log file
};

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Generic high-performance log macro
 *
 * @param context Context name (e.g., "ConnectionPool", "MySQLDriver")
 * @param msg Message string (std::string, std::string_view, or const char*)
 */
#define HP_LOG(context, msg) \
    cpp_dbc::debug::HighPerfLogger::getInstance().log( \
        context, __FILE__, __LINE__, msg) // NOSONAR(cpp:S6190) — __FILE__/__LINE__ cannot be replaced with std::source_location while keeping macro call-site capture semantics

/**
 * @brief Connection pool debug macro
 */
#define CP_DEBUG(msg) HP_LOG("ConnectionPool", msg)

/**
 * @brief MySQL driver debug macro
 */
#define MYSQL_DEBUG(msg) HP_LOG("MySQL", msg)

/**
 * @brief PostgreSQL driver debug macro
 */
#define PGSQL_DEBUG(msg) HP_LOG("PostgreSQL", msg)

/**
 * @brief SQLite driver debug macro
 */
#define SQLITE_DEBUG(msg) HP_LOG("SQLite", msg)

} // namespace cpp_dbc::debug

#else

// ============================================================================
// No-op macros when logger is disabled (zero overhead)
// ============================================================================

#define HP_LOG(context, fmt, ...) ((void)0)
#define CP_DEBUG(fmt, ...) ((void)0)
#define MYSQL_DEBUG(fmt, ...) ((void)0)
#define PGSQL_DEBUG(fmt, ...) ((void)0)
#define SQLITE_DEBUG(fmt, ...) ((void)0)

#endif // ENABLE_HIGH_PERF_DEBUG_LOGGER
