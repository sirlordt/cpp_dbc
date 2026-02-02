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
- Always try to use C++17 style constructs and functions for more secure code.
- Use ranges and avoid index loops where possible.

## Thread Safety

- Use `DB_DRIVER_LOCK_GUARD(m_mutex)` macro for conditional locking
- The `DB_DRIVER_THREAD_SAFE` flag controls whether mutexes are active
- Prefer `std::scoped_lock` over other lock types where possible, as it's more secure

## Input Validation

- Validate database identifiers (keyspace, database names) against injection
- Only allow alphanumeric characters and underscores in schema names
