# Error Handling Patterns in cpp_dbc

This document describes the error handling patterns used in the cpp_dbc library. All drivers must follow these patterns for consistency.

## Table of Contents

1. [Overview](#overview)
2. [DBException Class](#dbexception-class)
3. [Error Codes](#error-codes)
4. [Exception-Based API](#exception-based-api)
5. [Exception-Free API (nothrow)](#exception-free-api-nothrow)
6. [Patterns by Component](#patterns-by-component)
7. [Best Practices](#best-practices)

---

## Overview

cpp_dbc provides two error handling approaches:

| Approach | Use Case | Performance | Error Recovery |
|----------|----------|-------------|----------------|
| **Exception-based** | General usage, complex error handling | Standard | Via try/catch |
| **Exception-free (nothrow)** | Performance-critical code, C interop | Zero overhead | Via expected<T,E> |

Both approaches use the same `DBException` class and error codes.

---

## DBException Class

All cpp_dbc errors are wrapped in `DBException`:

```cpp
// Location: libs/cpp_dbc/include/cpp_dbc/core/db_exception.hpp

class DBException final : public std::exception
{
private:
    // Fixed buffer: "XXXXXXXXXXXX: <message>\0" (79 bytes max)
    // Always populated — serves as fallback if heap allocation fails
    char m_full_message[79]{};                       // 12 mark + 2 ": " + 64 msg + 1 null

    // Heap overflow for messages > 64 chars (nullptr when not needed or alloc failed)
    std::shared_ptr<char[]> m_overflow{};             // allocated via new(std::nothrow)

    std::shared_ptr<system_utils::CallStackCapture> m_callstack{}; // optional

public:
    // Constructor — noexcept: fixed buffer always written, heap alloc via new(std::nothrow)
    explicit DBException(
        const std::string &mark,      // 12-char error code
        const std::string &message,   // Human-readable message (truncated at 64 chars in fixed buffer)
        std::shared_ptr<system_utils::CallStackCapture> callstack = nullptr
    ) noexcept;

    // Methods
    const char *what() const noexcept override;       // Full message (overflow or fixed)
    std::string_view what_s() const noexcept;         // Full message as string_view
    std::string_view getMark() const noexcept;        // Error code (zero-copy, always from fixed buffer)
    void printCallStack() const;                      // Print stack trace
    std::span<const system_utils::StackFrame> getCallStack() const; // Stack frames
};
// Object size: ~120 bytes (best case). Overflow and callstack are heap-allocated on demand.
```

### Key Members

| Member | Type | Description |
|--------|------|-------------|
| `m_full_message[79]` | `char[]` | Fixed buffer: "MARK: message" (12 mark + 2 ": " + 64 msg + 1 null) |
| `m_overflow` | `shared_ptr<char[]>` | Heap buffer for messages > 64 chars; `nullptr` when not needed or alloc failed |
| `m_callstack` | `shared_ptr<CallStackCapture>` | Optional stack trace (heap, shared on copy) |

> **Note:** Object size is ~120 bytes. Messages longer than 64 chars spill to the heap overflow buffer via `new(std::nothrow)`. On allocation failure, `what()` returns the truncated fixed buffer (graceful degradation).

---

## Error Codes

### Format Requirements

Error codes must be:
- **12 characters** exactly
- **Uppercase alphanumeric** (A-Z, 0-9)
- **At least 5 letters**
- **No more than 4 consecutive repeated characters**
- **Unique** across the entire project

### Generation

```bash
# Generate 1 unique code
./generate_dbexception_code.sh /path/to/project

# Generate 10 unique codes
./generate_dbexception_code.sh /path/to/project 10

# Check for duplicates
./check_dbexception_codes.sh --check

# Auto-fix duplicates
./check_dbexception_codes.sh --fix
```

### Examples

```cpp
// Correct
throw DBException("7K3F9J2B5Z8D", "Connection failed", system_utils::captureCallStack());
throw DBException("A7B3C9D2E5F1", "Column index out of range", system_utils::captureCallStack());

// Incorrect
throw DBException("CONN_ERROR", ...);    // Not alphanumeric only, wrong length
throw DBException("001", ...);            // Too short
throw DBException("AAAAAB123456", ...);   // 5 consecutive 'A's
throw DBException("123456789012", ...);   // Less than 5 letters
```

---

## Exception-Based API

### Throwing Exceptions

Always include the call stack for debugging:

```cpp
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// Basic pattern
throw DBException("7K3F9J2B5Z8D",
    "Connection failed: " + errorMessage,
    system_utils::captureCallStack());

// With dynamic information
throw DBException("A7B3C9D2E5F1",
    "Column index out of range: " + std::to_string(columnIndex),
    system_utils::captureCallStack());

// With external error info
throw DBException("B8C4D0E6F2A8",
    "MySQL error: " + std::string(mysql_error(conn)),
    system_utils::captureCallStack());
```

### Catching Exceptions

```cpp
try {
    auto conn = driver->connect(url, user, password);
    auto stmt = conn->prepareStatement("SELECT * FROM users");
    auto rs = stmt->executeQuery();

    while (rs->next()) {
        std::cout << rs->getString("name") << std::endl;
    }
}
catch (const cpp_dbc::DBException &e) {
    // Log the error with full details
    std::cerr << "Database error: " << e.what_s() << std::endl;
    std::cerr << "Error code: " << e.getMark() << std::endl;

    // Print stack trace for debugging
    e.printCallStack();

    // Re-throw or handle
    throw;
}
```

### Re-throwing with Additional Context

```cpp
try {
    return executeInternalQuery(sql);
}
catch (const DBException &ex) {
    // Add context and re-throw; preserve original call stack via shared_ptr
    throw DBException("C9D5E1F7A4B0",
        "Failed to execute query '" + sql + "': " + std::string(ex.what_s()),
        ex.m_callStack);  // Preserve original stack (shared_ptr copy, no allocation)
}
```

---

## Exception-Free API (nothrow)

> **Updated 2026-02-18:** All nothrow methods in `DBConnection`, `DBResultSet`, and `RelationalDBConnection` are now pure virtual (`= 0`). Every driver must implement the complete nothrow API surface. There are no default implementations that silently do nothing or return "not implemented" errors.

### Nothrow API Completeness Rule

Every driver must implement ALL of the following nothrow methods from `DBConnection`:
- `close(std::nothrow_t) noexcept`
- `reset(std::nothrow_t) noexcept`
- `isClosed(std::nothrow_t) const noexcept`
- `returnToPool(std::nothrow_t) noexcept`
- `isPooled(std::nothrow_t) const noexcept`
- `getURI(std::nothrow_t) const noexcept`

Relational drivers additionally implement:
- `prepareForPoolReturn(std::nothrow_t) noexcept`
- `prepareForBorrow(std::nothrow_t) noexcept`

### Throwing Wrappers Delegate to Nothrow

All throwing method variants must delegate to their nothrow counterpart (single code path):

```cpp
// Pattern: throwing wrapper always delegates to nothrow implementation
void MySQLDBConnection::close()
{
    auto result = close(std::nothrow);
    if (!result.has_value())
    {
        throw result.error();
    }
}
```

This ensures consistent behavior between the two API surfaces and reduces code duplication.

### Using expected<T, E>

The nothrow API returns `cpp_dbc::expected<T, DBException>`:

```cpp
#include "cpp_dbc/core/db_expected.hpp"

// Signature pattern
cpp_dbc::expected<ReturnType, DBException> methodName(std::nothrow_t, args...) noexcept;

// Examples
cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string& key) noexcept;
cpp_dbc::expected<void, DBException> setAutoCommit(std::nothrow_t, bool autoCommit) noexcept;
cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string& sql) noexcept;
```

### Implementing nothrow Methods

```cpp
cpp_dbc::expected<std::string, DBException>
RedisDBConnection::getString(std::nothrow_t, const std::string &key) noexcept
{
    try
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        if (m_closed)
        {
            return cpp_dbc::unexpected(DBException("7K3F9J2B5Z8D",
                "Connection is closed", system_utils::captureCallStack()));
        }

        // Perform operation...
        redisReply *reply = static_cast<redisReply*>(
            redisCommand(m_context.get(), "GET %s", key.c_str()));

        if (reply == nullptr)
        {
            return cpp_dbc::unexpected(DBException("A8B4C0D6E2F8",
                "Redis command failed", system_utils::captureCallStack()));
        }

        ReplyHandle replyHandle(reply);

        if (reply->type == REDIS_REPLY_NIL)
        {
            return std::string{};  // Success: empty value
        }

        return std::string(reply->str, reply->len);  // Success: actual value
    }
    catch (const DBException &e)
    {
        return cpp_dbc::unexpected(e);
    }
    catch (const std::exception &e)
    {
        return cpp_dbc::unexpected(DBException("B9C5D1E7F3A9",
            "Unexpected error: " + std::string(e.what()),
            system_utils::captureCallStack()));
    }
}
```

### Using nothrow Results

```cpp
// Basic usage
auto result = conn->getString(std::nothrow, "mykey");
if (result) {
    std::cout << "Value: " << result.value() << std::endl;
} else {
    std::cerr << "Error: " << result.error().what_s() << std::endl;
    result.error().printCallStack();
}

// With value_or for defaults
std::string value = conn->getString(std::nothrow, "mykey").value_or("default");

// Monadic operations
auto result = conn->getString(std::nothrow, "user_id")
    .and_then([&](const std::string& id) {
        return conn->getString(std::nothrow, "user:" + id);
    })
    .transform([](const std::string& data) {
        return parseUserData(data);
    });

// For void returns
auto result = conn->setAutoCommit(std::nothrow, false);
if (!result) {
    // Handle error
    handleError(result.error());
}
```

### Returning unexpected

Use `cpp_dbc::unexpected` to return errors:

```cpp
// Return error
return cpp_dbc::unexpected(DBException("A7B3C9D2E5F1",
    "Error message", system_utils::captureCallStack()));

// Return success (for expected<void, E>)
return {};  // or return cpp_dbc::expected<void, DBException>{};

// Return success with value
return someValue;
```

---

## Patterns by Component

### Driver Class

```cpp
std::shared_ptr<RelationalDBConnection>
NewDBDriver::connect(const std::string &url,
                     const std::string &user,
                     const std::string &password)
{
    // Validate URL format
    if (!acceptURI(url))
    {
        throw DBException("A1B2C3D4E5F6",
            "Invalid URL format: " + url,
            system_utils::captureCallStack());
    }

    // Parse URL
    auto parsed = system_utils::parseDBURI(url);
    if (!parsed.valid)
    {
        throw DBException("B2C3D4E5F6A7",
            "Failed to parse URL: " + url,
            system_utils::captureCallStack());
    }

    // Create connection (may throw)
    return std::make_shared<NewDBConnection>(parsed, user, password);
}
```

### Connection Class

```cpp
void NewDBConnection::close()
{
    DB_DRIVER_LOCK_GUARD(m_mutex);

    if (m_closed)
    {
        return;  // Already closed, no error
    }

    // Cleanup resources
    if (m_handle)
    {
        int result = native_close(m_handle.get());
        if (result != 0)
        {
            // Log but don't throw from close()
            NEWDB_DEBUG("Warning: close() returned error: " << result);
        }
    }

    m_closed = true;
}

std::shared_ptr<RelationalDBPreparedStatement>
NewDBConnection::prepareStatement(const std::string &sql)
{
    DB_DRIVER_LOCK_GUARD(m_mutex);

    if (m_closed)
    {
        throw DBException("C3D4E5F6A7B8",
            "Connection has been closed",
            system_utils::captureCallStack());
    }

    // Validate SQL (basic check)
    if (sql.empty())
    {
        throw DBException("D4E5F6A7B8C9",
            "SQL statement cannot be empty",
            system_utils::captureCallStack());
    }

    // Create prepared statement
    return std::make_shared<NewDBPreparedStatement>(
        m_connectionWeak, sql);
}
```

### PreparedStatement Class

```cpp
void NewDBPreparedStatement::setString(int parameterIndex,
                                       const std::string &value)
{
    DB_DRIVER_LOCK_GUARD(m_mutex);

    // Check connection validity
    auto conn = m_connectionWeak.lock();
    if (!conn)
    {
        throw DBException("E5F6A7B8C9D0",
            "Connection has been closed",
            system_utils::captureCallStack());
    }

    // Validate parameter index
    if (parameterIndex < 1 || parameterIndex > m_paramCount)
    {
        throw DBException("F6A7B8C9D0E1",
            "Parameter index out of range: " + std::to_string(parameterIndex) +
            " (valid range: 1-" + std::to_string(m_paramCount) + ")",
            system_utils::captureCallStack());
    }

    // Bind parameter
    int result = native_bind_string(m_stmt.get(), parameterIndex, value.c_str());
    if (result != 0)
    {
        throw DBException("A7B8C9D0E1F2",
            "Failed to bind string parameter: " + getNativeError(),
            system_utils::captureCallStack());
    }
}
```

### ResultSet Class

```cpp
std::string NewDBResultSet::getString(int columnIndex)
{
    DB_DRIVER_LOCK_GUARD(m_mutex);

    // Validate column index
    if (columnIndex < 1 || columnIndex > m_columnCount)
    {
        throw DBException("B8C9D0E1F2A3",
            "Column index out of range: " + std::to_string(columnIndex),
            system_utils::captureCallStack());
    }

    // Check for NULL
    if (isColumnNull(columnIndex))
    {
        m_wasNull = true;
        return {};
    }

    m_wasNull = false;
    return getNativeString(columnIndex);
}

std::string NewDBResultSet::getString(const std::string &columnName)
{
    int index = findColumn(columnName);
    if (index == -1)
    {
        throw DBException("C9D0E1F2A3B4",
            "Column not found: " + columnName,
            system_utils::captureCallStack());
    }
    return getString(index);
}
```

### Destructor Pattern (No Exceptions)

```cpp
NewDBConnection::~NewDBConnection()
{
    try
    {
        if (!m_closed)
        {
            close();
        }
    }
    catch (const std::exception &e)
    {
        // Log but never throw from destructor
        NEWDB_DEBUG("Warning in destructor: " << e.what());
    }
    catch (...)
    {
        NEWDB_DEBUG("Unknown error in destructor");
    }
}
```

---

## Best Practices

### DO

1. **Always capture call stack** when throwing:
   ```cpp
   throw DBException("CODE", "message", system_utils::captureCallStack());
   ```

2. **Include context** in error messages:
   ```cpp
   throw DBException("CODE",
       "Failed to parse column " + columnName + " at index " + std::to_string(index),
       system_utils::captureCallStack());
   ```

3. **Use unique error codes** (generate with script)

4. **Catch specific exceptions first**:
   ```cpp
   catch (const DBException &e) { ... }
   catch (const std::exception &e) { ... }
   ```

5. **Use `what_s()` instead of `what()`** (returns `std::string_view` — zero-copy, safe lifetime; convert to `std::string` before concatenation: `std::string(e.what_s())`)

6. **Guard against double-close**:
   ```cpp
   if (m_closed) return;  // No error for double close
   ```

### DON'T

1. **Don't throw from destructors**:
   ```cpp
   // BAD
   ~Connection() { throw DBException(...); }

   // GOOD
   ~Connection() {
       try { close(); } catch (...) { /* log only */ }
   }
   ```

2. **Don't use generic codes**:
   ```cpp
   // BAD
   throw DBException("ERROR", "Something failed", ...);

   // GOOD
   throw DBException("A1B2C3D4E5F6", "Connection timeout after 30s", ...);
   ```

3. **Don't lose context when re-throwing**:
   ```cpp
   // BAD
   catch (const DBException &e) {
       throw DBException("NEW_CODE", "Failed", {}); // Lost original stack
   }

   // GOOD
   catch (const DBException &ex) {
       throw DBException("NEW_CODE", "Context: " + std::string(ex.what_s()), ex.m_callStack);
   }
   ```

4. **Don't ignore nothrow results**:
   ```cpp
   // BAD - result silently discarded
   conn->setAutoCommit(std::nothrow, false);

   // GOOD
   auto result = conn->setAutoCommit(std::nothrow, false);
   if (!result) { /* handle error */ }
   ```

---

## Error Categories Reference

Common error categories and when to use them:

| Category | When to Use | Example Message |
|----------|-------------|-----------------|
| Connection | Connection failures, timeouts | "Failed to connect to server" |
| Authentication | Auth failures | "Invalid username or password" |
| State | Invalid object state | "Connection has been closed" |
| Parameter | Invalid parameters | "Parameter index out of range: 5" |
| Query | SQL/query errors | "Syntax error in query" |
| Data | Data conversion errors | "Cannot convert NULL to int" |
| Resource | Resource exhaustion | "Connection pool exhausted" |
| Internal | Unexpected internal errors | "Assertion failed: stmt != null" |
