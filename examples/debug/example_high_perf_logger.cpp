/**
 * @file example_high_perf_logger.cpp
 * @brief Example demonstrating usage of high-performance logger
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
#include "cpp_dbc/debug/high_perf_logger.hpp"
#endif

/**
 * @brief Simulate connection pool operations
 */
void simulateConnectionPool(int threadId, int operations)
{
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    for (int i = 0; i < operations; ++i)
    {
        // Simulate acquiring connection
        CP_DEBUG("Thread %d acquiring connection (op %d/%d)", threadId, i + 1, operations);
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // Simulate using connection
        CP_DEBUG("Thread %d executing query on connection", threadId);
        std::this_thread::sleep_for(std::chrono::microseconds(200));

        // Simulate releasing connection
        CP_DEBUG("Thread %d releasing connection", threadId);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
#else
    (void)threadId;
    (void)operations;
    std::cout << "Thread " << threadId << ": Logger disabled\n";
#endif
}

/**
 * @brief Simulate MySQL driver operations
 */
void simulateMySQLDriver(int queryId)
{
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    MYSQL_DEBUG("Preparing statement for query %d", queryId);
    std::this_thread::sleep_for(std::chrono::microseconds(50));

    MYSQL_DEBUG("Binding parameters for query %d", queryId);
    std::this_thread::sleep_for(std::chrono::microseconds(30));

    MYSQL_DEBUG("Executing query %d", queryId);
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    MYSQL_DEBUG("Fetching results for query %d", queryId);
    std::this_thread::sleep_for(std::chrono::microseconds(80));

    MYSQL_DEBUG("Finalizing query %d", queryId);
#else
    (void)queryId;
#endif
}

/**
 * @brief Example 1: Single-threaded logging
 */
void example1_SingleThreaded()
{
    std::cout << "\n=== Example 1: Single-Threaded Logging ===\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    std::cout << "Logging 100 messages from a single thread...\n";

    for (int i = 0; i < 100; ++i)
    {
        HP_LOG("Example1", "Message %d: single-threaded test", i);

        if (i % 25 == 0)
        {
            std::cout << "  Progress: " << i << "/100\n";
        }
    }

    std::cout << "✓ Completed\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    std::cout << "Logger is disabled. Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON\n";
#endif
}

/**
 * @brief Example 2: Multi-threaded connection pool simulation
 */
void example2_MultiThreaded()
{
    std::cout << "\n=== Example 2: Multi-Threaded Connection Pool ===\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 10;

    std::cout << "Simulating " << NUM_THREADS << " threads accessing connection pool...\n";

    std::vector<std::jthread> threads;
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back(simulateConnectionPool, i, OPS_PER_THREAD);
    }

    threads.clear();  // Wait for all threads to finish

    std::cout << "✓ Completed\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    std::cout << "Logger is disabled. Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON\n";
#endif
}

/**
 * @brief Example 3: Mixed context logging
 */
void example3_MixedContexts()
{
    std::cout << "\n=== Example 3: Mixed Context Logging ===\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    std::cout << "Logging from different contexts (ConnectionPool, MySQL, PostgreSQL)...\n";

    for (int i = 0; i < 20; ++i)
    {
        CP_DEBUG("Connection pool operation %d", i);
        MYSQL_DEBUG("MySQL query %d: SELECT * FROM users WHERE id=%d", i, i * 10);
        PGSQL_DEBUG("PostgreSQL transaction %d started", i);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "✓ Completed\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    std::cout << "Logger is disabled. Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON\n";
#endif
}

/**
 * @brief Example 4: High-volume logging (stress test)
 */
void example4_HighVolume()
{
    std::cout << "\n=== Example 4: High-Volume Logging (Stress Test) ===\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    constexpr int NUM_THREADS = 8;
    constexpr int QUERIES_PER_THREAD = 100;

    std::cout << "Stress test: " << NUM_THREADS << " threads, "
              << QUERIES_PER_THREAD << " operations each...\n";

    auto startTime = std::chrono::steady_clock::now();

    std::vector<std::jthread> threads;
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back([i]() {
            for (int j = 0; j < QUERIES_PER_THREAD; ++j)
            {
                simulateMySQLDriver(i * QUERIES_PER_THREAD + j);
            }
        });
    }

    threads.clear();  // Wait for all threads

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "✓ Completed in " << duration.count() << "ms\n";
    std::cout << "✓ Throughput: "
              << ((NUM_THREADS * QUERIES_PER_THREAD * 1000) / duration.count())
              << " operations/sec\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t overruns = cpp_dbc::debug::HighPerfLogger::getInstance().getOverrunCount();
    if (overruns > 0)
    {
        std::cout << "⚠ Warning: " << overruns << " logs were lost due to buffer overruns\n";
        std::cout << "  (Consider increasing BUFFER_SIZE or reducing log frequency)\n";
    }
    else
    {
        std::cout << "✓ No buffer overruns\n";
    }
#else
    std::cout << "Logger is disabled. Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON\n";
#endif
}

/**
 * @brief Example 5: Custom context logging
 */
void example5_CustomContext()
{
    std::cout << "\n=== Example 5: Custom Context Logging ===\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    std::cout << "Demonstrating custom contexts...\n";

    // Custom contexts for different components
    HP_LOG("TransactionMgr", "Beginning transaction txn_001");
    HP_LOG("TransactionMgr", "Acquired lock on table: users");
    HP_LOG("QueryOptimizer", "Optimizing query: SELECT * FROM users WHERE age > 25");
    HP_LOG("QueryOptimizer", "Using index: idx_users_age");
    HP_LOG("CacheMgr", "Cache miss for key: user_123");
    HP_LOG("CacheMgr", "Inserting into cache: user_123");
    HP_LOG("TransactionMgr", "Committing transaction txn_001");

    std::cout << "✓ Completed\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#else
    std::cout << "Logger is disabled. Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON\n";
#endif
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "High-Performance Logger Examples\n";
    std::cout << "========================================\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
    std::cout << "Logger: ENABLED\n";
    std::cout << "Buffer size: " << cpp_dbc::debug::HighPerfLogger::BUFFER_SIZE << " entries\n";
    std::cout << "Message size: " << cpp_dbc::debug::HighPerfLogger::MSG_SIZE << " bytes\n";
#else
    std::cout << "Logger: DISABLED\n";
    std::cout << "Build with -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON to see logs\n";
#endif

    try
    {
        // Run examples
        example1_SingleThreaded();
        example2_MultiThreaded();
        example3_MixedContexts();
        example4_HighVolume();
        example5_CustomContext();

        std::cout << "\n========================================\n";
        std::cout << "All examples completed!\n";
        std::cout << "========================================\n";

#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
        std::cout << "\nShutting down logger and flushing remaining logs...\n";
        cpp_dbc::debug::HighPerfLogger::getInstance().shutdown();

        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&time_t_val, &tm);

        char timestamp[64];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", &tm);

        std::cout << "\n✓ Log file written to:\n";
        std::cout << "  logs/debug/" << timestamp << "/ConnectionPool.log\n";
        std::cout << "\nYou can view the log with:\n";
        std::cout << "  cat logs/debug/*/ConnectionPool.log | tail -100\n";
        std::cout << "  grep 'Example1' logs/debug/*/ConnectionPool.log\n";
        std::cout << "  grep 'OVERRUN' logs/debug/*/ConnectionPool.log\n";
#endif

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
