# cpp_dbc Project Conventions

## DBException Error Codes

Error codes in `DBException` must be:
- **Format**: 12-character uppercase A-Z and 0-9 string. Cannot be only letter or numbers. Must be present both.
- **Uniqueness**: Each code must be unique across the entire project
- **Generation**: Random (not sequential)

```cpp
// Correct
throw DBException("7K3F9J2B5Z8D", "Error message", system_utils::captureCallStack());

// Incorrect
throw DBException("INVALID_KEYSPACE", "Error message", ...);  // Not hexadecimal
throw DBException("001", "Error message", ...);               // Too short
```

## Namespace Style

Use C++17 nested namespace syntax:

```cpp
// Correct
namespace cpp_dbc::MySQL
{
} // namespace cpp_dbc::MySQL
```

## Memory Safety

- Use RAII handles for external resources (BsonHandle, RedisReplyHandle, etc.)
- Defensively check for nulls before dereferencing pointers from external libraries
- Use `std::atomic` with explicit memory ordering (`memory_order_acquire`/`release`)
- Avoid the raw pointer. And use shared_pointer and unique_pointer and weak_pointer where is possible, in special in members state/class variables.
- Try use always the C++17 style, construct, functions. For most secure code.
- Use ranges and avoid index loops where is possible.

## Thread Safety

- Use `DB_DRIVER_LOCK_GUARD(m_mutex)` macro for conditional locking
- The `DB_DRIVER_THREAD_SAFE` flag controls whether mutexes are active
- Use scoped_lock over another *_lock where is possible and more secure

## Input Validation

- Validate database identifiers (keyspace, database names) against injection
- Only allow alphanumeric characters and underscores in schema names
