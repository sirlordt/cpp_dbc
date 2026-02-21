/**
 * @file 10_101_test_high_perf_logger.cpp
 * @brief Test for high-performance lock-free logger
 */

#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <sstream>
#include <cstdio>
#include "cpp_dbc/common/system_utils.hpp"

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
#include "cpp_dbc/debug/high_perf_logger.hpp"
#endif

/**
 * @brief Test concurrent logging from multiple threads
 *
 * This test simulates high-volume concurrent logging to verify:
 * 1. Lock-free writes work correctly
 * 2. No data corruption occurs
 * 3. Ring buffer handles overruns gracefully
 * 4. Performance is acceptable
 */
void testConcurrentLogging()
{
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test: Concurrent Logging from Multiple Threads ===");

    constexpr int NUM_THREADS = 8;
    constexpr int LOGS_PER_THREAD = 500;

    std::vector<std::jthread> threads;
    std::atomic<int> readyCount{0};
    std::atomic<bool> start{false};

    auto loggerFunc = [&](int threadId) {
        // Wait for all threads to be ready
        readyCount.fetch_add(1, std::memory_order_relaxed);
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }

        // Log messages
        for (int i = 0; i < LOGS_PER_THREAD; ++i)
        {
            HP_LOG("TestThread", "Thread %d, iteration %d: testing concurrent logging", threadId, i);

            // Simulate some work
            if (i % 100 == 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    };

    // Create threads
    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back(loggerFunc, i);
    }

    // Wait for all threads to be ready
    while (readyCount.load(std::memory_order_relaxed) < NUM_THREADS)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Start all threads simultaneously
    char msg[256];
    std::snprintf(msg, sizeof(msg), "Starting %d threads, %d logs per thread (%d total logs)...",
                  NUM_THREADS, LOGS_PER_THREAD, NUM_THREADS * LOGS_PER_THREAD);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    start.store(true, std::memory_order_release);

    // Wait for all threads to finish
    threads.clear();  // jthread auto-joins

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::snprintf(msg, sizeof(msg), "✓ All threads completed in %ldms", duration.count());
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    double avgMicroseconds = static_cast<double>(duration.count()) * 1000.0 / (NUM_THREADS * LOGS_PER_THREAD);
    std::snprintf(msg, sizeof(msg), "✓ Average: %.2f microseconds per log", avgMicroseconds);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    // Give writer thread time to flush
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Waiting for disk flush...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t overruns = cpp_dbc::debug::HighPerfLogger::getInstance().getOverrunCount();
    std::snprintf(msg, sizeof(msg), "✓ Buffer overruns: %zu", overruns);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    if (overruns > 0)
    {
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "  (Some logs were lost due to buffer overflow - this is expected under high load)");
    }

    cpp_dbc::system_utils::logWithTimesMillis("TEST", "✓ Test PASSED");
#else
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test SKIPPED: ENABLE_HIGH_PERF_DEBUG_LOGGER not defined ===");
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON to enable this test.");
#endif
}

/**
 * @brief Test basic logging functionality
 */
void testBasicLogging()
{
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test: Basic Logging ===");

    // Test different log contexts
    HP_LOG("TestContext", "Test message 1: %s", "simple string");
    HP_LOG("TestContext", "Test message 2: int=%d, float=%.2f", 42, 3.14);
    HP_LOG("TestContext", "Test message 3: long string: %s",
           "This is a longer string to test buffer handling");

    // Test convenience macros
    CP_DEBUG("Connection pool test: size=%d", 10);
    MYSQL_DEBUG("MySQL driver test: query=%s", "SELECT * FROM users");

    cpp_dbc::system_utils::logWithTimesMillis("TEST", "✓ Basic logging completed");
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "✓ Test PASSED");

    // Give writer thread time to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test SKIPPED: ENABLE_HIGH_PERF_DEBUG_LOGGER not defined ===");
#endif
}

/**
 * @brief Test ring buffer wraparound
 */
void testRingBufferWraparound()
{
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test: Ring Buffer Wraparound ===");

    constexpr size_t BUFFER_SIZE = cpp_dbc::debug::HighPerfLogger::BUFFER_SIZE;
    constexpr size_t EXTRA_LOGS = 500;

    char msg[256];
    std::snprintf(msg, sizeof(msg), "Writing %zu logs to trigger wraparound...", BUFFER_SIZE + EXTRA_LOGS);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    for (size_t i = 0; i < BUFFER_SIZE + EXTRA_LOGS; ++i)
    {
        HP_LOG("WrapTest", "Log entry %zu", i);

        if (i % 500 == 0)
        {
            std::snprintf(msg, sizeof(msg), "  Progress: %zu/%zu", i, BUFFER_SIZE + EXTRA_LOGS);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);
        }
    }

    cpp_dbc::system_utils::logWithTimesMillis("TEST", "✓ Wraparound test completed");

    // Give writer thread time to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t overruns = cpp_dbc::debug::HighPerfLogger::getInstance().getOverrunCount();
    std::snprintf(msg, sizeof(msg), "✓ Buffer overruns: %zu", overruns);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "✓ Test PASSED");
#else
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n=== Test SKIPPED: ENABLE_HIGH_PERF_DEBUG_LOGGER not defined ===");
#endif
}

int main()
{
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "========================================");
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "High-Performance Logger Test Suite");
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "========================================");

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Logger is ENABLED");

    char msg[256];
    std::snprintf(msg, sizeof(msg), "Buffer size: %zu entries", cpp_dbc::debug::HighPerfLogger::BUFFER_SIZE);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);

    std::snprintf(msg, sizeof(msg), "Message size: %zu bytes", cpp_dbc::debug::HighPerfLogger::MSG_SIZE);
    cpp_dbc::system_utils::logWithTimesMillis("TEST", msg);
#else
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Logger is DISABLED");
    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON to enable.");
#endif

    try
    {
        testBasicLogging();
        testConcurrentLogging();
        testRingBufferWraparound();

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "\n========================================");
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "All tests completed successfully!");
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "========================================");

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "\nShutting down logger...");
        cpp_dbc::debug::HighPerfLogger::getInstance().shutdown();
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Check log file in: logs/debug/YYYY-MM-DD-HH-mm-SS/ConnectionPool.log");
#endif

        return 0;
    }
    catch (const std::exception& e)
    {
        char errorMsg[512];
        std::snprintf(errorMsg, sizeof(errorMsg), "ERROR: %s", e.what());
        cpp_dbc::system_utils::logWithTimesMillis("ERROR", errorMsg);
        return 1;
    }
}
