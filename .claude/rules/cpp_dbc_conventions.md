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

#### Diagnostic Checklist — Is the try/catch Dead Code?

Walk every statement inside the `try` block and ask: *"Can this expression throw a **recoverable** exception?"* The key word is **recoverable**. If **every** answer is NO, the entire try/catch is dead code and must be removed.

| Expression pattern | Recoverable throw? | Reason |
|--------------------|--------------------|--------|
| `someMethod(std::nothrow)` | **NO** | Takes `std::nothrow_t`, declared `noexcept` |
| `result.has_value()`, `result.value()`, `result.error()` | **NO** | `std::expected` accessors are `noexcept` |
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

```cpp
class MyClass
{
    // ── private ────────────────────────────────────────────────────────────────
    // 1. Member variables
    std::string      m_host;
    int              m_port{0};

    // 2. Private nothrow constructor (std::nothrow_t makes the intent explicit)
    explicit MyClass(std::nothrow_t, std::string host, int port) noexcept;

    // 3. Private helper functions (noexcept, return std::expected)
    //    initialize() is optional — only present when construction requires fallible steps
    std::expected<void, DBException> initialize(std::nothrow_t) noexcept;
    std::expected<void, DBException> helperFoo(std::nothrow_t) noexcept;

protected:
    // ── protected (optional — only when the class is designed for inheritance) ─
    // 4. Protected member variables (if any)
    int m_retryCount{3};

    // 5. Protected nothrow helper methods (noexcept, return std::expected)
    std::expected<void, DBException> retryInternal(std::nothrow_t) noexcept;

public:
    // ── public ─────────────────────────────────────────────────────────────────
    // 6. Destructor (override when inheriting from an interface)
    ~MyClass() override;

    // 7. Deleted copy/move operators (when the class is non-copyable/non-movable)
    MyClass(const MyClass &)            = delete;
    MyClass &operator=(const MyClass &) = delete;

#ifdef __cpp_exceptions
    // 8. Throwing static factory (only compiled when exceptions are enabled)
    static std::shared_ptr<MyClass> create(std::string host, int port);

    // 9. Throwing public methods
    void   connect();
    Result query(const std::string &sql);
#endif

    // 10. Nothrow static factory (always compiled)
    static std::expected<std::shared_ptr<MyClass>, DBException>
    create(std::nothrow_t, std::string host, int port) noexcept;

    // 11. Nothrow public methods (always compiled)
    std::expected<void,   DBException> connect(std::nothrow_t) noexcept;
    std::expected<Result, DBException> query(std::nothrow_t, const std::string &sql) noexcept;
};
```

The `protected` section follows the same internal pattern as `private`: **members first, then nothrow methods**. There is no throwing API in `protected` — protected methods are internal implementation helpers and must be `noexcept`, returning `std::expected<T, DBException>` with `std::nothrow_t` as their first parameter, identical to private helpers.

### ConstructorTag Variant — `std::make_shared` + `enable_shared_from_this`

When a class uses `std::enable_shared_from_this`, the constructor **must** be accessible to `std::make_shared`. A private constructor prevents this. The **ConstructorTag** pattern solves the problem: the constructor is public, but requires a tag type that only the class (or its factory) can create — preventing accidental direct construction.

**When to use this variant instead of the private `std::nothrow_t` constructor**:
- The class inherits from `std::enable_shared_from_this`
- The nothrow factory uses `std::make_shared<MyClass>(...)` (which requires a public or friend-accessible constructor)

**Key differences from the standard pattern**:
1. The constructor is **public**, not private
2. The first parameter is `ConstructorTag` (a private tag type from the base class or the class itself), **not** `std::nothrow_t` — `ConstructorTag` already signals "do not call directly"
3. The constructor is still `noexcept` — no fallible work happens in it

**Header layout with ConstructorTag** — the access specifier order remains **`private` → `protected` → `public`**, but the public section gains a new item (ConstructorTag constructors) that comes **before** the destructor:

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
    // 4. ConstructorTag constructors (public, but uncallable without the tag)
    //    No std::nothrow_t needed — ConstructorTag already prevents misuse.
    MyPool(DBConnectionPool::ConstructorTag,
           const std::string &url, int maxSize) noexcept;
    explicit MyPool(DBConnectionPool::ConstructorTag,
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

**Why ConstructorTag constructors come before the destructor**: they are the primary construction interface — the factory calls them directly. Placing them first makes the construction path immediately visible when reading the class.

**ConstructorTag source** — `ConstructorTag` is typically defined as a private struct in the base class (e.g. `DBConnectionPool::ConstructorTag`). The factory method creates an instance of the tag inline:

```cpp
// In the nothrow factory:
auto pool = std::make_shared<MyPool>(DBConnectionPool::ConstructorTag{}, url, maxSize);
auto initResult = pool->initializePool(std::nothrow);
if (!initResult.has_value())
{
    return std::unexpected(initResult.error());
}
return pool;
```

**Choosing between patterns**:

| Criterion | Private `std::nothrow_t` constructor | Public `ConstructorTag` constructor |
|-----------|--------------------------------------|-------------------------------------|
| `enable_shared_from_this` | Not compatible with `make_shared` | Compatible — constructor is public |
| Protection against misuse | Private access | Tag type is private in base class |
| `std::nothrow_t` first param | Required | Not needed — `ConstructorTag` serves the same role |
| Fallible initialization | `initialize(std::nothrow)` called by factory | `initializePool(std::nothrow)` called by factory |

**Classes that use this variant**: all connection pool classes (`*DBConnectionPool`) and their pooled connection wrappers (`*PooledDBConnection`).

### Source File Distribution

Implementations are split across multiple `.cpp` files to keep compilation units focused and to cleanly separate exception-dependent code from exception-free code. The distribution follows this fixed pattern:

**`myclass_01.cpp`** — Private constructor + private helpers + destructor + first `#ifdef __cpp_exceptions` group:

```cpp
// ── Private nothrow constructor ───────────────────────────────────────────────
MyClass::MyClass(std::nothrow_t, std::string host, int port) noexcept
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
    auto obj  = std::make_shared<MyClass>(std::nothrow, std::move(host), port);
    auto init = obj->initialize(std::nothrow);
    if (!init.has_value())
    {
        return std::unexpected(init.error());
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

4. **Nothrow factory uses the private nothrow constructor**: `create(std::nothrow_t, ...)` allocates the object through the private constructor — which performs only trivially safe initialization — and then calls `initialize(std::nothrow)` for all fallible setup. This keeps the constructor itself safe under all conditions.

5. **Sections absent when there is nothing to put in them**: If the class has no throwing methods at all, the `#ifdef __cpp_exceptions` block is omitted entirely from every file. If all methods fit in two `.cpp` files, a third file is not created.

6. **Split-point heuristic**: Each `.cpp` file should contain a logically coherent group. Prefer grouping by (a) lifecycle (constructor / helpers / destructor), (b) data-path operations, (c) administrative or introspection methods. Avoid splitting a single conceptual operation across two files.

### DBDriver Variant — C Library Initialization with Double-Checked Locking

`DBDriver` subclasses (e.g. `MongoDBDriver`, `RedisDBDriver`, `ScyllaDBDriver`) that wrap a C library requiring one-time global initialization use a specific `initialize` pattern. This variant is **only used in `DBDriver` classes** and is **different from the general `initialize` helper** described above.

**Why not `std::once_flag` / `std::call_once`?**
1. `std::once_flag` cannot be reset — but `cleanup()` must allow re-initialization after driver destruction.
2. `std::call_once` may throw `std::system_error`, which is incompatible with `-fno-exceptions` builds.

**Pattern**:

```cpp
// ── Header (driver.hpp, inside #if USE_<DRIVER>) ──────────────────────────────
class MyDriver final : public SomeFamilyDBDriver
{
    // Note: atomic<bool> + mutex instead of std::once_flag because
    // std::once_flag cannot be reset, but we need cleanup() to allow
    // re-initialization on subsequent driver construction.
    // Also, std::call_once can throw std::system_error, which is incompatible
    // with -fno-exceptions builds.
    static std::atomic<bool> s_initialized;
    static std::mutex        s_initMutex;

    static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

public:
    MyDriver();   // calls initialize(std::nothrow) internally — throwing constructor is fine here
    ~MyDriver() override;
    // ...
    static void cleanup();   // resets s_initialized so a new driver can re-init
};
```

```cpp
// ── Implementation (driver_01.cpp) ────────────────────────────────────────────
std::atomic<bool> MyDriver::s_initialized{false};
std::mutex        MyDriver::s_initMutex;

cpp_dbc::expected<bool, DBException> MyDriver::initialize(std::nothrow_t) noexcept
{
    // Fast path: already initialized (acquire load for visibility)
    if (s_initialized.load(std::memory_order_acquire))
    {
        return true;
    }

    // Slow path: take lock and check again (double-checked locking)
    std::lock_guard<std::mutex> lock(s_initMutex);
    if (s_initialized.load(std::memory_order_acquire))
    {
        return true;
    }

    try
    {
        c_library_init();  // one-time C library global init
    }
    catch (const std::exception &ex)
    {
        return std::unexpected(DBException("XXXXXXXXXXXX", ex.what(), system_utils::captureCallStack()));
    }
    catch (...)
    {
        return std::unexpected(DBException("XXXXXXXXXXXX", "Unknown error during library init", system_utils::captureCallStack()));
    }

    s_initialized.store(true, std::memory_order_release);
    return true;
}

MyDriver::MyDriver()
{
    auto result = initialize(std::nothrow);
    if (!result.has_value())
    {
        throw result.error();  // constructor may throw; that is acceptable here
    }
}

void MyDriver::cleanup()
{
    std::lock_guard<std::mutex> lock(s_initMutex);
    c_library_cleanup();
    s_initialized.store(false, std::memory_order_release);
}
```

**Key constraints for this variant**:
- `s_initialized` and `s_initMutex` are **`static` members** — shared across all instances of the driver class.
- `cleanup()` **must reset** `s_initialized` to `false` so that a new driver instance can re-initialize the library after cleanup.
- The `MyDriver` constructor **may throw** in this pattern (it is not a `std::nothrow_t` constructor) — the factory pattern does not apply to `DBDriver` classes because they are typically constructed directly via `std::make_shared<MyDriver>()`.
- The static `initialize` helper is **always private** and takes `std::nothrow_t` as its first parameter, following the same nothrow contract as all other private helpers.

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