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

### Bug-Fix Comments — Always Required

Every code block that exists to fix a bug or patch a problem **must** include a comment explaining:
1. Why the code is necessary (the root cause or symptom being addressed)
2. What it patches or works around

The comment length should be proportional to the complexity of the fix:

```cpp
// Correct — simple fix with concise comment
// Restore active state before handoff: setActive(false) was called unconditionally
// above, so it must be undone here when a waiter takes the connection directly.
conn->setActive(true);
m_activeConnections++;

// Correct — complex workaround with detailed comment
// WORKAROUND: Helgrind cannot track POSIX file locks used by SQLite internally.
// Without this global mutex, ThreadSanitizer reports false-positive data races
// because it sees concurrent memory access without any visible synchronization.
// This mutex makes synchronization visible to sanitizers at the cost of ~33%
// throughput overhead, so it is compiled out in production via ENABLE_FILE_MUTEX_REGISTRY=OFF.
std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

// Incorrect — no explanation, intent is opaque
conn->setActive(true);
m_activeConnections++;
```

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

## Memory Safety

- Use RAII handles for external resources (BsonHandle, RedisReplyHandle, etc.)
- Defensively check for nulls before dereferencing pointers from external libraries
- Use `std::atomic` with explicit memory ordering (`memory_order_acquire`/`release`)
- Avoid raw pointers. Use `std::shared_ptr`, `std::unique_ptr`, and `std::weak_ptr` where possible, especially in member/class variables.
- Always try to use C++17-style constructs and functions for more secure code.
- Use ranges and avoid index loops where possible.

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
    PooledConnection() { /* nothing to initialize */ }
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

## Thread Safety

- Use `DB_DRIVER_LOCK_GUARD(m_mutex)` macro for conditional locking
- The `DB_DRIVER_THREAD_SAFE` flag controls whether mutexes are active
- Prefer `std::scoped_lock` over other lock guard types (`lock_guard`, `unique_lock`) wherever it makes sense, as it avoids deadlocks and is safer by default
- Prefer `std::recursive_mutex` over other mutex types (`mutex`, `timed_mutex`) wherever it makes sense, to allow re-entrant locking within the same thread without deadlock

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

This eliminates hidden control flow, makes error propagation explicit, and prevents exceptions from escaping internal implementation boundaries.

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
    catch (...)
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

## Adding New Database Drivers

When adding support for a new database driver, follow the comprehensive guide at:

**`libs/cpp_dbc/docs/how_add_new_db_drivers.md`**

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