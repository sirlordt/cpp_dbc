# How to Report Convention Violations

> **CRITICAL**: Every analysis MUST be written to a `.md` file on disk at `research/<driver_name>_improvements_<N>.md`. Showing results only in chat output is NOT valid. See "Output Location" at the end of this document.

When asked to analyze code for convention compliance, produce a structured report following the format below. The reference conventions are defined in `.claude/rules/cpp_dbc_conventions.md`.

## Report Structure

### 1. Header

```markdown
# <Driver/Component Name> — Convention Compliance Analysis

**Date**: YYYY-MM-DD
**Scope**: <Brief description of what was analyzed>
**Reference**: `.claude/rules/cpp_dbc_conventions.md`
```

### 2. Files Analyzed

List every file that was read and inspected, grouped by type:

```markdown
## Files Analyzed

### Headers
- `libs/cpp_dbc/include/cpp_dbc/drivers/.../file.hpp`

### Sources
- `libs/cpp_dbc/src/drivers/.../file_01.cpp`
```

**Rule**: Only list files you actually read. Never report violations in files you did not read. If the scope is too large to read every file, state which files were skipped and why.

### 3. Violations

Each violation is a numbered section with a horizontal rule separator (`---`) between them:

```markdown
---

## Violation #N — <Short descriptive title>

**Severity**: Critical | High | Medium | Low
**Count**: <Number of occurrences>
**Convention**: <Quote or paraphrase of the violated rule from cpp_dbc_conventions.md>

| File | Line |
|------|------|
| `file_01.cpp` | 42 |
| `file_02.cpp` | 108 |

<Optional: Show the current (incorrect) code>

**Fix**: <Concrete code example or clear instruction showing the correct form>
```

### 4. Summary Table

After all violations, include a summary:

```markdown
## Summary

| Severity | Count | Category |
|----------|-------|----------|
| **Critical** | N | <Brief description> (#ref) |
| **High** | N | <Brief description> (#ref) |
| **Medium** | N | <Brief description> (#ref) |
| **Low** | N | <Brief description> (#ref) |

**Total: X violations across Y files.**
```

Follow the summary with a single sentence highlighting the most critical finding.

### 5. What Passes Convention Checks

End the report with a section listing conventions that were verified and found correct:

```markdown
## What Passes Convention Checks

- <Convention name>: <brief confirmation of compliance>
- <Convention name>: <brief confirmation of compliance>
```

This section is mandatory. It proves the analysis was thorough and not just a search for problems. It also helps the user understand what is already correct and does not need attention.

---

## Severity Definitions

| Severity | Definition | Examples |
|----------|-----------|----------|
| **Critical** | Can cause crashes, data corruption, undefined behavior, or security vulnerabilities at runtime | Missing `m_closed` check leads to null dereference; missing mutex guard causes data race |
| **High** | Violates a core safety or correctness convention but may not crash immediately | Missing `// NOSONAR` annotation on `catch(...)`; private method with throwing variant (dead code that breaks `-fno-exceptions`) |
| **Medium** | Violates a structural or stylistic convention that affects maintainability or consistency | Redundant `private:` in class; missing `std::move` in error path; public method without dual API |
| **Low** | Minor deviation from convention with no functional impact | Missing explicit `{nullptr}` on `unique_ptr`; empty constructor body without comment |

---

## Rules for the Analyst

1. **Read before reporting**: You must read every file in scope before writing the report. Never guess line numbers or assume violations exist without seeing the code.

2. **Exact line numbers**: Every violation must include the exact file path and line number. If a violation spans multiple lines, report the starting line.

3. **One violation per convention rule**: Group all occurrences of the same convention violation under a single `## Violation #N` section. Do not create separate violations for each occurrence of the same rule break.

4. **Quote the convention**: Each violation must reference the specific convention rule being violated — either a direct quote or a precise paraphrase. This makes the report verifiable.

5. **Show the fix**: Every violation must include a concrete fix — either a code example or a clear instruction. The fix must follow the conventions exactly.

6. **Count accurately**: The `**Count**` field is the total number of individual occurrences, and it must match the number of rows in the location table.

7. **No false positives**: If you are unsure whether something is a violation, do not include it. Err on the side of precision over recall. A report with zero false positives is more valuable than one that catches everything but includes noise.

8. **No retroactive enforcement**: The conventions state that several rules apply "only to new or modified code" and that "pre-existing comments do not need to be updated retroactively." By default, only report violations in new or modified code — not in pre-existing code that was untouched. A whole-driver review (where all code in the driver's files is treated as in-scope) only applies when the user explicitly requests it (e.g., "analyze the entire MySQL driver", "full audit of all ScyllaDB files"). If the user does not specify a whole-driver review, restrict findings to new or modified code.

9. **Consistent numbering**: Violations are numbered sequentially starting at `#1`. The numbering has no relation to severity — violations are ordered by the sequence in which they are discovered during the file-by-file analysis.

10. **Summary must match**: The total count in the summary table must equal the sum of all individual violation counts. The file count must equal the number of distinct files that have at least one violation.

11. **Be thorough on passing checks**: The "What Passes" section should cover every major convention category that was checked and found compliant. This is not filler — it demonstrates analytical depth.

---

## Checklist of Conventions to Verify

When performing a full compliance analysis, check each of the following. This list mirrors the sections in `cpp_dbc_conventions.md`:

### Error Codes & Formatting
- [ ] **DBException error codes**: 12-char uppercase alphanumeric, unique, random
- [ ] **Indentation**: 4 spaces, no tabs
- [ ] **Preprocessor directives**: Always flush left (column 0), no indentation
- [ ] **Braces**: Always required on control flow statements
- [ ] **Braces — Allman style**: Opening and closing braces each on their own line (no single-line blocks)
- [ ] **Empty braces**: Must contain an explanatory comment
- [ ] **NOSONAR comments**: Must include rule ID in parentheses and explanation
- [ ] **Bug-fix comments**: ISO 8601 timestamp + `Bug:` + `Solution:` structure
- [ ] **Namespace style**: C++17 nested syntax (`namespace cpp_dbc::MySQL`)

### Unused Parameters
- [ ] **No `(void)` cast**: Use unnamed parameter or `[[maybe_unused]]` instead of `(void)param;`

### Memory Safety
- [ ] **RAII handles**: Used for external C library resources (`BsonHandle`, `RedisReplyHandle`, etc.)
- [ ] **Null checks**: Defensively check for nulls before dereferencing pointers from external libraries
- [ ] **Smart pointers**: Avoid raw pointers; use `shared_ptr`, `unique_ptr`, `weak_ptr` in member/class variables
- [ ] **C++17 constructs**: Use C++17-style constructs and functions for more secure code
- [ ] **No `const_cast`**: Avoid `const_cast` to remove const; only permitted as last resort with third-party APIs and documented justification. **Accepted exception**: MySQL ResultSet materialized mode — `MYSQL_ROW` is `typedef char**` (MySQL C API, cannot be changed); the `const_cast` in `next()` is documented and not a violation
- [ ] **Ranges over index loops**: Prefer ranges, avoid index loops where possible
- [ ] **Member initialization**: Prefer in-class initializers over constructor body
- [ ] **Empty constructors**: Prefer `= default` over empty braces `{}`
- [ ] **String-to-number conversion**: Always `std::from_chars`; never `std::stoi`/`std::atoi`/`std::strtol`/`std::sscanf`
- [ ] **String containment**: Always `.contains()` for substring checks; never `find() != npos` for boolean containment
- [ ] **String prefix/suffix**: Always `.starts_with()` / `.ends_with()`; never `find() == 0`, `substr()`, `rfind()`, or `compare()` for prefix/suffix checks

### Atomics & Thread Safety
- [ ] **`std::atomic` reads**: Always `.load(std::memory_order_seq_cst)` — no implicit bool conversion, no bare `.load()` without memory order
- [ ] **`std::atomic` writes**: Always `.store(..., std::memory_order_seq_cst)` — no bare `.store()` without memory order
- [ ] **`std::atomic` exchange/CAS**: Always `std::memory_order_seq_cst` for `.exchange()` and `.compare_exchange_*()`
- [ ] **Weaker ordering requires justification**: Any use of `acquire`/`release`/`acq_rel`/`relaxed` must have a mandatory comment immediately above explaining (a) why seq_cst was replaced, (b) profiling evidence, (c) correctness proof. Without the comment, the weaker ordering is a violation regardless of technical safety
- [ ] **Native handle name**: Connection's native handle must be named `m_conn` (not `m_mysql`, `m_db`, etc.)
- [ ] **Connection mutex type**: Must be `SharedConnMutex` (`std::shared_ptr<std::recursive_mutex>`), named `m_connMutex`; never a direct `std::recursive_mutex` member. **Accepted exception**: SQLite uses `m_globalFileMutex` (file-level mutex from `FileMutexRegistry`) shared by all connections to the same database file — the different name is intentional and not a violation. `getConnectionMutex(std::nothrow)` returns `*m_globalFileMutex`. Child objects still use `weak_ptr<SQLiteDBConnection>`, `friend class SQLiteConnectionLock`, and check `m_closed`/`m_connection.expired()`.
- [ ] **`getConnectionMutex(std::nothrow_t)`**: Connection must declare `std::recursive_mutex &getConnectionMutex(std::nothrow_t) noexcept` in the **`protected`** section (not public); accessed by `*ConnectionLock` via `friend class`. SQLite's implementation returns `*m_globalFileMutex` and is NOT guarded by `#if DB_DRIVER_THREAD_SAFE` (always required for sanitizer compatibility).
- [ ] **Child objects must not store connection mutex**: PreparedStatement, Blob, Cursor, Collection, Document must not have `m_connMutex` or `m_globalFileMutex` as members; acquire via `weak_ptr<Connection>` + RAII helper. **Exception**: Store-result ResultSets (MySQL, PostgreSQL) may use their own independent `m_mutex` instead of the connection mutex — they load all data into client memory and never contact the server during iteration. Note: `*InputStream` is NOT a child of Connection — it is a pure in-memory byte buffer with no connection dependency.
- [ ] **Blob synchronization**: Blob classes (child of Connection, created by ResultSet) that perform native DB API calls (`ensureLoaded`, `save`, etc.) must use `weak_ptr<*DBConnection> m_connection` (not `weak_ptr<NativeHandle>`), `mutable atomic<bool> m_closed`, `friend *ConnectionLock`, and `*_STMT_LOCK_OR_RETURN` before any native call
- [ ] **Connection-level macros**: `*_CONNECTION_LOCK_OR_RETURN/THROW/SUCCESS_IF_CLOSED` must check `m_closed || !m_conn`; non-thread-safe `#else` must also check (not `(void)0`)
- [ ] **RAII ConnectionLock helper**: Each driver must define `*ConnectionLock` class with double-checked locking, `isAcquired() const noexcept`, `operator bool() const noexcept`
- [ ] **Statement/Child-level macros**: `*_LOCK_OR_RETURN/THROW/SUCCESS_IF_CLOSED` or `*_STMT_LOCK_OR_*` using the RAII helper in PreparedStatement, ResultSet, Blob, Cursor, Collection, Document; lock variable named `<driver>_conn_lock_` (never `__lock`). **Exception**: Store-result ResultSets (MySQL, PostgreSQL) use their own local mutex macros instead of the `*ConnectionLock` RAII helper — but still check `m_closed` + `m_connection.expired()`
- [ ] **Child objects reject operations when parent connection is closed**: Every child (PreparedStatement, ResultSet, Blob, Cursor, Collection, Document) must guard every public method checking both `weak_ptr` expiry AND `m_closed`. This applies even to store-result ResultSets (MySQL, PostgreSQL) that only read in-memory data — the lifecycle contract is enforced regardless of mutex source. `*InputStream` is exempt — it has no connection dependency.
- [ ] **Non-thread-safe macros check `m_connection.expired()`**: The `#else` (non-thread-safe) variants of `*_STMT_LOCK_OR_RETURN/THROW/SUCCESS_IF_CLOSED` must check `m_connection.expired()` and set `m_closed = true` if expired, not just check `m_closed`
- [ ] **No dead macros**: `DB_DRIVER_MUTEX` and `DB_DRIVER_UNIQUE_LOCK` must not be defined
- [ ] **Debug macro pattern**: `snprintf` to 1024-byte buffer + truncation only; no `std::cout`, no `snprintf < 0` handling
- [ ] **`std::scoped_lock` preference**: Prefer `std::scoped_lock` over `lock_guard`/`unique_lock` wherever it makes sense
- [ ] **`std::recursive_mutex` preference**: Prefer `std::recursive_mutex` over other mutex types for re-entrant locking
- [ ] **Connection pool mutex exception**: Pool classes must use `std::mutex` (not `recursive_mutex`) because `std::condition_variable` requires it

### Input Validation
- [ ] **Database identifier validation**: Validate keyspace/database names against injection (only alphanumeric + underscores)

### Error Handling
- [ ] **`std::expected` / `std::optional`**: Always `.has_value()` for checks
- [ ] **Private/protected methods**: Nothrow only (`std::nothrow_t` + `noexcept` + `std::expected`), no throwing variant
- [ ] **Infallible private/protected helpers**: May return natural type (`void`, `bool`, etc.) instead of `std::expected` when provably infallible; still require `std::nothrow_t` + `noexcept`
- [ ] **Prefer nothrow variants**: When both throwing and nothrow overloads exist, always call the nothrow one
- [ ] **Nothrow methods call nothrow overloads**: No throwing inner calls from nothrow methods
- [ ] **No redundant try/catch**: Dead catch blocks removed when all inner calls are nothrow (death-sentence exceptions do NOT justify try/catch)
- [ ] **`catch(...)` preceded by `catch(const std::exception &ex)`**: Always
- [ ] **Catch variable naming**: Always `ex` (or `ex1`, `ex2` for nested)

### Class Layout & Patterns
- [ ] **Redundant `private:` in `class`**: Must be omitted (required in `struct`)
- [ ] **Standard class layout — canonical order**: Items 1-13 in the mandatory order: PrivateCtorTag → member variables → construction state → private helpers → protected members → protected helpers → public constructor → destructor → deleted copy/move → throwing block (`#ifdef`) → nothrow block. Applies to ALL classes with dual throw/nothrow API
- [ ] **Header public section — two blocks**: All throwing methods in a single `#ifdef __cpp_exceptions` block first, then all nothrow methods in a separate block after `#endif`. No interleaving, no multiple `#ifdef` blocks, no throwing methods outside the guard, no nothrow methods inside it. Order within throwing block must match order within nothrow block
- [ ] **No private constructors**: All constructors must be public, guarded by `PrivateCtorTag`; private constructors must be migrated
- [ ] **PrivateCtorTag pattern (per-class)**: Correctly implemented in driver classes (Connection, PreparedStatement, ResultSet, Blob, InputStream, Cursor, Collection, Document); tag defined as private struct in the class itself
- [ ] **Construction state variables**: `bool m_initFailed{false}` + `std::unique_ptr<DBException> m_initError{nullptr}` in per-class PrivateCtorTag classes
- [ ] **PrivateCtorTag constructor try/catch**: Only present when body contains recoverable C++ exceptions; comment above `try` listing which exceptions are caught; omitted when body is entirely nothrow or only death-sentence exceptions
- [ ] **Shared PrivateCtorTag (pool classes)**: Single `PrivateCtorTag` defined as `protected` in `DBConnectionPool` base class, shared across the entire pool hierarchy; no `std::nothrow_t` needed; tag must be named `PrivateCtorTag` (not `ConstructorTag`)
- [ ] **DBDriver singleton**: PrivateCtorTag + `s_instance` shared_ptr + `getInstance()` + connection registry; constructor is `noexcept` with `m_initFailed`/`m_initError`
- [ ] **Static factory `::create`**: Throwing delegates to nothrow; `#ifdef __cpp_exceptions` guards
- [ ] **`std::move` in factory error paths**: `std::move(*obj->m_initError)` not a copy
- [ ] **`#ifdef __cpp_exceptions`**: All throwing code properly guarded
- [ ] **Public methods — dual API**: Public-facing classes (drivers, pools, connections, statements, result sets, blobs, input streams) must have both throwing and nothrow APIs. Internal implementation classes (RAII helpers, registries, internal structs) may omit the throwing API entirely — only the nothrow API is required
- [ ] **Source file distribution**: `_01.cpp` / `_02.cpp` / `_03.cpp` pattern followed
- [ ] **Definition order matches declaration order**: Method definitions in `.cpp` files must appear in the same order as their declarations in the `.hpp` header. Private/protected helpers first, then throwing block (`#ifdef __cpp_exceptions`), then nothrow methods — all following the top-to-bottom header order within each group

### Naming Conventions
- [ ] **Namespace naming**: Nested under `cpp_dbc` with brand's official casing (`cpp_dbc::MySQL`, `cpp_dbc::PostgreSQL`, etc.)
- [ ] **Class naming**: `<BrandName><RoleSuffix>` pattern; "DB" not doubled when brand already contains it (`MongoDBDriver`, not `MongoDBDBDriver`)
- [ ] **Header file naming**: Lowercase names (`connection.hpp`, `driver.hpp`, `prepared_statement.hpp`, etc.)
- [ ] **Umbrella header**: `driver_<lowercase_name>.hpp` with `#ifndef` include guard and `#if USE_<DRIVER>` conditional

---

## Output Location — MANDATORY File on Disk

> **THIS IS A BLOCKING REQUIREMENT. NO EXCEPTIONS. NO ALTERNATIVES.**

Every convention compliance analysis **MUST** produce a `.md` file written to disk at the following path:

```text
research/<driver_name>_improvements_<N>.md
```

Where `<N>` is an incrementing number (1, 2, 3...) to allow multiple analysis passes on the same driver. Example: `research/mysql_driver_improvements_7.md`, `research/firebird_driver_improvements_2.md`.

### What counts as a valid report

The **only** valid deliverable is a `.md` file physically written to disk using the `Write` tool. The following do **NOT** satisfy this requirement:

- Showing the report in the chat/terminal output — **NOT VALID**
- Summarizing findings verbally — **NOT VALID**
- Printing findings to stdout — **NOT VALID**
- Storing findings only in memory or context — **NOT VALID**
- Promising to write the file later — **NOT VALID**

If the file does not exist on disk at `research/<driver_name>_improvements_<N>.md` after the analysis, the analysis is **incomplete and must not be considered done**.

### Workflow

1. Read all files in scope
2. Analyze against conventions
3. **Write the report file to disk** using the `Write` tool — this step is not optional
4. Confirm to the user that the file was written and provide the path

### Immutability rule

Existing report files must **never** be modified or overwritten. Each new analysis pass must create a new file with the next incremental number. If `research/sqlite_driver_improvements_2.md` already exists, the next report must be `research/sqlite_driver_improvements_3.md`. Previous reports serve as historical record of the driver's compliance evolution.
