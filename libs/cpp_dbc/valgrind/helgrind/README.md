# Helgrind Thread Safety Analysis

This directory contains Helgrind suppression files and analysis documentation for the cpp_dbc project.

**Helgrind** is a Valgrind tool for detecting data races and synchronization errors in multithreaded programs.

---

## 📋 Files

| File | Purpose |
|------|---------|
| **suppression.conf** | Generic suppression patterns for database client library false positives |
| **ANALYSIS.md** | Complete investigation of all Helgrind errors with evidence and justification |
| **README.md** | This file - usage guide and overview |

---

## 🎯 Quick Start

### Run Tests with Helgrind

```bash
# Full test with suppressions (recommended)
./helper.sh --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,helgrind-s,auto,parallel=5,run=5

# Generate new suppressions (for analysis)
./helper.sh --run-test=rebuild,sqlite,helgrind-gs,auto,run=1

# Run specific driver with Helgrind
./helper.sh --run-test=rebuild,mysql,helgrind-s,auto,run=1
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `helgrind-s` | Run with suppressions from `suppression.conf` |
| `helgrind-gs` | Generate suppressions (`--gen-suppressions=all`) |
| `helgrind` | Run without suppressions (baseline) |

---

## ⚠️ Important: Errors Status (Updated 2026-02-17)

**As of 2026-02-17, ALL remaining Helgrind errors in cpp_dbc tests are FALSE POSITIVES from third-party database client libraries.**

### Real Bugs — FIXED (2026-02-17)

The following REAL bugs were found in cpp_dbc's Firebird driver and connection pool, and have been **fully fixed**:

| Bug | Type | Status |
|-----|------|--------|
| Firebird `m_trPtr` raw pointer (USE-AFTER-FREE) | REAL BUG | ✅ Fixed 2026-02-17 |
| Firebird lock order violation: `m_connMutex` ↔ `m_statementsMutex`/`m_resultSetsMutex` (ABBA deadlock) | REAL BUG | ✅ Fixed 2026-02-17 |
| Connection pool: closing connections inside pool locks (LockOrder) | REAL BUG | ✅ Fixed 2026-02-17 |
| `m_closed`/`m_resetting` non-atomic bools (data race) | REAL BUG | ✅ Fixed 2026-02-17 |

See [research/HELGRIND_FIX_COMPLETE_SUMMARY.md](../../../../research/HELGRIND_FIX_COMPLETE_SUMMARY.md) for complete fix documentation.

### Remaining False Positives (Library Internals)

These errors originate from **third-party database client libraries**, NOT from cpp_dbc code. See [ANALYSIS.md](./ANALYSIS.md) for complete investigation.

| Driver | Errors | Root Cause | Evidence |
|--------|--------|------------|----------|
| **Firebird** | ~482k | BSS global variables + internal threads | [ANALYSIS.md#1-firebird](./ANALYSIS.md#1-firebird-libfbclientso) |
| **PostgreSQL** | 216 | Async I/O during `PQconnectdb` | [ANALYSIS.md#2-postgresql](./ANALYSIS.md#2-postgresql-libpqso) |
| **SQLite** | 15+ | POSIX file locks (WAL mode) | [ANALYSIS.md#3-sqlite](./ANALYSIS.md#3-sqlite-libsqlite3so) |
| **ScyllaDB** | ~137k | Lock-free queues (libuv) + heap address reuse | [ANALYSIS.md#4-scylladb-cassandra](./ANALYSIS.md#4-scylladb--cassandra-libcassandraso) |
| **MySQL** | 2 | OpenSSL 3.x lock type mismatch | [ANALYSIS.md#5-mysql](./ANALYSIS.md#5-mysql-libmysqlclientso--openssl-3x) |

### Why They're False Positives

1. **Invisible Synchronization**: Libraries use POSIX file locks, custom atomics, and lock-free algorithms that Helgrind cannot detect
2. **Library-Internal Threads**: Errors occur between background threads created by the libraries, not application code
3. **Library-Internal Memory**: All conflicts are in library BSS segments, heap, or shared memory - not cpp_dbc variables
4. **Correct API Usage**: cpp_dbc only calls public API functions properly - no improper threading or handle sharing

---

## 📚 Suppression Strategy

### Generic Patterns

Suppressions target **library code only** using:

- **Library-scoped**: `obj:*/libfbclient.so*` - matches any version/path
- **Function-scoped**: `fun:PQconnectdb` - specific known functions
- **Wildcard patterns**: `fun:*datastax*RequestProcessor*` - internal symbols

### What's Suppressed

```
Firebird (libfbclient.so):
  ✓ Data races in BSS segment globals
  ✓ Lock order violations during fb_shutdown
  ✓ Races in isc_attach_database / isc_detach_database

PostgreSQL (libpq.so):
  ✓ Data races during PQconnectdb async I/O
  ✓ BSS segment races in connection polling

SQLite (libsqlite3.so):
  ✓ WAL mode shared memory (.db-shm) races
  ✓ POSIX file lock coordination (invisible to Helgrind)
  ✓ Pager/BTree commit races
  ✓ Lock order violations during open/close operations
  ✓ Multi-lock protocol in sqlite3BtreeOpen, sqlite3PagerOpen, sqlite3OsClose

ScyllaDB (libcassandra.so + libuv.so):
  ✓ Lock-free RequestProcessor queue races
  ✓ EventLoop async I/O races
  ✓ libuv event loop and thread races

MySQL (libmysqlclient.so + OpenSSL 3.x):
  ✓ OpenSSL CRYPTO_THREAD lock type mismatches
  ✓ SSL_CTX_free / SSL_CTX_flush_sessions during mysql_close
  ✓ pthread_rwlock/pthread_mutex type confusion in OpenSSL 3.x
```

### What's NOT Suppressed

- **cpp_dbc application code** - any real bugs will still be detected
- **C++ standard library** - data races in std::shared_ptr, std::mutex, etc.
- **Test code** - threading errors in test cases

---

## 🔍 Investigating New Errors

If you see Helgrind errors **not in database client libraries**, investigate:

### 1. Check the Stack Trace

```bash
# Look for cpp_dbc functions in the stack
grep -A 30 "Possible data race" your_log.log | grep "cpp_dbc::"
```

**Library code (OK to suppress):**
```
by 0x4F32727: ??? (in /usr/lib/x86_64-linux-gnu/libpq.so.5.14)
by PQconnectdb (in /usr/lib/x86_64-linux-gnu/libpq.so.5.14)
```

**Application code (INVESTIGATE):**
```
by 0x12345: cpp_dbc::MyClass::myMethod() (my_file.cpp:123)
```

### 2. Check the Memory Address

```bash
# Look for BSS segment (library globals)
grep "Address 0x" your_log.log
```

**Library memory (OK to suppress):**
```
Address 0x4f5d0e4 is in the BSS segment of /usr/lib/.../libpq.so.5.14
```

**Application memory (INVESTIGATE):**
```
Address 0x12345 is 16 bytes inside a block of size 64 alloc'd
  at MyClass::MyClass() (my_file.cpp:50)
```

### 3. Verify Thread Origins

**Library-created threads (OK to suppress):**
```
by 0x485396A: ??? (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
by 0x5A55AC2: start_thread
  (created by library internal code, not std::thread)
```

**Application threads (INVESTIGATE):**
```
by std::thread::_State_impl::_M_run() (in /usr/lib/.../libstdc++.so)
  (created by cpp_dbc or test code)
```

---

## 🛠️ Adding New Suppressions

### When to Add

**Add suppression when:**
1. Error is in **third-party library code** (not cpp_dbc)
2. Memory is in **library BSS segment** or library-allocated heap
3. Threads are **created by the library** internally
4. Synchronization mechanism is **invisible to Helgrind** (POSIX locks, custom atomics)

**Do NOT suppress when:**
1. Error is in **cpp_dbc code**
2. Error is in **test code** (fix the test)
3. Error is in **C++ standard library** (likely a real bug)

### How to Add

1. **Analyze the error** (see [ANALYSIS.md](./ANALYSIS.md) for methodology)
2. **Create generic pattern** (use wildcards, avoid specifics)
3. **Document thoroughly** (add comments with justification)
4. **Test validation** (verify suppression works without hiding real bugs)

### Suppression Template

```
# ------------------------------------------------------------------------------
# Library Name (library.so)
# ------------------------------------------------------------------------------
# Issue: Brief description of the problem
# Evidence: Memory addresses, functions, threads involved
# Justification: Why this is a false positive
# Test Prefix: Which tests trigger this
# ------------------------------------------------------------------------------

{
   descriptive_name_for_suppression
   Helgrind:Race
   obj:*/library.so*
   ...
   fun:problematic_function
   ...
}
```

---

## 📊 Expected Results

### Before Suppressions

```
10_ (common):      9 errors
20_ (MySQL):       2 errors
21_ (PostgreSQL):  216 errors
22_ (SQLite):      15 errors
23_ (Firebird):    482,585 errors
26_ (ScyllaDB):    137,000 errors
────────────────────────────────
Total:             ~620,000+ false positives
```

### After Suppressions

```
All tests:         0 errors
Suppressed:        ~620,000 (all database client library internals)
Real bugs:         0 (none found in cpp_dbc code)
```

---

## 🔗 Related Documentation

- **[ANALYSIS.md](./ANALYSIS.md)**: Complete investigation of all Helgrind errors
- **[MEMORY.md](../../../../.claude/projects/-home-dsystems-Desktop-projects-cpp-cpp-dbc/memory/MEMORY.md)**: SQLite FileMutexRegistry solution
- **[research/sqlite/file_mutex_registry/](../../../../research/sqlite/file_mutex_registry/)**: Research on eliminating SQLite false positives
- **[Helgrind Manual](https://valgrind.org/docs/manual/hg-manual.html)**: Official Valgrind Helgrind documentation
- **[SQLite WAL Mode](https://www.sqlite.org/wal.html)**: SQLite Write-Ahead Logging documentation

---

## 🎓 Understanding Helgrind Limitations

### What Helgrind Can Detect

✅ Traditional mutex-based synchronization (`pthread_mutex_lock`)
✅ POSIX read-write locks (`pthread_rwlock_*`)
✅ POSIX semaphores (`sem_wait`, `sem_post`)
✅ Happens-before relationships via mutexes

### What Helgrind Cannot Detect

❌ **POSIX file locks** (`fcntl()` advisory locks) - used by SQLite
❌ **Custom atomic operations** - lock-free queues (ScyllaDB/libuv)
❌ **Shared memory synchronization** - `.db-shm` files (SQLite WAL)
❌ **Library-internal lock protocols** - custom mutexes initialized before instrumentation

### Why This Matters

Database client libraries often use:
- **POSIX file locks** for inter-process synchronization (SQLite, Firebird)
- **Lock-free algorithms** for high-performance async I/O (ScyllaDB, PostgreSQL)
- **Custom memory barriers** and atomics invisible to Helgrind

This is **not a bug** in the libraries - it's a limitation of Helgrind's analysis capabilities.

---

## ⚙️ Special Cases

### SQLite FileMutexRegistry

For **testing builds only**, cpp_dbc has an **optional** solution to eliminate SQLite false positives:

```bash
# Build with FileMutexRegistry (testing only)
cmake -B build -DENABLE_FILE_MUTEX_REGISTRY=ON

# Run tests (will show 0 SQLite errors)
./helper.sh --run-test=rebuild,sqlite,helgrind-s,auto,run=5
```

**Result**: SQLite errors 132 → 0 (100% elimination)
**Overhead**: ~33% (acceptable for testing, disabled in production)
**See**: [MEMORY.md - FileMutexRegistry](../../../../.claude/projects/-home-dsystems-Desktop-projects-cpp-cpp-dbc/memory/MEMORY.md#sqlite-filemutexregistry-eliminating-threadsanitizer-false-positives)

### Helgrind Internal Crashes

**Known Issue**: Helgrind 3.18.1 can crash when analyzing complex lock protocols (OpenSSL 3.x, libuv):

```
Helgrind: hg_main.c:308 (lockN_acquire_reader): Assertion 'lk->kind == LK_rdwr' failed.
```

**Status**: ✅ Automatically detected as **WARNING** (not failure) by `run_test_parallel.sh`
**Impact**: Non-fatal - test code passed, only Helgrind crashed
**Occurrence**: Primarily in ScyllaDB tests (prefix 26_)

---

## 📝 Maintenance

### Periodic Review

Review suppressions when:
- ✅ Upgrading database client libraries (new versions may fix/change behavior)
- ✅ Upgrading Helgrind (newer versions may have improved analysis)
- ✅ Adding new database drivers
- ✅ Seeing unexplained test failures

### Validation Checklist

After modifying suppressions:

```bash
# 1. Run all tests with suppressions
./helper.sh --run-test=rebuild,all,helgrind-s,auto,parallel=5,run=5

# 2. Verify 0 errors (or only known warnings)
# Check logs: logs/test/YYYY-MM-DD-HH-mm-SS/*.log

# 3. Run a baseline without suppressions (on a single driver)
./helper.sh --run-test=rebuild,sqlite,helgrind,auto,run=1

# 4. Compare error counts - should match ANALYSIS.md
```

---

## 🚀 CI/CD Integration

### Recommended Pipeline

```yaml
test:
  script:
    # Run with suppressions - expect 0 errors
    - ./helper.sh --run-test=rebuild,all,helgrind-s,auto,parallel=8,run=3

  allow_failure:
    # Allow Helgrind internal crashes (known issue)
    - exit_code: 1
      when: on_warnings
```

### Exit Codes

| Code | Meaning | CI Action |
|------|---------|-----------|
| 0 | All tests passed, 0 errors | ✅ Pass |
| 1 | Helgrind errors found | ❌ Fail (investigate) |
| 1 + WARNING | Helgrind internal crash only | ⚠️ Warning (acceptable) |

---

## 📞 Support

For questions or issues:

1. **Read ANALYSIS.md first** - comprehensive investigation of all errors
2. **Check MEMORY.md** - related thread safety solutions (FileMutexRegistry)
3. **Search logs** - use `grep -A 30 "Possible data race" your.log`
4. **File an issue** - if you find a real bug in cpp_dbc code (not suppressed by library patterns)

---

**Last Updated**: 2026-02-17
**Helgrind Version**: 3.18.1
**Total Suppressions**: 50+ generic patterns
**Analysis**: Complete - see ANALYSIS.md
