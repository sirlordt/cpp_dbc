# Known Bugs and Issues

This directory contains detailed analysis of known bugs that need to be fixed.

---

## 🔴 Critical - High Priority

### 1. [Firebird Lock Order Deadlock](./firebird_lock_order_deadlock.md)
**Status**: 🔴 OPEN
**Severity**: HIGH
**Found**: 2026-02-14
**Description**: Lock order violation between `m_connMutex` and `m_activeResultSetsMutex` in `FirebirdDBConnection` can cause deadlock when multiple threads use connection pool.

**Impact**: Production deadlocks in multi-threaded applications
**Detected by**: Helgrind (Valgrind)
**Total Instances**: **5,949 lock order violations** across 42 contexts
**Affected Files**:
- `src/drivers/relational/firebird/connection_01.cpp` (lines 157, 241, 276, 285, 342, 354, 578)
- `src/drivers/relational/firebird/connection_02.cpp` (lines 110, 251, 265)
- `src/drivers/relational/firebird/connection_03.cpp` (line 370)
- `src/drivers/relational/firebird/result_set_02.cpp` (line 281)
- `src/core/relational/relational_db_connection_pool.cpp` (lines 189, 216, 279, 1340)

**Quick Fix**: Use consistent lock order (`m_connMutex` before `m_activeResultSetsMutex`)

**📋 Full Analysis**: [firebird_helgrind_analysis.md](./firebird_helgrind_analysis.md) - Complete 43-context analysis

---

### 2. [Firebird Data Race - m_lastUsedTime](./firebird_helgrind_analysis.md#context-1-data-race-in-connection-pool-1-error)
**Status**: 🔴 OPEN
**Severity**: MEDIUM
**Found**: 2026-02-14
**Description**: Unprotected concurrent access to `m_lastUsedTime` timestamp in `RelationalPooledDBConnection`. Maintenance thread reads while worker thread writes.

**Impact**: Race condition in connection pool maintenance
**Detected by**: Helgrind (Valgrind)
**Affected Files**:
- `src/core/relational/relational_db_connection_pool.cpp` (lines 1253, 1303)

**Quick Fix**: Use `std::atomic<std::chrono::steady_clock::time_point>` for `m_lastUsedTime`

---

## 📋 How to Use This Directory

1. **Report a new bug**: Create a detailed markdown file with analysis
2. **Update status**: Edit the file when bug is fixed/in-progress
3. **Link to issues**: Reference GitHub issues if applicable
4. **Include repro steps**: Make bugs reproducible

---

## 🏷️ Status Labels

- 🔴 **OPEN** - Not started
- 🟡 **IN PROGRESS** - Being worked on
- 🟢 **FIXED** - Resolved and verified
- 🔵 **WONTFIX** - Decided not to fix (with justification)

---

## 📊 Bug Statistics

| Status | Count |
|--------|-------|
| 🔴 OPEN | 2 |
| 🟡 IN PROGRESS | 0 |
| 🟢 FIXED | 0 |
| 🔵 WONTFIX | 0 |

| Severity | Count |
|----------|-------|
| 🔴 HIGH | 1 |
| 🟡 MEDIUM | 1 |

**Total Error Instances**: 5,950 errors (5,949 lock order + 1 data race)

**Last Updated**: 2026-02-14
