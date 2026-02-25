// SPDX-License-Identifier: MIT
// Copyright (c) 2025 cpp_dbc Project

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER

#include "cpp_dbc/debug/high_perf_logger.hpp"
#include <iomanip>
#include <sstream>

namespace cpp_dbc::debug
{

HighPerfLogger::HighPerfLogger()
{
    // Pre-initialize all buffer entries
    for (auto& entry : m_buffer)
    {
        entry.ready.store(false, std::memory_order_relaxed);
        entry.length = 0;
        std::memset(entry.message, 0, MSG_SIZE);
    }

    // Create log directory: logs/debug/YYYY-MM-DD-HH-mm-SS/
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time_t_val, &tm);

    std::ostringstream oss;
    oss << "logs/debug/"
        << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S");

    m_logPath = std::filesystem::path(oss.str());

    try
    {
        std::filesystem::create_directories(m_logPath);
    }
    catch (const std::exception &ex)
    {
        // NOSONAR(cpp:S6494) — std::print unavailable in GCC 11; fprintf to stderr is the only
        // safe option here since the logger itself is not yet operational.
        std::fprintf(stderr, "HighPerfLogger: Failed to create log directory: %s - %s\n", // NOSONAR(cpp:S6494)
                     m_logPath.c_str(), ex.what());
        return;
    }

    // Open log file
    m_logFilePath = m_logPath / "ConnectionPool.log";
    m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);

    if (!m_logFile.is_open())
    {
        std::fprintf(stderr, "HighPerfLogger: Failed to open log file: %s\n", // NOSONAR(cpp:S6494) — same reason as above
                     m_logFilePath.c_str());
        return;
    }

    // Write header
    m_logFile << "=== HighPerfLogger Started: "
              << std::setfill('0')
              << std::setw(4) << (tm.tm_year + 1900) << "-"
              << std::setw(2) << (tm.tm_mon + 1) << "-"
              << std::setw(2) << tm.tm_mday << " "
              << std::setw(2) << tm.tm_hour << ":"
              << std::setw(2) << tm.tm_min << ":"
              << std::setw(2) << tm.tm_sec
              << " ===\n";
    m_logFile.flush();

    // Start writer thread (C++20 jthread with stop_token)
    m_writerThread = std::jthread([this](std::stop_token stopToken) {
        writerThreadFunc(stopToken);
    });

    std::fprintf(stderr, "HighPerfLogger: Initialized, log file: %s\n", // NOSONAR(cpp:S6494) — std::print unavailable in GCC 11; emergency stderr output before logger is ready
                 m_logFilePath.c_str());
}

HighPerfLogger::~HighPerfLogger()
{
    shutdown();
}

void HighPerfLogger::log(const char* context, const char* file, int line,
                         std::string_view message)
{
    // Atomic fetch_add: lock-free, ultra fast (~10ns)
    size_t writeIdx = m_writeIndex.fetch_add(1, std::memory_order_relaxed);
    size_t index = writeIdx % BUFFER_SIZE;  // Ring buffer: wraps around

    LogEntry& entry = m_buffer[index];

    // CRITICAL: Mark as "not ready" before writing
    // This prevents the reader from reading partial data
    entry.ready.store(false, std::memory_order_release);

    // Format timestamp (HH:mm:ss.mmm)
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_r(&time_t_now, &tm);

    // Get thread ID hash (for readability)
    auto tid = std::this_thread::get_id();
    size_t tid_hash = std::hash<std::thread::id>{}(tid);

    // Extract filename from full path
    const char* filename = std::strrchr(file, '/');
    filename = filename ? filename + 1 : file;

    // Format header: [Context] [HH:mm:ss.mmm] [tid:XXXXX] [file:line]
    // snprintf writes into a fixed pre-allocated ring buffer entry; std::format is unavailable in GCC 11.
    int offset = std::snprintf(entry.message, MSG_SIZE, // NOSONAR(cpp:S6494)
        "[%s] [%02d:%02d:%02d.%03ld] [tid:%zu] [%s:%d] ",
        context,
        tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long>(ms.count()),
        tid_hash,
        filename, line);

    if (offset < 0 || offset >= static_cast<int>(MSG_SIZE))
    {
        entry.ready.store(false, std::memory_order_release);
        return;
    }

    // Copy user message directly into the ring buffer entry — zero allocation
    size_t remaining = MSG_SIZE - static_cast<size_t>(offset) - 2; // reserve 2: '\n' + '\0'
    size_t copyLen = std::min(message.size(), remaining);
    std::memcpy(entry.message + offset, message.data(), copyLen);

    // Calculate final length and add newline
    entry.length = static_cast<size_t>(offset) + copyLen;
    entry.message[entry.length] = '\n';
    entry.length++;
    entry.message[entry.length] = '\0';

    // CRITICAL: Mark as ready AFTER all writes are complete
    // Release semantics ensures all previous writes are visible to reader
    entry.ready.store(true, std::memory_order_release);
}

void HighPerfLogger::writerThreadFunc(std::stop_token stopToken)
{
    constexpr size_t SAFETY_MARGIN = 128;  // Safety margin for overrun recovery

    while (!stopToken.stop_requested() && m_running.load(std::memory_order_acquire))
    {
        size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
        size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

        // Detect overrun: writer advanced more than BUFFER_SIZE ahead of reader
        if (writeIdx - readIdx > BUFFER_SIZE)
        {
            // Reader fell behind, old logs were overwritten
            size_t logsLost = writeIdx - readIdx - BUFFER_SIZE;
            m_overrunCount.fetch_add(logsLost, std::memory_order_relaxed);

            // Jump to oldest available log (with safety margin)
            readIdx = writeIdx - BUFFER_SIZE + SAFETY_MARGIN;
            m_readIndex.store(readIdx, std::memory_order_release);

            // Log the overrun event.
            // Stack buffer used intentionally: runs under memory pressure (overrun condition);
            // heap allocation via std::string could fail exactly when needed. std::format unavailable in GCC 11.
            char overrunMsg[512]; // NOSONAR(cpp:S5945)
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&time_t_now, &tm);

            std::snprintf(overrunMsg, sizeof(overrunMsg), // NOSONAR(cpp:S6494)
                         "[HighPerfLogger] [%02d:%02d:%02d] *** OVERRUN: %zu logs lost, "
                         "jumping from idx=%zu to idx=%zu ***\n",
                         tm.tm_hour, tm.tm_min, tm.tm_sec,
                         logsLost,
                         readIdx - (writeIdx - BUFFER_SIZE + SAFETY_MARGIN),
                         readIdx);
            m_logFile << overrunMsg;
        }

        // Process all available entries
        bool wroteAny = false;
        while (readIdx < writeIdx)
        {
            size_t index = readIdx % BUFFER_SIZE;
            LogEntry& entry = m_buffer[index];

            // Acquire semantics: ensures we see the complete write
            if (entry.ready.load(std::memory_order_acquire))
            {
                // Write to disk
                m_logFile.write(entry.message, static_cast<std::streamsize>(entry.length));

                // Mark as consumed
                entry.ready.store(false, std::memory_order_release);

                readIdx++;
                m_readIndex.store(readIdx, std::memory_order_release);
                wroteAny = true;
            }
            else
            {
                // Not ready yet, wait for next iteration
                break;
            }
        }

        // Flush only occasionally (every 10 iterations) to reduce I/O blocking
        // Increment unconditionally so the counter advances regardless of wroteAny,
        // then flush only when something was written on a flush-due iteration.
        ++m_flushCounter;
        if (wroteAny && (m_flushCounter % 10 == 0))
        {
            m_logFile.flush();
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // FINAL FLUSH: Drain remaining logs before shutdown
    size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
    size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

    // Limit to BUFFER_SIZE to avoid reading garbage
    if (writeIdx - readIdx > BUFFER_SIZE)
    {
        readIdx = writeIdx - BUFFER_SIZE;
    }

    // Flush remaining logs
    while (readIdx < writeIdx)
    {
        size_t index = readIdx % BUFFER_SIZE;
        const LogEntry& entry = m_buffer[index]; // const: entry is only read here, never modified
        if (entry.ready.load(std::memory_order_acquire))
        {
            m_logFile.write(entry.message, static_cast<std::streamsize>(entry.length));
        }
        readIdx++;
    }

    // Report total overruns
    size_t overruns = m_overrunCount.load(std::memory_order_relaxed);
    if (overruns > 0)
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&time_t_now, &tm);

        m_logFile << "[HighPerfLogger] ["
                  << std::setfill('0')
                  << std::setw(2) << tm.tm_hour << ":"
                  << std::setw(2) << tm.tm_min << ":"
                  << std::setw(2) << tm.tm_sec
                  << "] === Shutdown: " << overruns
                  << " logs lost due to buffer overruns ===\n";
    }

    // Write footer
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time_t_now, &tm);

    m_logFile << "=== HighPerfLogger Stopped: "
              << std::setfill('0')
              << std::setw(4) << (tm.tm_year + 1900) << "-"
              << std::setw(2) << (tm.tm_mon + 1) << "-"
              << std::setw(2) << tm.tm_mday << " "
              << std::setw(2) << tm.tm_hour << ":"
              << std::setw(2) << tm.tm_min << ":"
              << std::setw(2) << tm.tm_sec
              << " ===\n";

    // Close file
    m_logFile.close();
}

void HighPerfLogger::shutdown()
{
    m_running.store(false, std::memory_order_release);

    if (m_writerThread.joinable())
    {
        m_writerThread.request_stop();
        m_writerThread.join();
    }
}

} // namespace cpp_dbc::debug

#endif // ENABLE_HIGH_PERF_DEBUG_LOGGER
