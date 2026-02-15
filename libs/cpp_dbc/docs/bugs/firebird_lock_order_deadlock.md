# Firebird Lock Order Violation - Potential Deadlock

**Status**: 🔴 OPEN - Not fixed yet
**Severity**: HIGH - Can cause deadlock in production
**Detected by**: Helgrind (Valgrind) during test execution
**Date Found**: 2026-02-14
**Test**: `23_031_test_firebird_real.cpp`
**Log**: `logs/test/2026-02-14-18-48-18/23_RUN01_fail.log`

---

## Executive Summary

**Lock order violation detected between two mutexes in `FirebirdDBConnection`**, creating a potential deadlock scenario. This is a **real concurrency bug**, not a Helgrind false positive.

**Impact**: In multi-threaded environments (especially with connection pools), this can cause the application to deadlock when:
1. One thread is executing a query (locks `m_connMutex` → `m_activeResultSetsMutex`)
2. Another thread is closing active result sets (locks `m_activeResultSetsMutex` → `m_connMutex`)

---

## Technical Details

### Locks Involved

1. **`m_connMutex`** (`std::recursive_mutex` at address `0x8A66640`)
   - **Purpose**: Protects the Firebird connection state
   - **Scope**: Global to the connection object
   - **File**: Declared in `FirebirdDBConnection` class

2. **`m_activeResultSetsMutex`** (`std::mutex` at address `0x8A7E750`)
   - **Purpose**: Protects the list of active ResultSet objects
   - **Scope**: Specific to ResultSet tracking
   - **File**: Declared in `FirebirdDBConnection` class

### Lock Order Violation

Helgrind detected two **conflicting lock acquisition orders**:

#### Scenario A: Established Order (Correct)
```
Thread executing query:
  FirebirdDBConnection::executeQuery() [connection_02.cpp:251]
    → Acquires m_connMutex (0x8A66640)                    ← FIRST
      → FirebirdDBPreparedStatement::executeQuery() [prepared_statement_03.cpp:222]
        → FirebirdDBConnection::registerResultSet() [connection_01.cpp:157]
          → Acquires m_activeResultSetsMutex (0x8A7E750)  ← SECOND
```

**Order**: `m_connMutex` → `m_activeResultSetsMutex` ✅

#### Scenario B: Violated Order (Incorrect - DEADLOCK!)
```
Thread returning connection to pool:
  FirebirdDBConnection::closeAllActiveResultSets() [connection_01.cpp:276]
    → Acquires m_activeResultSetsMutex (0x8A7E750)        ← FIRST
      → FirebirdDBResultSet::close() [result_set_02.cpp:281]
        → Acquires m_connMutex (0x8A66640)                ← SECOND
```

**Order**: `m_activeResultSetsMutex` → `m_connMutex` ❌

### Deadlock Scenario

**How the deadlock happens:**

| Time | Thread 1 (Executing Query) | Thread 2 (Closing ResultSets) |
|------|----------------------------|-------------------------------|
| T1 | Acquires `m_connMutex` | |
| T2 | | Acquires `m_activeResultSetsMutex` |
| T3 | Tries to acquire `m_activeResultSetsMutex` | |
| T4 | **BLOCKED** waiting for Thread 2 | Tries to acquire `m_connMutex` |
| T5 | | **BLOCKED** waiting for Thread 1 |
| T6 | **DEADLOCK** 💀 | **DEADLOCK** 💀 |

---

## Stack Traces from Helgrind

### First Lock Acquisition (Scenario A - Establishes Order)

```
Required order was established by acquisition of lock at 0x8A66640
  at std::recursive_mutex::lock() (mutex:108)
  by std::lock_guard<std::recursive_mutex>::lock_guard(std::recursive_mutex&) (std_mutex.h:229)
  by cpp_dbc::Firebird::FirebirdDBConnection::executeQuery(std::nothrow_t, ...)
     (connection_02.cpp:251)
  by cpp_dbc::Firebird::FirebirdDBConnection::executeQuery(...)
     (connection_01.cpp:578)
  by cpp_dbc::RelationalDBConnectionPool::validateConnection(...)
     (relational_db_connection_pool.cpp:216)

followed by a later acquisition of lock at 0x8A7E750
  at std::mutex::lock() (std_mutex.h:100)
  by std::lock_guard<std::mutex>::lock_guard(std::mutex&) (std_mutex.h:229)
  by cpp_dbc::Firebird::FirebirdDBConnection::registerResultSet(...)
     (connection_01.cpp:157)
  by cpp_dbc::Firebird::FirebirdDBPreparedStatement::executeQuery(std::nothrow_t)
     (prepared_statement_03.cpp:222)
```

### Lock Order Violation (Scenario B - Violates Order)

```
Thread #1: lock order "0x8A66640 before 0x8A7E750" violated

Observed (incorrect) order is: acquisition of lock at 0x8A7E750
  at std::mutex::lock() (std_mutex.h:100)
  by std::lock_guard<std::mutex>::lock_guard(std::mutex&) (std_mutex.h:229)
  by cpp_dbc::Firebird::FirebirdDBConnection::closeAllActiveResultSets()
     (connection_01.cpp:276)
  by cpp_dbc::Firebird::FirebirdDBConnection::prepareForPoolReturn()
     (connection_01.cpp:342)
  by cpp_dbc::RelationalDBConnectionPool::returnConnection(...)
     (relational_db_connection_pool.cpp:279)

followed by a later acquisition of lock at 0x8A66640
  at std::recursive_mutex::lock() (mutex:108)
  by std::lock_guard<std::recursive_mutex>::lock_guard(std::recursive_mutex&) (std_mutex.h:229)
  by cpp_dbc::Firebird::FirebirdDBResultSet::close()
     (result_set_02.cpp:281)
  by cpp_dbc::Firebird::FirebirdDBConnection::closeAllActiveResultSets()
     (connection_01.cpp:285)
```

---

## Affected Files

| File | Line | Function | Issue |
|------|------|----------|-------|
| [connection_01.cpp](../../src/drivers/relational/firebird/connection_01.cpp) | 157 | `registerResultSet()` | Acquires `m_activeResultSetsMutex` |
| [connection_01.cpp](../../src/drivers/relational/firebird/connection_01.cpp) | 276 | `closeAllActiveResultSets()` | Acquires `m_activeResultSetsMutex` BEFORE iterating |
| [connection_01.cpp](../../src/drivers/relational/firebird/connection_01.cpp) | 285 | `closeAllActiveResultSets()` | Calls `rs->close()` while holding `m_activeResultSetsMutex` |
| [connection_02.cpp](../../src/drivers/relational/firebird/connection_02.cpp) | 251 | `executeQuery(std::nothrow_t, ...)` | Acquires `m_connMutex` |
| [result_set_02.cpp](../../src/drivers/relational/firebird/result_set_02.cpp) | 281 | `FirebirdDBResultSet::close()` | Acquires `m_connMutex` |
| [prepared_statement_03.cpp](../../src/drivers/relational/firebird/prepared_statement_03.cpp) | 222 | `executeQuery(std::nothrow_t)` | Calls `registerResultSet()` |

---

## Proposed Solutions

### Solution 1: Consistent Lock Order (Recommended)

**Always acquire locks in this order**: `m_connMutex` → `m_activeResultSetsMutex`

**Change `closeAllActiveResultSets()` to:**
```cpp
void FirebirdDBConnection::closeAllActiveResultSets() {
    // Acquire m_connMutex FIRST to maintain consistent order
    std::lock_guard<std::recursive_mutex> connLock(*m_connMutex);

    // Then acquire m_activeResultSetsMutex
    std::lock_guard<std::mutex> lock(m_activeResultSetsMutex);

    for (auto it = m_activeResultSets.begin(); it != m_activeResultSets.end(); ) {
        if (auto rs = it->lock()) {
            rs->close();  // Now safe - m_connMutex already held
            ++it;
        } else {
            it = m_activeResultSets.erase(it);
        }
    }
}
```

**Pros:**
- ✅ Simple fix
- ✅ Maintains consistent lock ordering
- ✅ No architectural changes needed

**Cons:**
- ⚠️ Slightly broader lock scope (m_connMutex held longer)

---

### Solution 2: Use `std::scoped_lock` for Atomic Multi-Lock

**Acquire both locks atomically** to prevent deadlock:

```cpp
void FirebirdDBConnection::closeAllActiveResultSets() {
    // Acquire both locks atomically (deadlock-free)
    std::scoped_lock lock(m_activeResultSetsMutex, *m_connMutex);

    for (auto it = m_activeResultSets.begin(); it != m_activeResultSets.end(); ) {
        if (auto rs = it->lock()) {
            rs->close();
            ++it;
        } else {
            it = m_activeResultSets.erase(it);
        }
    }
}
```

**Pros:**
- ✅ Deadlock-proof (C++17 guarantees atomic acquisition)
- ✅ More elegant code

**Cons:**
- ⚠️ Requires C++17 (already enabled in this project)

---

### Solution 3: Re-design Locking Strategy

**Separate concerns** to avoid nested locking:

1. Copy weak_ptr list under `m_activeResultSetsMutex` lock
2. Release `m_activeResultSetsMutex`
3. Iterate and close without holding `m_activeResultSetsMutex`

```cpp
void FirebirdDBConnection::closeAllActiveResultSets() {
    // Step 1: Copy list under lock
    std::vector<std::weak_ptr<FirebirdDBResultSet>> resultSetsCopy;
    {
        std::lock_guard<std::mutex> lock(m_activeResultSetsMutex);
        resultSetsCopy = m_activeResultSets;  // Copy weak_ptrs
        m_activeResultSets.clear();
    }
    // m_activeResultSetsMutex is now released

    // Step 2: Close result sets without holding m_activeResultSetsMutex
    for (auto& weakRs : resultSetsCopy) {
        if (auto rs = weakRs.lock()) {
            rs->close();  // Can acquire m_connMutex safely
        }
    }
}
```

**Pros:**
- ✅ No nested locking
- ✅ Better separation of concerns
- ✅ Shorter critical sections

**Cons:**
- ⚠️ Slightly more complex
- ⚠️ Small overhead from vector copy (minimal for weak_ptr)

---

## Recommendation

**Use Solution 1 (Consistent Lock Order)** for immediate fix:
- Simple, safe, and maintains existing architecture
- Easy to verify correctness

**Consider Solution 3 (Re-design)** for future refactoring:
- Better long-term design
- More scalable for high-concurrency scenarios

---

## Testing Strategy

After implementing the fix:

1. **Run Helgrind tests** to verify lock order is consistent:
   ```bash
   ./helper.sh --run-test=rebuild,firebird,helgrind-s,auto,run=10
   ```

2. **Stress test with connection pool**:
   - Multiple threads acquiring/releasing connections concurrently
   - Validate connections (triggers `executeQuery`)
   - Return connections (triggers `closeAllActiveResultSets`)

3. **Verify no regressions**:
   - All existing tests still pass
   - No performance degradation

---

## Related Issues

- This bug affects **ALL database drivers** that use similar locking patterns
- Check for similar patterns in:
  - `MySQLDBConnection`
  - `PostgreSQLDBConnection`
  - `SQLiteDBConnection`

---

## Notes

- **Do NOT suppress this error** - it's a real bug, not a Helgrind false positive
- **Priority**: HIGH - can cause production deadlocks in multi-threaded applications
- **Helgrind is correct** - this detection saved us from a subtle concurrency bug

---

**Last Updated**: 2026-02-14
**Assigned To**: TBD
**Target Fix Version**: TBD
