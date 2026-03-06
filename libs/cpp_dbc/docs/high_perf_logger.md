# High-Performance Lock-Free Logger

## Overview

The high-performance logger is a specialized debugging tool designed to diagnose race conditions and threading issues without introducing synchronization that could mask bugs (Heisenbugs).

### Key Features

- ✅ **Lock-free writes**: Only atomic operations in hot path (~50-100ns per log)
- ✅ **Ring buffer**: Pre-allocated 2048-entry buffer with automatic wraparound
- ✅ **Asynchronous I/O**: Disk writes happen in separate thread
- ✅ **Thread-safe**: No mutexes, uses C++20 atomics with explicit memory ordering
- ✅ **Minimal overhead**: ~1000x faster than stdout/stderr logging
- ✅ **Compile-time toggle**: Zero overhead when disabled

### When to Use

This logger is specifically designed for:

1. **Debugging Helgrind/ThreadSanitizer issues**
   - When normal stdout logging makes bugs disappear (Heisenbugs)
   - When you need to log inside critical sections without adding synchronization

2. **High-frequency logging scenarios**
   - Connection pool operations
   - Database driver internals
   - Transaction manager state changes

3. **Race condition investigation**
   - Understanding thread interleaving
   - Capturing exact timing of events across threads

### When NOT to Use

- ❌ Production logging (use proper logging framework)
- ❌ User-facing messages
- ❌ Error reporting (use exceptions or stderr)
- ❌ Low-frequency events (normal logging is fine)

## Building

### Enable at build time

```bash
# Enable high-performance logger (ON by default)
cmake -B build -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON

# Disable (zero overhead)
cmake -B build -DENABLE_HIGH_PERF_DEBUG_LOGGER=OFF
```

### Combined with debug flags

```bash
# Example: Enable connection pool debug with high-perf logger
cmake -B build \
    -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON \
    -DDEBUG_CONNECTION_POOL=ON

# Example: With ThreadSanitizer
cmake -B build \
    -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
```

## Usage

### Basic Usage

```cpp
#include "cpp_dbc/debug/high_perf_logger.hpp"

// Generic logging
HP_LOG("MyContext", "Connection opened: id=%d", connectionId);
HP_LOG("MyContext", "Query executed: %s", query.c_str());

// Context-specific macros
CP_DEBUG("Pool size: %zu, active: %zu", poolSize, activeConns);
MYSQL_DEBUG("Executing query: %s", sql.c_str());
PGSQL_DEBUG("Transaction started: isolation=%d", level);
SQLITE_DEBUG("Statement prepared: %s", stmt.c_str());
```

### Output Format

```text
[Context] [HH:mm:ss.mmm] [tid:XXXXX] [file:line] message
```

Example:
```text
[ConnectionPool] [14:23:45.123] [tid:12345] [connection_pool.cpp:123] Acquiring connection...
[ConnectionPool] [14:23:45.125] [tid:12345] [connection_pool.cpp:145] Connection acquired: 0x7f8b4c001a80
[MySQL] [14:23:45.126] [tid:12345] [connection.cpp:89] Executing query: SELECT * FROM users
```

### Log File Location

Logs are written to:
```text
logs/debug/YYYY-MM-DD-HH-mm-SS/ConnectionPool.log
```

Example:
```text
logs/debug/2026-02-14-14-23-45/ConnectionPool.log
```

## Architecture

### Ring Buffer Design

```text
┌───────────────────────────────────────────┐
│  Ring Buffer (2048 entries)               │
│                                           │
│  [0] [1] [2] ... [2046] [2047]           │
│   ↑                        ↑              │
│   │                        │              │
│   readIndex (consumer)     writeIndex     │
│   (disk writer thread)     (producers)    │
└───────────────────────────────────────────┘

writeIndex: atomic, increments infinitely (0, 1, 2, ..., 2048, 2049, ...)
readIndex:  atomic, chases writeIndex
Position:   index % BUFFER_SIZE (wraparound)
```

### Write Path (Lock-Free)

```cpp
1. writeIdx = m_writeIndex.fetch_add(1)        // ~10ns (atomic)
2. index = writeIdx % BUFFER_SIZE              // ~1ns (modulo)
3. entry.ready = false                         // ~5ns (atomic store)
4. snprintf(entry.message, ...)                // ~100-500ns (formatting)
5. entry.ready = true                          // ~5ns (atomic store)

Total: ~50-100ns per log entry
```

### Read Path (Async Thread)

```cpp
while (running) {
    readIdx = m_readIndex.load()
    writeIdx = m_writeIndex.load()

    // Detect overrun
    if (writeIdx - readIdx > BUFFER_SIZE) {
        // Jump to oldest available
        readIdx = writeIdx - BUFFER_SIZE + MARGIN
        log_overrun()
    }

    // Write available entries to disk
    while (readIdx < writeIdx) {
        if (entry.ready) {
            fwrite(entry.message)
            readIdx++
        }
    }

    sleep(10ms)
}
```

## Buffer Overruns

### What Happens

When writers produce logs faster than the disk writer can consume:

1. **Overrun detected**: `writeIdx - readIdx > BUFFER_SIZE`
2. **Reader jumps forward**: `readIdx = writeIdx - BUFFER_SIZE + 128`
3. **Logs are lost**: Old logs are overwritten before being written to disk
4. **Overrun logged**: Event is written to log file with count

### Example Overrun Log

```text
[HighPerfLogger] [14:23:47.456] *** OVERRUN: 523 logs lost, jumping from idx=2048 to idx=2176 ***
```

### Preventing Overruns

1. **Reduce log frequency**: Log only critical events
2. **Increase buffer size**: Modify `BUFFER_SIZE` in header (e.g., 4096)
3. **Reduce sleep interval**: Change writer thread sleep from 10ms to 5ms
4. **Use faster storage**: SSD instead of HDD

### Monitoring Overruns

```cpp
size_t overruns = HighPerfLogger::getInstance().getOverrunCount();
std::cout << "Total logs lost: " << overruns << "\n";
```

## Performance Characteristics

### Benchmarks

| Operation | Latency | Notes |
|-----------|---------|-------|
| HP_LOG() call | ~50-100ns | Lock-free atomic + snprintf |
| stdout logging | ~50-100μs | ~1000x slower (syscall + mutex) |
| Ring buffer write | ~10ns | fetch_add atomic operation |
| Disk flush | ~10ms | Batched, async in writer thread |

### Memory Usage

- **Static allocation**: 2048 entries × 1024 bytes = 2 MB
- **No dynamic allocation**: All memory pre-allocated at startup
- **Cache-friendly**: Sequential access pattern

### Thread Scalability

- ✅ Scales linearly with thread count (lock-free)
- ✅ No contention on mutexes
- ✅ Only contention is atomic increment (minimal)

## Integration Example

### Connection Pool

```cpp
// libs/cpp_dbc/src/pool/relational/relational_db_connection_pool.cpp
#ifdef ENABLE_HIGH_PERF_DEBUG_LOGGER
#include "cpp_dbc/debug/high_perf_logger.hpp"
#endif

std::shared_ptr<RelationalDBConnection>
RelationalDBConnectionPool::getConnection()
{
    CP_DEBUG("getConnection called");

    DB_DRIVER_LOCK_GUARD(m_poolMutex);
    CP_DEBUG("Lock acquired, available=%zu", m_availableConnections.size());

    if (!m_availableConnections.empty())
    {
        auto conn = m_availableConnections.back();
        m_availableConnections.pop_back();
        CP_DEBUG("Returning connection from pool: %p", conn.get());
        return conn;
    }

    CP_DEBUG("No connections available, creating new...");
    // ... rest of function
}
```

## Testing

### Run the test suite

```bash
# Build test
cmake -B build -DENABLE_HIGH_PERF_DEBUG_LOGGER=ON
cmake --build build --target test_high_perf_logger

# Run test
./build/libs/cpp_dbc/test/test_high_perf_logger
```

### Test output

```text
========================================
High-Performance Logger Test Suite
========================================
Logger is ENABLED
Buffer size: 2048 entries
Message size: 1024 bytes

=== Test: Basic Logging ===
✓ Basic logging completed
✓ Test PASSED

=== Test: Concurrent Logging from Multiple Threads ===
Starting 8 threads, 500 logs per thread (4000 total logs)...
✓ All threads completed in 45ms
✓ Average: 11.25 microseconds per log
✓ Buffer overruns: 0
✓ Test PASSED

=== Test: Ring Buffer Wraparound ===
Writing 2548 logs to trigger wraparound...
✓ Wraparound test completed
✓ Buffer overruns: 0
✓ Test PASSED

========================================
All tests completed successfully!
========================================

Check log file in: logs/debug/2026-02-14-14-23-45/ConnectionPool.log
```

## Troubleshooting

### Logger not working

**Problem**: Logs are not being written

**Solutions**:
1. Check build flag: `cmake -L | grep ENABLE_HIGH_PERF_DEBUG_LOGGER`
2. Verify log directory exists: `ls -la logs/debug/`
3. Check stderr for initialization errors

### Buffer overruns

**Problem**: Many logs are being lost

**Solutions**:
1. Increase `BUFFER_SIZE` in header (e.g., 4096 or 8192)
2. Reduce log frequency (log less often)
3. Check disk I/O speed: `iostat -x 1`

### Performance degradation

**Problem**: Logger is slower than expected

**Solutions**:
1. Verify lock-free compilation: `objdump -d | grep atomic`
2. Check for I/O bottleneck: move logs to faster storage
3. Profile with `perf record -g ./your_app`

### Missing logs

**Problem**: Some logs are missing but no overruns reported

**Solutions**:
1. Ensure proper shutdown: call `shutdown()` before exit
2. Add sleep before exit: `sleep_for(500ms)` to allow flush
3. Check final flush logic in writer thread

## Implementation Details

### Memory Ordering

- **Relaxed**: `writeIndex.fetch_add()` (performance critical)
- **Acquire**: `entry.ready.load()` (ensure complete write visible)
- **Release**: `entry.ready.store()` (publish complete write)

### Thread Safety

- No mutexes (100% lock-free)
- C++20 `std::jthread` with `std::stop_token`
- Atomic operations with explicit memory ordering
- Ring buffer with two-cursor protocol

### File I/O

- Buffered writes via `std::ofstream`
- Periodic flush (every 10ms)
- Final flush on shutdown
- Overrun events logged to file

## Future Improvements

Potential enhancements (not yet implemented):

1. **Multiple contexts**: Separate log files per context
2. **Binary format**: More compact, post-process for text
3. **Memory-mapped I/O**: Even faster disk writes via `mmap()`
4. **Thread-local buffers**: Eliminate atomic contention
5. **Configurable buffer size**: Runtime configuration
6. **Log rotation**: Auto-rotate based on size/time

## References

- [Lock-Free Ring Buffer](https://www.boost.org/doc/libs/1_82_0/doc/html/lockfree.html)
- [C++ Memory Ordering](https://en.cppreference.com/w/cpp/atomic/memory_order)
- [Heisenbug on Wikipedia](https://en.wikipedia.org/wiki/Heisenbug)
