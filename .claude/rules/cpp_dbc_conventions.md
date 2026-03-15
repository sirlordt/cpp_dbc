# cpp_dbc Project Conventions

## DBException Error Codes

Error codes in `DBException` must be:
- **Format**: 12-character uppercase alphanumeric (A–Z, 0–9) string with at least 5 letters and no more than 4 consecutive repeated characters
- **Uniqueness**: Each code must be unique across the entire project
- **Generation**: Random (not sequential)

**Scripts**:

1. **Generate codes**: `./generate_dbexception_code.sh` generates unique codes:
   ```bash
   # Generate 1 unique code
   ./generate_dbexception_code.sh /path/to/project

   # Generate N unique codes
   ./generate_dbexception_code.sh /path/to/project N
   ```

2. **Check/Fix duplicates**: `./check_dbexception_codes.sh` manages code uniqueness:
   ```bash
   # Check for problematic duplicates (different files with same code)
   ./check_dbexception_codes.sh --check

   # List all codes with occurrence count
   ./check_dbexception_codes.sh --list

   # Auto-fix problematic duplicates
   ./check_dbexception_codes.sh --fix
   ```

```cpp
// Correct
throw DBException("7K3F9J2B5Z8D", "Error message", system_utils::captureCallStack());

// Incorrect
throw DBException("INVALID_KEYSPACE", "Error message", ...);  // Not alphanumeric format
throw DBException("001", "Error message", ...);               // Too short
```

## Code Formatting

- **Indentation**: 4 spaces per level (no tabs)

### Preprocessor Directives — Always Flush Left (Column 0)

All preprocessor directives (`#ifdef`, `#ifndef`, `#if`, `#else`, `#elif`, `#endif`, `#define`, `#include`, `#pragma`) must start at column 0 (no indentation), regardless of the surrounding code's nesting level. This is a universal C/C++ convention and is required by the project:

```cpp
// Correct — directives at column 0, even inside a class or function
class MyClass
{
    int m_value{0};

#ifdef __cpp_exceptions
    void doSomething();
#endif

    void doOtherThing(std::nothrow_t) noexcept;
};

// Correct — nested #if/#endif, all at column 0
#if USE_MYSQL
#ifdef __cpp_exceptions
    static std::shared_ptr<MySQLDBDriver> create();
#endif
#endif

// Incorrect — indented to match surrounding code
class MyClass
{
    int m_value{0};

    #ifdef __cpp_exceptions          // WRONG: indented
        void doSomething();
    #endif                           // WRONG: indented

    void doOtherThing(std::nothrow_t) noexcept;
};
```

This applies only to new or modified code. Pre-existing indented directives do not need to be updated retroactively.

### Braces — Always Required

Every control flow statement (`if`, `else`, `while`, `for`, `do`, `switch`, `case`) or any other C++ syntax block **must** use braces, even for single-line bodies:

```cpp
// Correct
if (condition)
{
    doSomething();
}

// Incorrect — braces missing
if (condition)
    doSomething();
```

### Braces — Allman Style (Own Line)

Opening and closing braces **must** each be on their own line. Placing a brace on the same line as a statement, keyword, or return expression is forbidden. This applies to every C++ construct that uses braces: `if`, `else`, `while`, `for`, `do`, `switch`, `case`, `try`, `catch`, functions, lambdas, classes, structs, namespaces, and any other compound statement.

```cpp
// Correct — each brace on its own line
catch (const DBException &ex)
{
    return cpp_dbc::unexpected(ex);
}

// Correct
if (condition)
{
    doSomething();
}
else
{
    doOtherThing();
}

// Incorrect — opening brace on same line as keyword
catch (const DBException &ex) {
    return cpp_dbc::unexpected(ex);
}

// Incorrect — entire block collapsed to one line
catch (const DBException &ex)
{ return cpp_dbc::unexpected(ex); }

// Incorrect — entire block collapsed to one line
catch (const DBException &ex) { return cpp_dbc::unexpected(ex); }

// Incorrect — closing brace on same line as statement
if (condition)
{
    doSomething(); }
```

This applies only to new or modified code. Pre-existing brace placement does not need to be updated retroactively.

### Empty Braces — Always Documented

An intentionally empty compound statement must contain a comment explaining why:

```cpp
// Correct
catch (...)
{
    // Intentionally empty — exception silenced during cleanup path
}

// Incorrect — bare empty braces
catch (...) {}
```

### NOSONAR Comments — Rule ID and Explanation Required

Every `// NOSONAR` annotation must include the SonarQube rule ID in parentheses and a brief English explanation of why the suppression is justified, all on a single line:

```cpp
// Correct — rule ID + explanation on one line
catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
{
    // ...
}

m_pool.reset(); // NOSONAR(cpp:S5weakptr) — intentional release of shared ownership during shutdown

// Incorrect — missing rule ID
catch (...) // NOSONAR — fallback for non-std exceptions
{
}

// Incorrect — missing explanation
catch (...) // NOSONAR(cpp:S2738)
{
}

// Incorrect — no annotation at all
catch (...)
{
}
```

This applies only to new or modified code. Pre-existing NOSONAR comments do not need to be updated retroactively.

### Bug-Fix Comments — Always Required

Every code block that exists to fix a bug or patch a problem **must** include a structured comment with an ISO 8601 timestamp, problem description, and solution. Each line is at most 128 characters. Always in English:

```text
// YYYY-MM-DDTHH:mm:ssZ (ISO 8601, UTC timezone)
// Bug: <problem description line 1, max 128 chars>
// <problem description continuation line 2, max 128 chars>
// <problem description continuation line 3, max 128 chars>
// Solution: <solution description line 1, max 128 chars>
// <solution description continuation line 2, max 128 chars>
```

The `Bug:` and `Solution:` prefixes are mandatory. Problem lines (up to 3) describe the root cause or symptom. Solution lines (up to 2) describe what the fix does. Additional continuation lines without a prefix belong to the preceding `Bug:` or `Solution:` block.

```cpp
// Correct — simple fix
// 2026-03-07T14:30:00Z
// Bug: setActive(false) was called unconditionally before the handoff decision,
// so when a waiter takes the connection directly, the active count is wrong.
// Solution: Restore active state (setActive(true) + m_activeConnections++) before
// setting req->fulfilled = true in the direct-handoff path.
conn->setActive(true);
m_activeConnections++;

// Correct — complex workaround
// 2026-03-05T09:15:00Z
// Bug: Helgrind cannot track POSIX file locks used by SQLite internally. ThreadSanitizer
// reports 132 false-positive data races because it sees concurrent memory access
// without any visible synchronization primitive.
// Solution: FileMutexRegistry provides a shared std::recursive_mutex per database file,
// making synchronization visible to sanitizers. Compiled out in production via
// ENABLE_FILE_MUTEX_REGISTRY=OFF (zero overhead).
std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

// Incorrect — no timestamp
// Bug: Connection leak when pool is exhausted.
// Solution: Return connection before timeout.
conn->returnToPool();

// Incorrect — free-form comment without Bug:/Solution: structure
// Restore active state before handoff: setActive(false) was called unconditionally
// above, so it must be undone here when a waiter takes the connection directly.
conn->setActive(true);
m_activeConnections++;
```

This format applies only to new or modified code. Pre-existing bug-fix comments do not need to be updated retroactively.

## Namespace Style

Use C++17 nested namespace syntax:

```cpp
// Correct
namespace cpp_dbc::MySQL
{
} // namespace cpp_dbc::MySQL

// Incorrect - do NOT use old-style nested namespaces
namespace cpp_dbc
{
    namespace MySQL
    {
    } // namespace MySQL
} // namespace cpp_dbc
```

## Unused Parameters — No `(void)` Cast

When a function parameter is intentionally unused, suppress the compiler warning using one of the two allowed techniques. The C-style `(void)param;` cast is **forbidden** — it is a pre-C++17 idiom that adds noise and is unnecessary in modern C++.

**Allowed techniques** (in order of preference):

1. **Omit the parameter name** (declaration and definition) — preferred when the parameter is never referenced:

```cpp
// Correct — unnamed parameter in definition
cpp_dbc::expected<std::string, DBException> ScyllaDBDriver::buildURI(
    std::nothrow_t,
    const std::string &host,
    int port,
    const std::string &database,
    const std::map<std::string, std::string> & /* options */) noexcept
{
    return "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) +
           (database.empty() ? "" : "/" + database);
}
```

2. **`[[maybe_unused]]` attribute** — preferred when the parameter might be used conditionally (e.g. behind `#ifdef`) or when omitting the name would hurt readability:

```cpp
// Correct — [[maybe_unused]] attribute
cpp_dbc::expected<std::string, DBException> ScyllaDBDriver::buildURI(
    std::nothrow_t,
    const std::string &host,
    int port,
    const std::string &database,
    [[maybe_unused]] const std::map<std::string, std::string> &options) noexcept
{
    return "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) +
           (database.empty() ? "" : "/" + database);
}
```

**Forbidden**:

```cpp
// Incorrect — C-style void cast
cpp_dbc::expected<std::string, DBException> ScyllaDBDriver::buildURI(
    std::nothrow_t,
    const std::string &host,
    int port,
    const std::string &database,
    const std::map<std::string, std::string> &options) noexcept
{
    (void)options;  // WRONG: use unnamed parameter or [[maybe_unused]] instead
    // ...
}
```

This applies only to new or modified code. Pre-existing `(void)param;` casts do not need to be updated retroactively.

## Shared Utilities and Constants — Centralization Rule

All reusable functions and constants that are used (or could potentially be used) across multiple files in the library **must** be centralized in the common utilities:

- **Functions** → `libs/cpp_dbc/include/cpp_dbc/common/system_utils.hpp` (header) / `libs/cpp_dbc/src/common/system_utils.cpp` (implementation)
- **Constants** → `libs/cpp_dbc/include/cpp_dbc/common/system_constants.hpp` (header) / `libs/cpp_dbc/src/common/system_constants.cpp` (translation unit)

**Namespaces**:
- Functions live in `cpp_dbc::system_utils`
- Constants live in `cpp_dbc::system_constants`

**Before implementing any new utility function or constant**, you **must**:

1. Check whether the function already exists in `system_utils.hpp` — avoid reimplementing what is already available.
2. Check whether the constant already exists in `system_constants.hpp` — avoid defining local duplicates.
3. If it does not exist and is potentially reusable, add it to the appropriate shared file instead of defining it locally.

**Forbidden patterns**:

```cpp
// Incorrect — local constexpr that duplicates a shared constant
constexpr std::string_view CPP_DBC_PREFIX = "cpp_dbc:";  // WRONG: use system_constants::URI_PREFIX

// Incorrect — inline utility function defined locally in a driver file
static std::string sanitize(const std::string &s) { ... }  // WRONG: belongs in system_utils if reusable

// Incorrect — magic literal instead of shared constant
if (url.starts_with("cpp_dbc:"))  // WRONG: use system_constants::URI_PREFIX
```

**Correct usage**:

```cpp
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// From within namespace cpp_dbc or a sub-namespace:
if (url.starts_with(cpp_dbc::system_constants::URI_PREFIX))
{
    url = url.substr(cpp_dbc::system_constants::URI_PREFIX.size());
}
```

This applies only to new or modified code. Pre-existing local duplicates do not need to be updated retroactively.

## Memory Safety

- Use RAII handles for external resources (BsonHandle, RedisReplyHandle, etc.)
- Defensively check for nulls before dereferencing pointers from external libraries
- Use `std::atomic` with explicit memory ordering (`memory_order_acquire`/`release`)
- Avoid raw pointers. Use `std::shared_ptr`, `std::unique_ptr`, and `std::weak_ptr` where possible, especially in member/class variables.
- Always try to use C++17-style constructs and functions for more secure code.
- Use ranges and avoid index loops where possible.

### `const_cast` — Avoid at All Costs

`const_cast` to remove `const` qualification from a pointer or reference can lead to **undefined behavior** if the underlying object was originally declared `const`. It also defeats the compiler's ability to enforce immutability, making the code fragile and harder to reason about.

`const_cast` must be avoided in the codebase. Before resorting to it, exhaust every alternative:

1. **Fix the API**: If a function takes a non-const pointer but does not modify the object, change its signature to accept `const`.
2. **Use `mutable`**: If a member needs to be modified in a `const` method (e.g. a cache or mutex), declare it `mutable`.
3. **Redesign the data flow**: If `const_cast` seems necessary, it usually indicates a design flaw — the const-correctness chain is broken somewhere upstream.

`const_cast` is permitted **only** as a last resort when interfacing with a third-party C or C++ API that has an incorrect signature (takes non-const but guarantees no mutation), and only with a comment explaining why it is safe:

```cpp
// Correct — last resort, documented justification
// legacy_api_read() takes char* but is documented as read-only (does not modify the buffer).
// The library does not provide a const-correct overload.
legacy_api_read(const_cast<char *>(data.c_str()), data.size());

// Incorrect — const_cast without justification
auto *mutablePtr = const_cast<MyClass *>(constPtr);
mutablePtr->setValue(42);  // UB if constPtr points to a const object

// Incorrect — const_cast to work around own API
const auto &ref = getConfig();
const_cast<Config &>(ref).setDebug(true);  // WRONG: fix getConfig() or setDebug() instead
```

**Real-world example — MySQL ResultSet materialized mode** (SonarQube cpp:S859):

`MYSQL_ROW` is a MySQL C API typedef for `char**` (non-const). In materialized mode, row data lives in `std::vector<std::optional<std::string>>`, and `std::string::c_str()` returns `const char*`. The `const_cast` bridges the const mismatch to reuse the same `m_currentRow` (`MYSQL_ROW`) interface for both native and materialized rows:

```cpp
// Problematic — const_cast to satisfy MYSQL_ROW (char**) interface
const auto &row = m_materializedRows[m_rowPosition];
for (size_t i = 0; i < m_fieldCount; ++i)
{
    if (row[i].has_value())
    {
        // c_str() returns const char*, but MYSQL_ROW expects char*
        m_currentRowPtrs[i] = const_cast<char *>(row[i].value().c_str());  // SonarQube cpp:S859
        m_currentLengths[i] = static_cast<unsigned long>(row[i].value().size());
    }
}
m_currentRow = m_currentRowPtrs.data();  // MYSQL_ROW = char**

// Better — change m_currentRowPtrs to const char* and avoid const_cast entirely
std::vector<const char *> m_currentRowPtrs;  // const-correct pointer storage
// ...
m_currentRowPtrs[i] = row[i].value().c_str();  // no cast needed
```

The preferred fix is to redesign the internal storage to use `const char*` instead of `char*`, eliminating the need for `const_cast` altogether. The `const_cast` is only acceptable as a temporary measure when the third-party typedef cannot be changed and all access through those pointers is provably read-only.

This applies only to new or modified code. Pre-existing uses of `const_cast` should be migrated when encountered and the case applies.

### Member Initialization — Prefer In-Class Initializers Over Constructor Body

When possible, initialize member variables with in-class default member initializers (`= value` or `{value}`) rather than assigning them in the constructor body. This eliminates redundant constructor code, ensures consistent initialization across all constructors, and makes the class definition self-documenting.

If a member depends on another member declared earlier in the same class, in-class initialization is still valid — members are initialized in declaration order, so the dependency is safe as long as the order is respected:

```cpp
// Correct — in-class initializers, order-safe
class PooledConnection
{
    std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
    std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()}; // safe: m_creationTime declared first
    std::atomic<bool> m_active{false};

public:
    PooledConnection() = default;
};

// Incorrect — assignment in constructor body when in-class init is possible
class PooledConnection
{
    std::chrono::time_point<std::chrono::steady_clock> m_creationTime;
    std::atomic<int64_t> m_lastUsedTimeNs{0}; // wrong initial value; fixed in constructor
    std::atomic<bool> m_active{false};

public:
    PooledConnection()
    {
        m_creationTime = std::chrono::steady_clock::now(); // redundant if in-class init is possible
        m_lastUsedTimeNs.store(m_creationTime.time_since_epoch().count(), std::memory_order_relaxed);
    }
};
```

Constructor body assignment is only appropriate when the initial value depends on a constructor parameter or cannot be expressed as a constant or simple expression.

### Empty Constructors — Prefer `= default` Over `{}`

When a constructor body is empty (no statements), always use `= default` instead of an empty brace pair `{}`. The `= default` form is explicit about intent, allows the compiler to generate a trivial constructor when possible, and is the idiomatic C++11+ way to express "use the default behavior":

```cpp
// Correct — = default
class MyClass
{
    int m_value{0};
    std::string m_name{"default"};

public:
    MyClass() = default;
};

// Correct — = default with noexcept
struct PrivateCtorTag
{
    explicit PrivateCtorTag() = default;
};

// Incorrect — empty braces
class MyClass
{
    int m_value{0};
    std::string m_name{"default"};

public:
    MyClass() {}  // WRONG: use = default
};

// Incorrect — empty braces with comment
class MyClass
{
public:
    MyClass() { /* nothing to initialize */ }  // WRONG: use = default
};
```

**When `= default` does NOT apply**: If the constructor body contains any statement — even a single line such as an atomic store, a log call, or any side effect — `= default` is not applicable. Only truly empty bodies must use `= default`.

This applies only to new or modified code. Pre-existing empty constructors do not need to be updated retroactively.

### `std::atomic` — Always Use `.load(std::memory_order_acquire)`

Every read of a `std::atomic` variable **must** call `.load(std::memory_order_acquire)` explicitly. No exceptions. Implicit boolean conversion, bare `.load()` without memory order, and comparison operators that bypass `.load()` are all forbidden:

```cpp
// Correct
if (m_running.load(std::memory_order_acquire))
{
    // ...
}

if (m_closed.load(std::memory_order_acquire))
{
    return cpp_dbc::unexpected(...);
}

// Incorrect — implicit conversion, no memory ordering
if (m_running)         { }  // implicit bool conversion
if (!m_closed)         { }  // implicit bool conversion
if (m_running.load())  { }  // missing memory_order argument
```

The only exception is `std::atomic::store()`, `exchange()`, and `compare_exchange_*()`, which use `std::memory_order_release` or `std::memory_order_acq_rel` as appropriate for write operations.

### String-to-Number Conversion — Always Use `std::from_chars`

When converting strings to numeric types (`int`, `long`, `double`, etc.), always use `std::from_chars` (C++17, `<charconv>`). It is `noexcept`, does not allocate, does not throw, and reports errors via `std::errc` — making it the only safe choice in `noexcept` methods and the preferred choice everywhere else.

The following alternatives are **forbidden**:

| Function | Why forbidden |
|----------|---------------|
| `std::stoi`, `std::stol`, `std::stod`, etc. | Throw `std::invalid_argument` or `std::out_of_range` — incompatible with `noexcept` and `-fno-exceptions` |
| `std::atoi`, `std::atol`, `std::atof` | Undefined behavior on out-of-range input; no error reporting; C legacy |
| `std::strtol`, `std::strtod` | Locale-dependent, requires `errno` checking, C legacy |
| `std::sscanf` | Locale-dependent, format-string parsing overhead, C legacy |

```cpp
// Correct — std::from_chars, noexcept-safe, no locale, explicit error check
int port = 0;
auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
if (ec != std::errc{})
{
    return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX",
        "Invalid port number: " + portStr,
        system_utils::captureCallStack()));
}

// Incorrect — std::stoi throws on invalid input
int port = std::stoi(portStr);  // WRONG: throws std::invalid_argument or std::out_of_range

// Incorrect — std::atoi has no error reporting, UB on overflow
int port = std::atoi(portStr.c_str());  // WRONG: silent failure on invalid input
```

This applies only to new or modified code. Pre-existing uses of `std::stoi`/`std::atoi` do not need to be updated retroactively.

### String Containment — Prefer `contains()` Over `find()`

When checking whether a `std::string` (or `std::string_view`) contains a substring, always use `.contains()` (C++23). It expresses intent directly and avoids the error-prone `!= std::string::npos` idiom:

```cpp
// Correct — clear intent, no magic constant
if (url.contains("://"))
{
    // ...
}

if (!errorMsg.contains("timeout"))
{
    // ...
}

// Incorrect — find() + npos comparison for a simple containment check
if (url.find("://") != std::string::npos)  // WRONG: use .contains()
{
    // ...
}

if (errorMsg.find("timeout") == std::string::npos)  // WRONG: use !.contains()
{
    // ...
}
```

**When `find()` is still appropriate**: Use `find()` only when you need the **position** of the substring (e.g. to extract or split around it). If you only need a boolean yes/no answer, `.contains()` is the correct choice.

**Migration**: Pre-existing uses of `find()` for containment checks must be migrated to `.contains()` when encountered and the case applies. Pre-existing uses of `find()`/`substr()`/`rfind()`/`compare()` for prefix/suffix checks must be migrated to `.starts_with()` / `.ends_with()` when encountered and the case applies (see next section).

### String Prefix/Suffix — Prefer `starts_with()` / `ends_with()` Over `find()` or `substr()`

When checking whether a `std::string` (or `std::string_view`) starts or ends with a specific prefix or suffix, always use `.starts_with()` / `.ends_with()` (C++20). They are `noexcept`, express intent clearly, and avoid off-by-one errors and unnecessary temporaries:

```cpp
// Correct — starts_with / ends_with
if (url.starts_with("cpp_dbc:"))
{
    // ...
}

if (filename.ends_with(".cpp"))
{
    // ...
}

// Incorrect — find() for prefix check
if (url.find("cpp_dbc:") == 0)  // WRONG: use .starts_with()
{
    // ...
}

// Incorrect — substr() for prefix check
if (url.substr(0, 8) == "cpp_dbc:")  // WRONG: use .starts_with(); substr creates a temporary
{
    // ...
}

// Incorrect — rfind() + length arithmetic for suffix check
if (filename.rfind(".cpp") == filename.size() - 4)  // WRONG: use .ends_with()
{
    // ...
}

// Incorrect — compare() with length arithmetic for suffix check
if (filename.compare(filename.size() - 4, 4, ".cpp") == 0)  // WRONG: use .ends_with()
{
    // ...
}
```

**Migration**: Pre-existing uses of `find()`/`substr()`/`rfind()`/`compare()` for prefix/suffix checks must be migrated to `.starts_with()` / `.ends_with()` when encountered and the case applies. Pre-existing uses of `find()` for containment checks must be migrated to `.contains()` when encountered and the case applies.

## Thread Safety

- Use `DB_DRIVER_LOCK_GUARD(m_mutex)` macro for conditional locking
- The `DB_DRIVER_THREAD_SAFE` flag controls whether mutexes are active
- Prefer `std::scoped_lock` over other lock guard types (`lock_guard`, `unique_lock`) wherever it makes sense, as it avoids deadlocks and is safer by default
- Prefer `std::recursive_mutex` over other mutex types (`mutex`, `timed_mutex`) wherever it makes sense, to allow re-entrant locking within the same thread without deadlock

### Exception: Connection Pool Mutex

Connection pool classes (`*DBConnectionPool`) **must** use `std::mutex` (not `std::recursive_mutex`) for their pool-level mutex (`m_mutexPool`). This is required because `std::condition_variable` only works with `std::mutex` via `std::unique_lock<std::mutex>` — it does not support `std::recursive_mutex`. Since the pool's wait/notify mechanism (`m_maintenanceCondition`, per-waiter `ConnectionRequest::cv`) depends on `std::condition_variable`, `std::mutex` is the correct and only valid choice here.

## Input Validation

- Validate database identifiers (keyspace, database names) against injection
- Only allow alphanumeric characters and underscores in schema names

## Error Handling

### `std::optional` and `std::expected` — Always Use `.has_value()`

When checking whether an `optional` or `expected` holds a value, always call `.has_value()` explicitly. Implicit boolean conversion is ambiguous and flagged by static analysis (SonarQube cpp:S7172):

```cpp
// Correct
if (!result.has_value())
{
    throw result.error();
}

// Incorrect — implicit bool conversion, ambiguous intent
if (!result)
{
    throw result.error();
}
```

### Private and Protected Methods — Return `std::expected<T, DBException>` and Are `noexcept`

Internal class methods (`private` or `protected`) must:
1. Be declared `noexcept` — making it explicit and compiler-enforced that the method never throws
2. Return `std::expected<T, DBException>` instead of throwing exceptions — keeping error paths visible to the caller
3. **Never have a throwing version** — the dual throw/nothrow pattern does NOT apply here. Only the nothrow version exists.

This eliminates hidden control flow, makes error propagation explicit, and prevents exceptions from escaping internal implementation boundaries.

**Exception — protected override methods**: When a base class (abstract interface) declares both a throwing and a nothrow version of a `virtual` method, the derived class **must** override both versions to satisfy the interface contract. In this case, the `protected override` method follows the same dual throw/nothrow pattern as `public` methods: the throwing override delegates to the nothrow override. This is not a design choice of the derived class — it is a requirement imposed by the base class.

**The dual throw/nothrow pattern (two versions of the same method) applies exclusively to:**
- `public` methods
- `public override` methods
- `protected override` methods (overriding a base class interface that declares both versions)

**Private methods and non-override protected methods must only have the nothrow version.** A throwing variant for internal methods is unnecessary — there is no external caller that needs it, and it adds dead code. Internal callers always use the nothrow version and check `.has_value()`.

**Migration rule**: If an existing private or non-override protected method is found with a throwing signature (no `std::nothrow_t`, no `noexcept`, may throw), it **must** be converted to the nothrow pattern: add `std::nothrow_t` as first parameter, declare `noexcept`, and return `std::expected<T, DBException>` if it contains fallible operations. The throwing version must be removed entirely — it is not kept for backwards compatibility. **All call sites must be updated** to reflect the new signature: pass `std::nothrow` as first argument and handle the `std::expected` return value via `.has_value()` and `.error()` instead of try/catch. Note: `.value()` throws `std::bad_expected_access` if the expected holds an error, so it must only be called after a preceding `.has_value()` check confirms success. After migration, if all inner calls at a call site are now nothrow and no code can throw exceptions other than death sentences (`std::bad_alloc`, mutex `std::system_error`), any surrounding try/catch blocks become dead code and **must be removed** — see [No Redundant try/catch in Nothrow Methods](#no-redundant-trycatch-in-nothrow-methods).

```cpp
// Correct — private method exists ONLY as nothrow
    std::expected<void, DBException> doWorkInternal(std::nothrow_t) noexcept;

// Incorrect — private method has a throwing version (unnecessary, dead code)
    void doWorkInternal();  // WRONG: throwing variant of a private method
    std::expected<void, DBException> doWorkInternal(std::nothrow_t) noexcept;

// Correct — public method has BOTH versions (throw delegates to nothrow)
public:
#ifdef __cpp_exceptions
    void connect();  // throwing wrapper
#endif
    std::expected<void, DBException> connect(std::nothrow_t) noexcept;  // real logic

// Correct — protected override has BOTH versions (base class defines both)
protected:
#ifdef __cpp_exceptions
    void onPoolReturn() override;  // throwing wrapper
#endif
    std::expected<void, DBException> onPoolReturn(std::nothrow_t) noexcept override;
```

The first parameter must always be `std::nothrow_t`. This makes it explicit at every call site that the method never throws — the caller must pass `std::nothrow` to invoke it:

```cpp
// Correct — private method takes std::nothrow_t as first parameter, is noexcept, returns expected
private:
    std::expected<ConnectionPtr, DBException> createConnectionInternal(std::nothrow_t) noexcept;
    std::expected<void, DBException> validateConnectionInternal(std::nothrow_t, const ConnectionPtr &conn) noexcept;

// Usage at the call site (within the class) — std::nothrow makes intent explicit
auto result = createConnectionInternal(std::nothrow);
if (!result.has_value())
{
    return std::unexpected(result.error());  // propagate up
}

// Incorrect — no std::nothrow_t parameter, no noexcept, throwing return type
private:
    ConnectionPtr createConnectionInternal();  // may throw, hidden from caller
```

#### Exception: Infallible Private/Protected Helpers

Private or protected methods that are **provably infallible** — composed entirely of `noexcept` operations with no possible error path — may return their natural type (`void`, `bool`, etc.) instead of `std::expected`. They must still take `std::nothrow_t` as the first parameter and be declared `noexcept`.

A method qualifies as infallible when **every** operation in its body is guaranteed `noexcept` by the C++ standard:

| Pattern | Why infallible |
|---------|---------------|
| `cv.notify_one()` / `cv.notify_all()` | `noexcept` by standard |
| `std::atomic<T>::load()` / `store()` | `noexcept` by standard |
| `std::weak_ptr::expired()` | `noexcept` by standard |
| Thread entry point (no caller to check return) | Return value is discarded |

```cpp
// Correct — infallible helper, returns void directly
void notifyFirstWaiter(std::nothrow_t) noexcept;
void updateLastUsedTime(std::nothrow_t) noexcept;
bool isPoolValid(std::nothrow_t) const noexcept;
bool isActive(std::nothrow_t) const noexcept;

// Correct — thread entry point, no caller checks return
void maintenanceTask(std::nothrow_t) noexcept;

// Incorrect — method CAN fail (I/O, external call), must return expected
void connectInternal(std::nothrow_t) noexcept;  // wrong: should return expected
```

**Rule of thumb**: if adding `std::expected` would force the caller to write `.has_value()` on something that can **never** return an error, the wrapping is dead code and should be omitted.

If the method wraps an external library call that can throw, catch at the boundary and convert to `std::unexpected`. The `noexcept` on the wrapper remains valid because no exception escapes it:

```cpp
// Correct — noexcept wrapper with std::nothrow_t that absorbs external exceptions
std::expected<void, DBException> MyClass::connectInternal(std::nothrow_t) noexcept
{
    try
    {
        m_handle = ExternalLib::connect(m_url);
    }
    catch (const std::exception &ex)
    {
        return std::unexpected(DBException("A1B2C3D4E5F6", ex.what(), system_utils::captureCallStack()));
    }
    catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
    {
        return std::unexpected(DBException("G7H8I9J0K1L2", "Unknown error during connect", system_utils::captureCallStack()));
    }
    return {};
}
```

### Prefer `noexcept` / `std::nothrow` Variants

When a method has both a throwing variant and a non-throwing variant (`noexcept` or accepting `std::nothrow_t`), always prefer the non-throwing one. This avoids unnecessary try/catch boilerplate and produces cleaner, more predictable code:

```cpp
// Correct — prefer the nothrow overload, then check the result
auto closeResult = conn->close(std::nothrow);
if (!closeResult.has_value())
{
    CP_DEBUG("close() failed: " + closeResult.error().what());
}

// Incorrect — throwing overload forces a try/catch
try
{
    conn->close();
}
catch (const std::exception &ex)
{
    CP_DEBUG("close() failed: " + std::string(ex.what()));
}
catch (...)
{
    // Intentionally empty
}
```

### Nothrow Methods Must Call Nothrow Overloads

A method declared `noexcept` or accepting `std::nothrow_t` **must** call the `std::nothrow` overload of any inner method that has one. Calling the throwing overload from within a nothrow method is a violation, even if surrounded by a try/catch — it defeats the purpose of the nothrow contract and introduces hidden control flow:

```cpp
// Correct — nothrow method calls nothrow overload, propagates expected
cpp_dbc::expected<bool, DBException> MyPooledConnection::isClosed(std::nothrow_t) const noexcept
{
    if (m_closed.load(std::memory_order_acquire))
    {
        return true;
    }
    auto result = m_conn->isClosed(std::nothrow);
    if (!result.has_value())
    {
        return result;
    }
    return result.value();
}

// Incorrect — nothrow method calls throwing overload
cpp_dbc::expected<bool, DBException> MyPooledConnection::isClosed(std::nothrow_t) const noexcept
{
    return m_closed.load(std::memory_order_acquire) || m_conn->isClosed();  // isClosed() can throw!
}
```

The only exception is when **no nothrow overload exists** for the inner method. In that case, the nothrow method must wrap the call in try/catch and convert any exception to `std::unexpected`.

### No Redundant try/catch in Nothrow Methods

A `noexcept` / `std::nothrow_t` method that calls **only** nothrow overloads (functions that return `expected` and never throw) must **not** wrap the body in try/catch. Since none of the inner calls can throw, the catch blocks are dead code and create false noise — they suggest that exceptions are possible when they are not:

```cpp
// Correct — no try/catch needed; all inner calls are nothrow
cpp_dbc::expected<std::shared_ptr<PreparedStatement>, DBException>
MyConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
{
    if (m_closed.load(std::memory_order_acquire))
    {
        return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX", "Connection is closed", system_utils::captureCallStack()));
    }
    updateLastUsedTime(std::nothrow);
    return m_conn->prepareStatement(std::nothrow, query);
}

// Incorrect — try/catch is dead code; all inner calls are nothrow and cannot throw
cpp_dbc::expected<std::shared_ptr<PreparedStatement>, DBException>
MyConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
{
    try
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->prepareStatement(std::nothrow, query);
    }
    catch (const DBException &ex)                  // unreachable
    {
        return cpp_dbc::unexpected(ex);
    }
    catch (const std::exception &ex)               // unreachable
    {
        return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX", ex.what(), system_utils::captureCallStack()));
    }
    catch (...)                                    // unreachable
    {
        return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX", "unknown error", system_utils::captureCallStack()));
    }
}
```

The try/catch is **only** required when at least one inner call has no nothrow overload and can actually throw.

#### Diagnostic Checklist — Is the try/catch Dead Code?

Walk every statement inside the `try` block and ask: *"Can this expression throw a **recoverable** exception?"* The key word is **recoverable**. If **every** answer is NO, the entire try/catch is dead code and must be removed.

| Expression pattern | Recoverable throw? | Reason |
|--------------------|--------------------|--------|
| `someMethod(std::nothrow)` | **NO** | Takes `std::nothrow_t`, declared `noexcept` |
| `result.has_value()`, `result.error()` | **NO** | `std::expected` observers are `noexcept` |
| `result.value()` (after `.has_value()` check) | **NO** | Throws `std::bad_expected_access` only if called without checking — programmer error, not a recoverable exception |
| `m_flag.load(std::memory_order_acquire)` | **NO** | `std::atomic::load` is `noexcept` |
| `return {}` / `return cpp_dbc::unexpected(DBException(...))` | **NO** | Value construction; failure is `std::terminate`, not a catchable exception |
| Guard macros that expand to `return cpp_dbc::unexpected(...)` or `(void)0` | **NO** | The expansion contains only noexcept operations |
| Simple member reads (`m_tr`, `m_closed`) | **NO** | Plain member access never throws |
| Debug macros (`DRIVER_DEBUG(...)`) | **NO** | Expand to no-op or simple `std::cout <<` which is noexcept |
| `std::make_shared<T>(...)`, `std::string`, `std::vector`, heap allocation | **NO** | Can throw `std::bad_alloc`, but that is a death sentence (see below) |
| `std::lock_guard<std::mutex>`, `std::recursive_mutex::lock()` | **NO** | Can throw `std::system_error`, but that is also a death sentence (see below) |

**Death-sentence exceptions** — `std::bad_alloc` and mutex `std::system_error` are in a special category: they signal that the process is in a state from which there is no meaningful recovery.

- **`std::bad_alloc`** (memory exhaustion): the heap is full. Any catch block that tries to construct a `DBException`, log a message, or allocate a recovery object will itself fail for the same reason. Catching it gives an illusion of safety while providing none. The OS will terminate the process moments later regardless.
- **`std::system_error` from a mutex**: the OS kernel reported a fatal threading error — the mutex is corrupted, the recursive lock count exceeded the platform limit, or the thread state is inconsistent. The system is in a critical state and there is no defined recovery path. `std::terminate` is the correct and honest response.

In both cases, catching the exception and returning `std::unexpected(DBException(...))` actively makes things worse: it hides a catastrophic failure as a silent, normal error return, potentially leaving the program running in an invalid state. Letting `std::terminate` fire is safer and more debuggable.

**Practical rule**: treat death-sentence exceptions the same as non-throwing expressions. Their presence does NOT justify a try/catch.

**Concrete example** — `returnToPool(std::nothrow_t)` in `FirebirdDBConnection`:

```cpp
// Incorrect — every inner call is nothrow; the three catch blocks are unreachable dead code
cpp_dbc::expected<void, DBException>
FirebirdDBConnection::returnToPool(std::nothrow_t) noexcept
{
    try
    {
        // FIREBIRD_CONNECTION_LOCK_OR_RETURN expands to:
        //   std::lock_guard<std::recursive_mutex> lock(*m_connMutex);  (noexcept — mutex lock failure
        //                                                                is a programming error → terminate)
        //   if (m_closed.load(...)) return cpp_dbc::unexpected(DBException(...));
        // → no throw possible
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("HFEEQDUN8QKD", "Connection closed");

        // prepareForPoolReturn(std::nothrow) — noexcept, returns std::expected → no throw
        auto prepResult = prepareForPoolReturn(std::nothrow);
        if (!prepResult.has_value())    // has_value() is noexcept → no throw
        {
            FIREBIRD_DEBUG("...");      // debug macro → no throw
        }

        // m_tr — plain member read → no throw
        // m_closed.load(...) — atomic load → no throw
        if (!m_tr && !m_closed.load(std::memory_order_acquire))
        {
            // startTransaction(std::nothrow) — noexcept, returns std::expected → no throw
            [[maybe_unused]] auto startResult = startTransaction(std::nothrow);
        }

        return {};  // noexcept value construction → no throw
    }
    catch (const DBException &ex)      // UNREACHABLE — DBException is never thrown above
    {
        return cpp_dbc::unexpected(ex);
    }
    catch (const std::exception &ex)   // UNREACHABLE — std::exception is never thrown above
    {
        return cpp_dbc::unexpected(DBException("21I433I3JGSM", ex.what(), ...));
    }
    catch (...)                        // UNREACHABLE
    {
        return cpp_dbc::unexpected(DBException("RX0KXBFWZ9AP", "...", ...));
    }
}

// Correct — try/catch removed; the noexcept contract is already enforced by the caller signatures
cpp_dbc::expected<void, DBException>
FirebirdDBConnection::returnToPool(std::nothrow_t) noexcept
{
    FIREBIRD_CONNECTION_LOCK_OR_RETURN("HFEEQDUN8QKD", "Connection closed");

    auto prepResult = prepareForPoolReturn(std::nothrow);
    if (!prepResult.has_value())
    {
        FIREBIRD_DEBUG("  prepareForPoolReturn failed: %s", prepResult.error().what_s().data());
    }

    if (!m_tr && !m_closed.load(std::memory_order_acquire))
    {
        [[maybe_unused]] auto startResult = startTransaction(std::nothrow);
        if (!startResult.has_value())
        {
            FIREBIRD_DEBUG("  Failed to start fresh transaction");
        }
    }

    return {};
}
```

The `FIREBIRD_CONNECTION_LOCK_OR_RETURN` macro uses `std::lock_guard<std::recursive_mutex>` internally. As explained in the death-sentence rule above, any exception from that lock would be `std::system_error` — unrecoverable, so the absence of try/catch is correct.

### `catch(...)` Requires a Preceding `catch(const std::exception &ex)`

A bare `catch(...)` block is only permitted when a `catch(const std::exception &ex)` block precedes it in the same try/catch chain. If none exists, one must be added. This ensures typed exceptions are handled and logged before the fallback:

```cpp
// Correct
try
{
    riskyOperation();
}
catch (const std::exception &ex)
{
    CP_DEBUG("Operation failed: " + std::string(ex.what()));
    // restore invariants...
}
catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
{
    CP_DEBUG("Operation failed: unknown exception");
    // restore invariants...
}

// Incorrect — catch(...) without a typed catch above it
try
{
    riskyOperation();
}
catch (...)
{
    // restore invariants...
}
```

### Catch Variable Naming — Always `ex`

The variable that captures the exception in a `catch` block must be named `ex`. Any other name is a violation. In nested try/catch blocks, use `ex1`, `ex2`, etc. to indicate nesting depth:

```cpp
// Correct
catch (const std::exception &ex)
{
    CP_DEBUG(ex.what());
}

// Correct — nested catches
try
{
    try
    {
        riskyOperation();
    }
    catch (const std::exception &ex1)
    {
        cleanup();  // may also throw
    }
}
catch (const std::exception &ex2)
{
    CP_DEBUG("Cleanup also failed: " + std::string(ex2.what()));
}

// Incorrect — non-standard variable names
catch (const std::exception &e)   { }   // too short
catch (const std::exception &err) { }   // not 'ex'
catch (const std::exception &exc) { }   // not 'ex'
```

## Class Layout — Static Factory Pattern and `-fno-exceptions` Compatibility

Classes whose construction can fail, or that must remain compilable under `-fno-exceptions`, follow the **static factory + dual API** pattern. The goals are:

- Move all fallible initialization out of the constructor and into the `create` factory or a private `initialize` helper.
- Expose a throwing `create` overload guarded by `#ifdef __cpp_exceptions` for conventional call sites.
- Expose a nothrow `create(std::nothrow_t, ...)` overload that always compiles, returning `std::expected`.
- Keep the class body free of any `throw` statement — all error paths return `std::unexpected`. The throwing API is a thin wrapper that calls the nothrow overload and rethrows.

### Header File Layout (`.hpp`)

The access specifier order is always **`private` → `protected` → `public`**. The `protected` section is optional and only present when the class is designed for inheritance. Within each section the order is: member variables first, then methods.

### `private:` Access Specifier — Omit in `class`, Require in `struct`

In C++, `class` defaults to `private` access and `struct` defaults to `public` access. Since the layout puts private members first:

- **`class`**: The `private:` specifier is **redundant and must be omitted**. Members at the top of a `class` body are already private by default.
- **`struct`**: The `private:` specifier is **required and must be explicit**. Without it, members at the top of a `struct` body would be public by default, which is incorrect.

```cpp
// Correct — class: no private: tag (redundant, members are private by default)
class MyClass
{
    // ── private ──────────────────────────────────────────────
    std::string m_host;
    int         m_port{0};

protected:
    // ...

public:
    // ...
};

// Correct — struct: private: tag required (default is public)
struct MyStruct
{
private:
    // ── private ──────────────────────────────────────────────
    std::string m_host;
    int         m_port{0};

protected:
    // ...

public:
    // ...
};

// Incorrect — class with redundant private: tag
class MyClass
{
private:  // redundant — class is already private by default
    std::string m_host;
};

// Incorrect — struct missing private: tag
struct MyStruct
{
    // ── private ──────────────────────────────────────────────
    std::string m_host;  // WRONG: this is public by default in a struct!
};
```

This applies only to new or modified class/struct definitions. Pre-existing definitions do not need to be updated retroactively.

```cpp
class MyClass
{
    // ── private ────────────────────────────────────────────────────────────────
    // 1. PrivateCtorTag (prevents external construction; see PrivateCtorTag Pattern)
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // 2. Member variables
    std::string      m_host;
    int              m_port{0};

    // 3. Construction state variables (for deferred error delivery)
    bool m_initFailed{false};
    std::unique_ptr<DBException> m_initError{nullptr};

    // 4. Private helper functions (noexcept, return std::expected)
    std::expected<void, DBException> helperFoo(std::nothrow_t) noexcept;

protected:
    // ── protected (optional — only when the class is designed for inheritance) ─
    // 5. Protected member variables (if any)
    int m_retryCount{3};

    // 6. Protected nothrow helper methods (noexcept, return std::expected)
    std::expected<void, DBException> retryInternal(std::nothrow_t) noexcept;

public:
    // ── public ─────────────────────────────────────────────────────────────────
    // 7. Public nothrow constructor (guarded by PrivateCtorTag)
    MyClass(PrivateCtorTag, std::nothrow_t, std::string host, int port) noexcept;

    // 8. Destructor (override when inheriting from an interface)
    ~MyClass() override;

    // 9. Deleted copy/move operators (when the class is non-copyable/non-movable)
    MyClass(const MyClass &)            = delete;
    MyClass &operator=(const MyClass &) = delete;

#ifdef __cpp_exceptions
    // 10. Throwing static factory (only compiled when exceptions are enabled)
    static std::shared_ptr<MyClass> create(std::string host, int port);

    // 11. Throwing public methods
    void   connect();
    Result query(const std::string &sql);
#endif

    // 12. Nothrow static factory (always compiled)
    static std::expected<std::shared_ptr<MyClass>, DBException>
    create(std::nothrow_t, std::string host, int port) noexcept;

    // 13. Nothrow public methods (always compiled)
    std::expected<void,   DBException> connect(std::nothrow_t) noexcept;
    std::expected<Result, DBException> query(std::nothrow_t, const std::string &sql) noexcept;
};
```

**Private constructors are forbidden.** All constructors must be **public** and guarded by `PrivateCtorTag`. For driver classes, `PrivateCtorTag` is defined as a `private` struct in the class itself. For pool classes, a single shared `PrivateCtorTag` is defined as a `protected` struct in the abstract base class (`DBConnectionPool`) and referenced by all derived classes — see [Shared PrivateCtorTag — Pool Classes](#shared-privatectortag--pool-classes) for the rationale. The tag name is always `PrivateCtorTag` — never `ConstructorTag` or any other name. This ensures compatibility with `std::make_shared` and `std::enable_shared_from_this`, and makes the construction path uniform across the entire codebase.

**Migration rule**: If an existing class has a private constructor, it **must** be migrated to the PrivateCtorTag pattern: add a `PrivateCtorTag` private struct, move the constructor to `public`, and add `PrivateCtorTag` as the first parameter. The constructor remains `noexcept` and takes `std::nothrow_t` as the second parameter. All call sites (typically only in `::create` factories) must be updated to pass `PrivateCtorTag{}` as the first argument.

The `protected` section follows the same internal pattern as `private`: **members first, then nothrow methods**. Non-override protected methods are internal implementation helpers and must be `noexcept`, returning `std::expected<T, DBException>` with `std::nothrow_t` as their first parameter, identical to private helpers. **Exception**: `protected override` methods that override a base class interface declaring both throwing and nothrow versions **must** implement both versions (the throwing override delegates to the nothrow override) — see [Private and Protected Methods](#private-and-protected-methods--return-stdexpectedt-dbexception-and-are-noexcept).

### Shared PrivateCtorTag — Pool Classes

Pool classes have a deep inheritance chain (`MySQLConnectionPool` → `RelationalDBConnectionPool` → `DBConnectionPoolBase` → `DBConnectionPool`). Unlike driver classes, pool classes **cannot** use a per-class `PrivateCtorTag` because each constructor must pass the tag to its parent constructor. If `MySQLConnectionPool` declared its own private `PrivateCtorTag`, it could not construct `RelationalDBConnectionPool` — it has no access to `RelationalDBConnectionPool::PrivateCtorTag` (it is private). Solving this with two constructors per level (one tagged for `create()`, one protected for derived classes) would double the constructor count at every level of the hierarchy.

The chosen design uses a **single shared `PrivateCtorTag`** defined once in the abstract interface (`DBConnectionPool`), in the **`protected`** section. All derived classes — base, family, and concrete — reference `DBConnectionPool::PrivateCtorTag` as their first constructor parameter and pass it up the chain:

```cpp
// The tag is defined ONCE in the abstract interface
class DBConnectionPool
{
protected:
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ... pure virtual methods ...
};
```

```cpp
// Every level of the hierarchy receives and forwards the same tag:
MySQLConnectionPool(DBConnectionPool::PrivateCtorTag tag, url, user, pass)
    → RelationalDBConnectionPool(tag, url, user, pass, ...)
        → DBConnectionPoolBase(tag, url, user, pass, ...)
```

**Why `protected` and not `private`**: The tag must be accessible to all derived classes so they can (a) accept it as a constructor parameter and (b) instantiate it in their `::create` factories. A `private` tag would require `friend` declarations in the base class for every derived class, which is fragile and defeats the open-for-extension principle. `protected` provides the correct accessibility — derived classes and their members can access it, but external code cannot.

**Key differences from the per-class PrivateCtorTag pattern**:
1. The tag comes from the **base class** (`DBConnectionPool::PrivateCtorTag`), not a private struct in each derived class
2. The tag is **`protected`** in the base class (not `private`) — so all derived classes can access it
3. The first parameter is `PrivateCtorTag` — **`std::nothrow_t` is not needed** as the second parameter because `PrivateCtorTag` already signals "do not call directly"
4. The constructor is still `noexcept` — no fallible work happens in it
5. There are no `m_initFailed` / `m_initError` construction state variables — fallible initialization is deferred to `initializePool(std::nothrow)` called by the factory after construction

**Header layout with shared PrivateCtorTag** — the access specifier order remains **`private` → `protected` → `public`**, but the public section gains a new item (PrivateCtorTag constructors) that comes **before** the destructor:

```cpp
class MyPool : public DBConnectionPool, public std::enable_shared_from_this<MyPool>
{
    // ── private ────────────────────────────────────────────────────────────────
    // 1. Member variables
    std::string m_url;
    int         m_maxSize{0};

    // 2. Private helper functions (noexcept, return std::expected)
    std::expected<std::shared_ptr<PooledConn>, DBException> createPooledDBConnection(std::nothrow_t) noexcept;
    std::expected<void, DBException> returnConnection(std::nothrow_t, std::shared_ptr<PooledConn> conn) noexcept;

protected:
    // ── protected (optional) ──────────────────────────────────────────────────
    // 3. Protected nothrow helper methods
    std::expected<void, DBException> initializePool(std::nothrow_t) noexcept;

public:
    // ── public ─────────────────────────────────────────────────────────────────
    // 4. PrivateCtorTag constructors (public, but uncallable without the tag)
    //    No std::nothrow_t needed — PrivateCtorTag already prevents misuse.
    //    Tag comes from DBConnectionPool (protected), shared across the hierarchy.
    MyPool(DBConnectionPool::PrivateCtorTag,
           const std::string &url, int maxSize) noexcept;
    explicit MyPool(DBConnectionPool::PrivateCtorTag,
                    const config::DBConnectionPoolConfig &config) noexcept;

    // 5. Destructor
    ~MyPool() override;

    // 6. Deleted copy/move operators
    MyPool(const MyPool &)            = delete;
    MyPool &operator=(const MyPool &) = delete;

#ifdef __cpp_exceptions
    // 7. Throwing static factory
    static std::shared_ptr<MyPool> create(const std::string &url, int maxSize);

    // 8. Throwing public methods
    std::shared_ptr<DBConnection> getDBConnection() override;
    void close() final;
#endif

    // 9. Nothrow static factory
    static std::expected<std::shared_ptr<MyPool>, DBException>
    create(std::nothrow_t, const std::string &url, int maxSize) noexcept;

    // 10. Nothrow public methods
    std::expected<std::shared_ptr<DBConnection>, DBException> getDBConnection(std::nothrow_t) noexcept override;
    std::expected<void, DBException> close(std::nothrow_t) noexcept override;
};
```

**Why PrivateCtorTag constructors come before the destructor**: they are the primary construction interface — the factory calls them directly. Placing them first makes the construction path immediately visible when reading the class.

**PrivateCtorTag source** — `PrivateCtorTag` is defined as a **protected** struct in the abstract base class (`DBConnectionPool::PrivateCtorTag`). The factory method creates an instance of the tag inline:

```cpp
// In the nothrow factory:
auto pool = std::make_shared<MyPool>(DBConnectionPool::PrivateCtorTag{}, url, maxSize);
auto initResult = pool->initializePool(std::nothrow);
if (!initResult.has_value())
{
    return std::unexpected(initResult.error());
}
return pool;
```

**Choosing between per-class and shared PrivateCtorTag**:

| Criterion | Per-class `PrivateCtorTag` | Shared `PrivateCtorTag` (pool classes) |
|-----------|----------------------------|------------------------------|
| Tag location | `private` struct in the class itself | `protected` struct in the abstract base class (`DBConnectionPool`) |
| Why this design | Each class is standalone, no inheritance chain for construction | Deep inheritance chain — each constructor must pass the tag to its parent |
| `std::nothrow_t` second param | Required | Not needed — `PrivateCtorTag` alone prevents misuse |
| Construction state | `m_initFailed` + `m_initError` in the class | `initializePool(std::nothrow)` called by factory |
| Used by | Driver classes (Connection, PreparedStatement, ResultSet, Blob, InputStream, Cursor, Collection, Document) | Pool classes (`*DBConnectionPool`, `*PooledDBConnection`) |

**Migration rule**: If existing pool classes use a tag named `ConstructorTag`, it **must** be renamed to `PrivateCtorTag`. The tag name must be uniform across the entire codebase — always `PrivateCtorTag`, never `ConstructorTag`.

### PrivateCtorTag Pattern — Driver Classes (Creational Pattern)

All driver classes — Connection, PreparedStatement, ResultSet, Blob, InputStream, Cursor, Collection, Document, and **`*DBDriver`** — use the **PrivateCtorTag creational pattern**. `*DBDriver` classes combine PrivateCtorTag with the [Singleton + Connection Registry](#dbdriver--singleton--connection-registry) pattern (`s_instance` weak_ptr + `getInstance()` + `s_connectionRegistry`).

The goal is a **throw-free creational pattern**: no `throw` statement exists anywhere except inside the explicit `::create` throwing wrappers. Constructors never throw — they capture errors in member variables for deferred delivery by the factory.

#### PrivateCtorTag

Each class defines its own `PrivateCtorTag` as a private nested struct. This prevents external code from constructing instances directly (the tag type is inaccessible outside the class), while keeping the constructor public so `std::make_shared` can call it:

```cpp
class MyDBConnection final : public RelationalDBConnection,
                              public std::enable_shared_from_this<MyDBConnection>
{
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ... rest of class ...
};
```

Every driver class declares exactly **one** `PrivateCtorTag` in its private section. The tag is never shared across classes — each class owns its own.

#### Construction State Variables

Every class declares two member variables that track whether the constructor succeeded or failed:

```cpp
    // Set to true by the constructor when initialization fails.
    // Inspected by create(nothrow_t) to propagate the error via expected.
    bool m_initFailed{false};

    // Only allocated on the failure path. Holds the DBException for deferred
    // delivery. nullptr when construction succeeds (~256 bytes saved per
    // successful instance vs storing a bare DBException inline).
    std::unique_ptr<DBException> m_initError{nullptr};
```

**Why `std::unique_ptr<DBException>`**: A `DBException` is ~256 bytes. Storing it inline wastes that space on every successful instance (the vast majority). With `std::unique_ptr`, successful instances pay only 8 bytes (the pointer). The `DBException` is heap-allocated only on the failure path. `std::make_unique` can throw `std::bad_alloc` inside the catch block, but if the system cannot allocate ~256 bytes, it is a death sentence regardless.

#### Public Nothrow Constructors

All constructors are **public** and **`noexcept`**. The first parameter is always `PrivateCtorTag`, the second is always `std::nothrow_t`, followed by any class-specific parameters:

```cpp
public:
    // PrivateCtorTag prevents external instantiation.
    // std::nothrow_t signals the constructor never throws.
    MyDBConnection(PrivateCtorTag,
                   std::nothrow_t,
                   const std::string &host,
                   int port,
                   const std::string &database,
                   const std::string &user,
                   const std::string &password,
                   const std::map<std::string, std::string> &options) noexcept;
```

The constructor wraps fallible initialization in try/catch **only when the body contains calls that can throw recoverable C++ exceptions** (e.g. C++ library APIs like the MySQL Connector/C++ or mongocxx that throw on connection failure). The try/catch captures errors in `m_initFailed` / `m_initError` instead of letting them escape. A comment above the `try` must list the specific recoverable exceptions being caught:

```cpp
MySQLDBConnection::MySQLDBConnection(PrivateCtorTag,
                                     std::nothrow_t,
                                     const std::string &host,
                                     int port,
                                     const std::string &database,
                                     const std::string &user,
                                     const std::string &password,
                                     const std::map<std::string, std::string> &options) noexcept
{
    // sql::mysql::get_mysql_driver_instance() and driver->connect() are C++ APIs
    // that throw sql::SQLException on authentication/network failure — recoverable.
    try
    {
        auto *driver = sql::mysql::get_mysql_driver_instance();
        m_conn.reset(driver->connect("tcp://" + host + ":" + std::to_string(port), user, password));
        if (!m_conn)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(
                "XXXXXXXXXXXX", "Failed to connect",
                system_utils::captureCallStack());
            return;
        }
        m_conn->setSchema(database);
        m_closed.store(false, std::memory_order_release);
    }
    catch (const std::exception &ex)
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(
            "XXXXXXXXXXXX", ex.what(),
            system_utils::captureCallStack());
    }
    catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(
            "XXXXXXXXXXXX", "Unknown error in constructor",
            system_utils::captureCallStack());
    }
}
```

**When the constructor body contains only nothrow operations** (atomic stores, member assignments, calls to nothrow methods) and/or operations that can only throw death-sentence exceptions (`std::bad_alloc`, mutex `std::system_error`), the try/catch **must be omitted** — it would be dead code per the [No Redundant try/catch](#no-redundant-trycatch-in-nothrow-methods) rule:

```cpp
// Correct — no try/catch: constructor body is entirely nothrow
// (std::make_shared in the factory may throw bad_alloc, but that is a
// death sentence handled outside the constructor)
MyDBPreparedStatement::MyDBPreparedStatement(PrivateCtorTag,
                                             std::nothrow_t,
                                             std::weak_ptr<MyDBConnection> conn,
                                             const std::string &sql) noexcept
    : m_connection(std::move(conn)), m_sql(sql)
{
    m_closed.store(false, std::memory_order_release);
}

// Incorrect — try/catch is dead code; no recoverable exceptions possible
MyDBPreparedStatement::MyDBPreparedStatement(PrivateCtorTag,
                                             std::nothrow_t,
                                             std::weak_ptr<MyDBConnection> conn,
                                             const std::string &sql) noexcept
    : m_connection(std::move(conn)), m_sql(sql)
{
    try
    {
        m_closed.store(false, std::memory_order_release);
    }
    catch (const std::exception &ex)  // UNREACHABLE — no recoverable throw above
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(...);
    }
}
```

**Rule of thumb**: Before writing a try/catch in a PrivateCtorTag constructor, walk every statement in the body using the [Diagnostic Checklist](#diagnostic-checklist--is-the-trycatch-dead-code). If no statement can throw a recoverable exception, omit the try/catch entirely.

If a class has multiple constructors (e.g. different parameter sets), each one follows this same pattern — public, `noexcept`, `PrivateCtorTag` first, `std::nothrow_t` second, errors captured in `m_initFailed` / `m_initError` only when recoverable exceptions are possible.

#### Static Factory `::create`

The `::create` static factory is the **only** way to instantiate a class. There is one `::create` per constructor. Each `::create` has **two versions**:

**Throwing version** — a thin wrapper that delegates entirely to the nothrow version. Must be inside `#ifdef __cpp_exceptions` to avoid compilation under `-fno-exceptions`:

```cpp
#ifdef __cpp_exceptions
    static std::shared_ptr<MyDBConnection>
    create(const std::string &host, int port, const std::string &database,
           const std::string &user, const std::string &password,
           const std::map<std::string, std::string> &options = {})
    {
        auto r = create(std::nothrow, host, port, database, user, password, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }
#endif
```

**Nothrow version** — contains the real creation logic. First parameter is always `std::nothrow_t`. Uses `std::make_shared` and checks `m_initFailed` after construction:

```cpp
    static cpp_dbc::expected<std::shared_ptr<MyDBConnection>, DBException>
    create(std::nothrow_t,
           const std::string &host, int port, const std::string &database,
           const std::string &user, const std::string &password,
           const std::map<std::string, std::string> &options = {}) noexcept
    {
        // std::make_shared may throw std::bad_alloc. No try/catch here:
        // if the system cannot allocate memory for the object, it is a
        // death sentence — no meaningful recovery is possible.
        auto obj = std::make_shared<MyDBConnection>(
            PrivateCtorTag{}, std::nothrow, host, port, database, user, password, options);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }
```

#### Complete Header Example

```cpp
class MyDBPreparedStatement final : public RelationalDBPreparedStatement
{
    // ── PrivateCtorTag ────────────────────────────────────────────────────
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ── Member variables ──────────────────────────────────────────────────
    std::weak_ptr<MyDBConnection> m_connection;
    std::string m_sql;
    std::atomic<bool> m_closed{true};
    // ...

    // ── Construction state ────────────────────────────────────────────────
    bool m_initFailed{false};
    std::unique_ptr<DBException> m_initError{nullptr};

    // ── Private helpers ───────────────────────────────────────────────────
    cpp_dbc::expected<void, DBException> prepareInternal(std::nothrow_t) noexcept;

public:
    // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
    MyDBPreparedStatement(PrivateCtorTag,
                          std::nothrow_t,
                          std::weak_ptr<MyDBConnection> conn,
                          const std::string &sql) noexcept;

    ~MyDBPreparedStatement() override;

    MyDBPreparedStatement(const MyDBPreparedStatement &) = delete;
    MyDBPreparedStatement &operator=(const MyDBPreparedStatement &) = delete;

#ifdef __cpp_exceptions
    // ── Throwing factory (delegates to nothrow) ──────────────────────────
    static std::shared_ptr<MyDBPreparedStatement>
    create(std::weak_ptr<MyDBConnection> conn, const std::string &sql)
    {
        auto r = create(std::nothrow, std::move(conn), sql);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }
#endif

    // ── Nothrow factory (real creation logic) ────────────────────────────
    static cpp_dbc::expected<std::shared_ptr<MyDBPreparedStatement>, DBException>
    create(std::nothrow_t,
           std::weak_ptr<MyDBConnection> conn,
           const std::string &sql) noexcept
    {
        // std::make_shared may throw std::bad_alloc. No try/catch here:
        // if the system cannot allocate memory for the object, it is a
        // death sentence — no meaningful recovery is possible.
        auto obj = std::make_shared<MyDBPreparedStatement>(
            PrivateCtorTag{}, std::nothrow, std::move(conn), sql);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    // ── Nothrow public methods ───────────────────────────────────────────
    cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept;
    // ...
};
```

#### Summary

| Rule | Description |
|------|-------------|
| **Scope** | All driver classes including `*DBDriver` |
| **PrivateCtorTag** | One per class, private nested struct, prevents external construction |
| **Constructor** | Public, `noexcept`, params: `PrivateCtorTag` → `std::nothrow_t` → class-specific |
| **Construction state** | `bool m_initFailed{false}` + `std::unique_ptr<DBException> m_initError{nullptr}` |
| **Factory** | `::create` for most classes; `::getInstance` for `*DBDriver` (singleton). One per constructor |
| **Factory throwing** | Inside `#ifdef __cpp_exceptions`, delegates to nothrow version |
| **Factory nothrow** | First param `std::nothrow_t`, uses `std::make_shared`, no try/catch |
| **`bad_alloc`** | Death sentence — no try/catch in factory nothrow |
| **Throw-free guarantee** | No `throw` anywhere except inside factory throwing wrappers |

### Source File Distribution

Implementations are split across multiple `.cpp` files to keep compilation units focused and to cleanly separate exception-dependent code from exception-free code. The distribution follows this fixed pattern:

**`myclass_01.cpp`** — Public nothrow constructor (PrivateCtorTag) + private helpers + destructor + first `#ifdef __cpp_exceptions` group:

```cpp
// ── Public nothrow constructor (PrivateCtorTag) ───────────────────────────────
MyClass::MyClass(PrivateCtorTag, std::nothrow_t, std::string host, int port) noexcept
    : m_host(std::move(host)), m_port(port)
{
}

// ── Private helpers ───────────────────────────────────────────────────────────
std::expected<void, DBException> MyClass::initialize(std::nothrow_t) noexcept
{
    // ...
}

std::expected<void, DBException> MyClass::helperFoo(std::nothrow_t) noexcept
{
    // ...
}

// ── Destructor ────────────────────────────────────────────────────────────────
MyClass::~MyClass()
{
    // ...
}

// ── Throwing API (first group) ────────────────────────────────────────────────
#ifdef __cpp_exceptions

std::shared_ptr<MyClass> MyClass::create(std::string host, int port)
{
    auto result = MyClass::create(std::nothrow, std::move(host), port);
    if (!result.has_value())
    {
        throw result.error();
    }
    return result.value();
}

void MyClass::connect()
{
    auto result = connect(std::nothrow);
    if (!result.has_value())
    {
        throw result.error();
    }
}

#endif
```

**`myclass_02.cpp`** — Remaining throwing methods + nothrow factory + first nothrow group:

```cpp
// ── Throwing API (continuation) ───────────────────────────────────────────────
#ifdef __cpp_exceptions

Result MyClass::query(const std::string &sql)
{
    auto result = query(std::nothrow, sql);
    if (!result.has_value())
    {
        throw result.error();
    }
    return result.value();
}

#endif

// ── Nothrow static factory ────────────────────────────────────────────────────
std::expected<std::shared_ptr<MyClass>, DBException>
MyClass::create(std::nothrow_t, std::string host, int port) noexcept
{
    auto obj = std::make_shared<MyClass>(PrivateCtorTag{}, std::nothrow, std::move(host), port);
    if (obj->m_initFailed)
    {
        return cpp_dbc::unexpected(std::move(*obj->m_initError));
    }
    return obj;
}

// ── Nothrow public methods (first group) ─────────────────────────────────────
std::expected<void, DBException> MyClass::connect(std::nothrow_t) noexcept
{
    // ...
}
```

**`myclass_03.cpp`** — Remaining nothrow methods:

```cpp
// ── Nothrow public methods (continuation) ────────────────────────────────────
std::expected<Result, DBException> MyClass::query(std::nothrow_t, const std::string &sql) noexcept
{
    // ...
}
```

### Rules and Constraints

1. **No `throw` in the class body**: The class itself never uses `throw` directly. All error paths return `std::unexpected`. The throwing API is a thin wrapper that delegates to the nothrow overload and rethrows on error:
   ```cpp
   void MyClass::connect()          // throwing wrapper — no logic of its own
   {
       auto result = connect(std::nothrow);
       if (!result.has_value())
       {
           throw result.error();
       }
   }
   ```

2. **`#ifdef __cpp_exceptions` scope**: Every throwing declaration in the header and every throwing definition in `.cpp` files must sit inside `#ifdef __cpp_exceptions` / `#endif`. This applies to the throwing `create` and to every throwing public method.

3. **Ordering within a `.cpp` file**: When a `.cpp` file contains both throwing and nothrow methods, the `#ifdef __cpp_exceptions` block always comes first, followed by the nothrow implementations. This mirrors the declaration order in the header.

4. **Nothrow factory uses the public PrivateCtorTag constructor**: `create(std::nothrow_t, ...)` allocates the object via `std::make_shared` using the `PrivateCtorTag`-guarded public constructor. For driver classes, fallible initialization happens inside the constructor itself (errors captured in `m_initFailed` / `m_initError`). For pool classes, trivially safe initialization happens in the constructor, and fallible setup is deferred to `initializePool(std::nothrow)` called by the factory after construction.

5. **Sections absent when there is nothing to put in them**: If the class has no throwing methods at all, the `#ifdef __cpp_exceptions` block is omitted entirely from every file. If all methods fit in two `.cpp` files, a third file is not created.

6. **Split-point heuristic**: Each `.cpp` file should contain a logically coherent group. Prefer grouping by (a) lifecycle (constructor / helpers / destructor), (b) data-path operations, (c) administrative or introspection methods. Avoid splitting a single conceptual operation across two files.

### DBDriver — Singleton + Connection Registry

`*DBDriver` classes use the same **PrivateCtorTag creational pattern** as all other driver classes (Connection, PreparedStatement, etc.), with two additional concerns layered on top:

1. **Singleton**: A `static std::weak_ptr<MyDriver> s_instance` + `static std::mutex s_instanceMutex` pair ensures only one driver instance exists at a time. `getInstance()` locks `s_instanceMutex`, tries to promote the weak_ptr, and creates a new instance via `std::make_shared` with `PrivateCtorTag{}` if the weak_ptr is expired.

2. **Connection registry**: A `static std::set<std::weak_ptr<MyConnection>, std::owner_less<...>> s_connectionRegistry` + `static std::mutex s_registryMutex` pair tracks all live connections created through `connectRelational()` (or the family-specific connect method). Connections are registered after creation and unregistered when closed.

**Pattern**:

```cpp
// ── Header (driver.hpp, inside #if USE_<DRIVER>) ──────────────────────────────
class MyDriver final : public SomeFamilyDBDriver
{
    // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ── Singleton state ───────────────────────────────────────────────────
    static std::weak_ptr<MyDriver> s_instance;
    static std::mutex              s_instanceMutex;

    // ── Connection registry ───────────────────────────────────────────────
    static std::mutex                                                s_registryMutex;
    static std::set<std::weak_ptr<MyConnection>,
                    std::owner_less<std::weak_ptr<MyConnection>>>   s_connectionRegistry;

    static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

    static void registerConnection(std::nothrow_t, std::weak_ptr<MyConnection> conn) noexcept;
    static void unregisterConnection(std::nothrow_t, const std::weak_ptr<MyConnection> &conn) noexcept;

    static void cleanup();

    friend class MyConnection;

    // ── Construction state ────────────────────────────────────────────────
    bool m_initFailed{false};
    std::unique_ptr<DBException> m_initError{nullptr};

public:
    MyDriver(PrivateCtorTag, std::nothrow_t) noexcept;
    ~MyDriver() override;

    MyDriver(const MyDriver &) = delete;
    MyDriver &operator=(const MyDriver &) = delete;
    MyDriver(MyDriver &&) = delete;
    MyDriver &operator=(MyDriver &&) = delete;

#ifdef __cpp_exceptions
    static std::shared_ptr<MyDriver> getInstance();
    // ... throwing public methods ...
#endif

    static cpp_dbc::expected<std::shared_ptr<MyDriver>, DBException> getInstance(std::nothrow_t) noexcept;
    // ... nothrow public methods ...

    static size_t getConnectionAlive() noexcept;
};
```

```cpp
// ── Implementation (driver_01.cpp) ────────────────────────────────────────────

// ── Static member initialization ──────────────────────────────────────────
std::weak_ptr<MyDriver> MyDriver::s_instance;
std::mutex              MyDriver::s_instanceMutex;
std::mutex              MyDriver::s_registryMutex;
std::set<std::weak_ptr<MyConnection>,
         std::owner_less<std::weak_ptr<MyConnection>>> MyDriver::s_connectionRegistry;

// ── Private static helpers ────────────────────────────────────────────────
cpp_dbc::expected<bool, DBException> MyDriver::initialize(std::nothrow_t) noexcept
{
    // Call C library init if needed; return error on failure.
    // For libraries that don't need global init (e.g. libpq), this is a no-op.
    return true;
}

void MyDriver::registerConnection(std::nothrow_t, std::weak_ptr<MyConnection> conn) noexcept
{
    std::scoped_lock lock(s_registryMutex);
    s_connectionRegistry.insert(std::move(conn));
}

void MyDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<MyConnection> &conn) noexcept
{
    std::scoped_lock lock(s_registryMutex);
    s_connectionRegistry.erase(conn);
}

void MyDriver::cleanup()
{
    // Call C library cleanup if needed.
}

// ── Constructor + Destructor ──────────────────────────────────────────────
MyDriver::MyDriver(MyDriver::PrivateCtorTag, std::nothrow_t) noexcept
{
    auto result = initialize(std::nothrow);
    if (!result.has_value())
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(std::move(result.error()));
    }
}

MyDriver::~MyDriver()
{
    cleanup();
}

// ── Singleton ─────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions
std::shared_ptr<MyDriver> MyDriver::getInstance()
{
    auto result = getInstance(std::nothrow);
    if (!result.has_value())
    {
        throw result.error();
    }
    return result.value();
}
#endif

cpp_dbc::expected<std::shared_ptr<MyDriver>, DBException>
MyDriver::getInstance(std::nothrow_t) noexcept
{
    std::scoped_lock lock(s_instanceMutex);
    auto existing = s_instance.lock();
    if (existing)
    {
        return existing;
    }
    // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
    // Constructor errors are captured in m_initFailed / m_initError.
    auto inst = std::make_shared<MyDriver>(MyDriver::PrivateCtorTag{}, std::nothrow);
    if (inst->m_initFailed)
    {
        return cpp_dbc::unexpected(std::move(*inst->m_initError));
    }
    s_instance = inst;
    return inst;
}

size_t MyDriver::getConnectionAlive() noexcept
{
    std::scoped_lock lock(s_registryMutex);
    return static_cast<size_t>(std::ranges::count_if(
        s_connectionRegistry,
        [](const auto &w) { return !w.expired(); }));
}
```

**Key constraints**:
- The constructor is **public**, **`noexcept`**, and guarded by `PrivateCtorTag` — identical to all other driver classes. It **never throws**.
- Errors from `initialize()` are captured in `m_initFailed` / `m_initError` for deferred delivery by `getInstance()`.
- `getInstance(std::nothrow_t)` checks `m_initFailed` after construction — **no try/catch** (death-sentence exceptions only).
- `cleanup()` is called unconditionally in the destructor. It performs any C library cleanup needed.
- The `initialize()` helper is **always private**, takes `std::nothrow_t`, and is `noexcept`.

## Adding New Database Drivers

When adding support for a new database driver, follow the comprehensive guide at:

**`libs/cpp_dbc/docs/how_add_new_db_drivers.md`**

### Class and File Naming Conventions

#### Namespace

Each driver lives in its own nested namespace under `cpp_dbc`, using the brand's official casing:

```cpp
namespace cpp_dbc::MySQL      { }   // relational
namespace cpp_dbc::PostgreSQL { }   // relational
namespace cpp_dbc::SQLite     { }   // relational
namespace cpp_dbc::Firebird   { }   // relational
namespace cpp_dbc::Redis      { }   // kv
namespace cpp_dbc::MongoDB    { }   // document
namespace cpp_dbc::ScyllaDB   { }   // columnar
```

#### Class Names by Family

Class names are formed as `<BrandName><RoleSuffix>`. The role suffix follows the abstract base class name with the family prefix stripped:

| Role | Relational (`<Name>` = MySQL, PostgreSQL, SQLite, Firebird) | KV (Redis) | Document (MongoDB) | Columnar (ScyllaDB) |
|------|--------------------------------------------------------------|------------|---------------------|----------------------|
| Driver | `<Name>DBDriver` | `RedisDBDriver` | `MongoDBDriver` | `ScyllaDBDriver` |
| Connection | `<Name>DBConnection` | `RedisDBConnection` | `MongoDBConnection` | `ScyllaDBConnection` |
| PreparedStatement | `<Name>DBPreparedStatement` | — | — | `ScyllaDBPreparedStatement` |
| ResultSet | `<Name>DBResultSet` | — | — | `ScyllaDBResultSet` |
| Blob | `<Name>Blob` | — | — | — |
| InputStream | `<Name>InputStream` | — | — | — |
| Cursor | — | — | `MongoDBCursor` | — |
| Collection | — | — | `MongoDBCollection` | — |
| Document data | — | — | `MongoDBDocument` | — |

**Rule**: When the brand name already contains "DB" (MongoDB, ScyllaDB), the role suffix does **not** add another "DB" — the brand name itself provides it (`MongoDB` + `Driver` = `MongoDBDriver`). When the brand name does not contain "DB" (MySQL, PostgreSQL, SQLite, Firebird, Redis), "DB" is inserted between the brand name and the role (`MySQL` + `DBDriver` = `MySQLDBDriver`, `Redis` + `DBDriver` = `RedisDBDriver`).

#### Header File Names

All header files use lowercase names inside the driver subdirectory:

| File | Contents |
|------|----------|
| `handles.hpp` | RAII handles for native C library resources |
| `driver.hpp` | `<Name>Driver` class (real + stub implementations) |
| `connection.hpp` | `<Name>Connection` class |
| `prepared_statement.hpp` | `<Name>PreparedStatement` class (relational, columnar) |
| `result_set.hpp` | `<Name>ResultSet` class (relational, columnar) |
| `blob.hpp` | `<Name>Blob` class (relational, when BLOB support is needed) |
| `input_stream.hpp` | `<Name>InputStream` class (relational, when streaming BLOBs) |
| `cursor.hpp` | `<Name>Cursor` class (document) |
| `collection.hpp` | `<Name>Collection` class (document) |
| `document.hpp` | `<Name>Document` class (document) |

#### Umbrella Header

Each driver family directory also has an **umbrella header** that includes all driver components. Its name is `driver_<lowercase_name>.hpp` and it lives one level above the driver subdirectory:

```
drivers/relational/driver_mysql.hpp       → includes mysql/
drivers/document/driver_mongodb.hpp       → includes mongodb/
drivers/kv/driver_redis.hpp               → includes redis/
drivers/columnar/driver_scylladb.hpp      → includes scylladb/
```

The umbrella header uses an `#ifndef` include guard (not `#pragma once`) because it conditionally includes the full or stub implementation via `#if USE_<DRIVER>` / `#else`.

### Guide Phases

This guide covers all 5 phases of driver implementation:
1. **Phase 1**: Creating driver files (.hpp/.cpp) and updating `cpp_dbc.hpp`
2. **Phase 2**: Updating CMake, shell scripts, distros folder, and config files
3. **Phase 3**: Creating tests (with validation checkpoint)
4. **Phase 4**: Creating benchmarks (with validation checkpoint)
5. **Phase 5**: Creating examples by driver family

Key files that require `USE_<DRIVER>` updates:
- `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp` - Main header with conditional includes
- `libs/cpp_dbc/cmake/cpp_dbc-config.cmake` - Package config for installed library
- `libs/cpp_dbc/cmake/FindCPPDBC.cmake` - Fallback driver discovery
- `libs/cpp_dbc/benchmark/benchmark_common.hpp` - Benchmark conditional includes
- `libs/cpp_dbc/CMakeLists.txt` - Main CMake configuration

Always use `helper.sh` for build validation between phases.

## Deep Reasoning Principle

While following user instructions is important, this is a serious programming project. Therefore:

- **Always justify**: If something is technically a bad idea, you must clearly explain why with solid arguments.
- **Proactively expose problems**: If you detect something that could be insecure, counterproductive, inefficient, or poorly designed, you must expose it clearly and directly, regardless of whether the user requested it that way.
- **Quality over speed**: It doesn't matter if you consume more tokens or take longer to respond. It must always be a deeply reasoned effort.
- **Technical honesty**: Prioritize technical accuracy and truth over validating the user's beliefs. If there's a better way to do something, propose it.
- **Critical thinking**: Question assumptions and design decisions when appropriate. A good engineer doesn't just execute, they also analyze and improve.


---

## ⛔ STRICTLY AND ABSOLUTELY FORBIDDEN — NO EXCEPTIONS

> **These rules are MANDATORY and apply to ALL situations, ALL contexts, and ALL circumstances.**
> **No exception exists. Ignorance or urgency is NOT a valid justification for violating them.**

### 1. `git checkout HEAD -- <files>` — PERMANENTLY BANNED

The command `git checkout HEAD -- *.*` (or any variant of it, including specifying individual files) is **completely and permanently forbidden**. This command silently discards ALL uncommitted changes in the working tree for the matched files, with NO undo and NO confirmation prompt.

**Why this is catastrophic in this project:**

- The working tree routinely contains large amounts of staged and unstaged in-progress work (as visible in the git status above: 100+ modified files)
- Running `git checkout HEAD -- *.*` would **irreversibly destroy all of that work in a single command**
- There is **no recycle bin, no undo, no recovery** — the changes are gone permanently
- Git does not ask for confirmation before overwriting

**Forbidden forms (partial list — ALL variants are banned):**

```bash
# FORBIDDEN — all of these are banned
git checkout HEAD -- *.*
git checkout HEAD -- *.cpp
git checkout HEAD -- *.hpp
git checkout HEAD -- some_file.cpp
git checkout HEAD -- libs/
git checkout HEAD -- .
```

**If you ever feel the need to discard changes, STOP and ask the user first.** Describe exactly which files you intend to revert and why. The user must explicitly approve each file before any discard operation.

**Safe alternatives (only with explicit user approval per file):**

```bash
# Show what would be affected — ALWAYS do this first
git diff HEAD -- path/to/specific_file.cpp

# Only after user explicitly approves discarding that specific file
git restore path/to/specific_file.cpp   # preferred modern form
```