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
**Affected Files**:
- `src/drivers/relational/firebird/connection_01.cpp` (lines 157, 276, 285)
- `src/drivers/relational/firebird/connection_02.cpp` (line 251)
- `src/drivers/relational/firebird/result_set_02.cpp` (line 281)

**Quick Fix**: Use consistent lock order (`m_connMutex` before `m_activeResultSetsMutex`)

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
| 🔴 OPEN | 1 |
| 🟡 IN PROGRESS | 0 |
| 🟢 FIXED | 0 |
| 🔵 WONTFIX | 0 |

**Last Updated**: 2026-02-14
