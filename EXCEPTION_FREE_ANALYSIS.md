# Analysis: Exception-Free API using std::expected and std::nothrow

## Executive Summary

This document describes the implementation of an optional exception-free API in cpp_dbc using `std::expected<T, E>` (with polyfill for C++11+) and the `std::nothrow` pattern. The architecture inverts the traditional implementation: **the `std::nothrow` versions contain the real logic, and the exception-throwing versions are lightweight wrappers**.

## Proposed Architecture

### Key Principles

1. ✅ **Always available**: Methods with `std::nothrow` are ALWAYS present (no conditional compilation)
2. ✅ **Inverted implementation**: The real logic is in `nothrow` versions, exception versions are wrappers
3. ✅ **No redundancy**: Use `DBException` directly in `expected<T, DBException>` (no creating DBError)
4. ✅ **Single maintenance**: Only one implementation of the logic (in nothrow versions)

### Code Structure

```cpp
#include <new>  // For std::nothrow_t and std::nothrow
#include "cpp_dbc/core/db_expected.hpp"  // Our expected

class RelationalDBConnection {
public:
    // Nothrow version: REAL IMPLEMENTATION
    virtual cpp_dbc::expected<uint64_t, DBException> 
        executeUpdate(std::nothrow_t, const std::string& sql) noexcept = 0;
    
    // Exception version: WRAPPER that calls nothrow
    virtual uint64_t executeUpdate(const std::string& sql) {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result) {
            throw result.error();  // Throws the contained exception
        }
        return *result;
    }
    
    // Same pattern for void:
    virtual cpp_dbc::expected<void, DBException> 
        commit(std::nothrow_t) noexcept = 0;
    
    virtual void commit() {
        auto result = commit(std::nothrow);
        if (!result) {
            throw result.error();
        }
        // For void, there's no value to return
    }
};
```

## Current State vs New

### Before (Exceptions only)
```cpp
uint64_t executeUpdate(const std::string& sql) override {
    if (m_closed) {
        throw DBException("5A6B7C8D9E0F", "Connection is closed");
    }
    if (mysql_query(m_mysql.get(), sql.c_str()) != 0) {
        throw DBException("1G2H3I4J5K6L", 
            std::string("Update failed: ") + mysql_error(m_mysql.get()));
    }
    return mysql_affected_rows(m_mysql.get());
}
```

### After (Inverted implementation)

```cpp
// REAL IMPLEMENTATION (exception-free)
cpp_dbc::expected<uint64_t, DBException> executeUpdate(
    std::nothrow_t, const std::string& sql) noexcept override {
    
    if (m_closed) {
        return cpp_dbc::unexpected(DBException(
            "5A6B7C8D9E0F", 
            "Connection is closed"
        ));
    }
    
    if (mysql_query(m_mysql.get(), sql.c_str()) != 0) {
        return cpp_dbc::unexpected(DBException(
            "1G2H3I4J5K6L",
            std::string("Update failed: ") + mysql_error(m_mysql.get())
        ));
    }
    
    return mysql_affected_rows(m_mysql.get());
}

// WRAPPER (with exceptions) - calls the nothrow version
uint64_t executeUpdate(const std::string& sql) override {
    auto result = executeUpdate(std::nothrow, sql);
    if (!result) {
        throw result.error();
    }
    return *result;
}
```

## Complete Example: MySQLDBConnection

```cpp
class MySQLDBConnection : public RelationalDBConnection {
public:
    // ============================================================
    // REAL IMPLEMENTATIONS (nothrow) - All logic goes here
    // ============================================================
    
    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
    prepareStatement(std::nothrow_t, const std::string& sql) noexcept override {
        if (m_closed) {
            return cpp_dbc::unexpected(DBException(
                "471F2E35F961", "MySQL connection has been closed"
            ));
        }
        
        MYSQL_STMT* stmt = mysql_stmt_init(m_mysql.get());
        if (!stmt) {
            return cpp_dbc::unexpected(DBException(
                "3Y4Z5A6B7C8D", "Failed to initialize statement"
            ));
        }
        
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0) {
            std::string error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return cpp_dbc::unexpected(DBException(
                "9E0F1G2H3I4J", "Failed to prepare statement: " + error
            ));
        }
        
        return std::make_shared<MySQLDBPreparedStatement>(m_mysql, sql);
    }
    
    cpp_dbc::expected<uint64_t, DBException>
    executeUpdate(std::nothrow_t, const std::string& sql) noexcept override {
        if (m_closed) {
            return cpp_dbc::unexpected(DBException(
                "5A6B7C8D9E0F", "Connection is closed"
            ));
        }
        
        if (mysql_query(m_mysql.get(), sql.c_str()) != 0) {
            return cpp_dbc::unexpected(DBException(
                "1G2H3I4J5K6L",
                std::string("Update failed: ") + mysql_error(m_mysql.get())
            ));
        }
        
        return mysql_affected_rows(m_mysql.get());
    }
    
    cpp_dbc::expected<void, DBException>
    commit(std::nothrow_t) noexcept override {
        if (m_closed) {
            return cpp_dbc::unexpected(DBException(
                "3S4T5U6V7W8X", "Connection is closed"
            ));
        }
        
        if (mysql_query(m_mysql.get(), "COMMIT") != 0) {
            return cpp_dbc::unexpected(DBException(
                "9Y0Z1A2B3C4D",
                std::string("Commit failed: ") + mysql_error(m_mysql.get())
            ));
        }
        
        return {}; // expected<void, E> success
    }
    
    // ============================================================
    // WRAPPERS (with exceptions) - Only call nothrow version
    // ============================================================
    
    std::shared_ptr<RelationalDBPreparedStatement> 
    prepareStatement(const std::string& sql) override {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result) {
            throw result.error();
        }
        return *result;
    }
    
    uint64_t executeUpdate(const std::string& sql) override {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result) {
            throw result.error();
        }
        return *result;
    }
    
    void commit() override {
        auto result = commit(std::nothrow);
        if (!result) {
            throw result.error();
        }
    }
};
```

## Client Usage

### Code with Exceptions (existing - no changes)
```cpp
try {
    auto conn = DriverManager::getConnection(url, user, pass);
    auto stmt = conn->prepareStatement("INSERT INTO users VALUES (?, ?)");
    stmt->setString(1, "John");
    stmt->setInt(2, 30);
    uint64_t affected = stmt->executeUpdate();
    conn->commit();
} catch (const DBException& ex) {
    std::cerr << "Error: " << ex.what_s() << std::endl;
    ex.printCallStack();
}
```

### Exception-Free Code (new)
```cpp
// Verbose version
auto conn_result = DriverManager::getConnection(std::nothrow, url, user, pass);
if (!conn_result) {
    std::cerr << "Connection failed: " << conn_result.error().what_s() << std::endl;
    return;
}

auto conn = *conn_result;
auto stmt_result = conn->prepareStatement(std::nothrow, "INSERT INTO users VALUES (?, ?)");
if (!stmt_result) {
    std::cerr << "Prepare failed: " << stmt_result.error().what_s() << std::endl;
    return;
}

auto stmt = *stmt_result;
if (auto result = stmt->setString(std::nothrow, 1, "John"); !result) {
    std::cerr << "Error: " << result.error().what_s() << std::endl;
    return;
}

if (auto result = stmt->setInt(std::nothrow, 2, 30); !result) {
    std::cerr << "Error: " << result.error().what_s() << std::endl;
    return;
}

auto exec_result = stmt->executeUpdate(std::nothrow);
if (!exec_result) {
    std::cerr << "Execute failed: " << exec_result.error().what_s() << std::endl;
    return;
}

uint64_t affected = *exec_result;
auto commit_result = conn->commit(std::nothrow);
if (!commit_result) {
    std::cerr << "Commit failed: " << commit_result.error().what_s() << std::endl;
}
```

### Using with Monadic Operations (C++23)
```cpp
// Fluent usage with and_then, transform, or_else
auto result = DriverManager::getConnection(std::nothrow, url, user, pass)
    .and_then([](auto conn) {
        return conn->prepareStatement(std::nothrow, "SELECT * FROM users WHERE id = ?");
    })
    .and_then([](auto stmt) {
        return stmt->setInt(std::nothrow, 1, 42);
    })
    .and_then([](auto stmt) {
        return stmt->executeQuery(std::nothrow);
    })
    .and_then([](auto rs) -> cpp_dbc::expected<std::vector<User>, DBException> {
        std::vector<User> users;
        while (rs->next()) {
            User u;
            if (auto name = rs->getString(std::nothrow, 1); name) {
                u.name = *name;
            } else {
                return cpp_dbc::unexpected(name.error());
            }
            users.push_back(u);
        }
        return users;
    })
    .or_else([](const DBException& error) -> cpp_dbc::expected<std::vector<User>, DBException> {
        std::cerr << "Database error: " << error.what_s() << std::endl;
        error.printCallStack();
        return cpp_dbc::unexpected(error);
    });

if (result) {
    for (const auto& user : *result) {
        std::cout << "User: " << user.name << std::endl;
    }
}
```

## Advantages of this Architecture

### 1. No Logic Duplication
✅ Business logic is ONLY in the nothrow versions  
✅ Exception versions are trivial wrappers (3 lines)  
✅ Single place to maintain and debug  

### 2. Always Available
✅ No `#if CPP_DBC_USE_EXPECTED`  
✅ Both APIs always coexist  
✅ The user chooses which to use according to their case  

### 3. Natural Migration
✅ Existing code works without changes (uses wrappers)  
✅ New code can gradually adopt nothrow  
✅ Both styles can be mixed in the same application  

### 4. Simplicity
✅ No additional `DBError` type  
✅ Reuses existing `DBException`  
✅ Fewer classes, less complexity  

### 5. Performance
✅ Nothrow users completely avoid exception overhead  
✅ Exception users have minimal additional overhead (one check + throw)  
✅ The compiler can inline the wrappers  

## Changes in CMakeLists.txt

```cmake
# libs/cpp_dbc/CMakeLists.txt

# NO CPP_DBC_USE_EXPECTED option - always available

# Automatic detection of std::expected
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <expected>
    int main() {
        std::expected<int, int> e = 42;
        return e.has_value() ? 0 : 1;
    }
" HAVE_STD_EXPECTED)

if(HAVE_STD_EXPECTED)
    message(STATUS "Using native std::expected (C++23)")
    target_compile_definitions(cpp_dbc PUBLIC CPP_DBC_HAS_STD_EXPECTED=1)
else()
    message(STATUS "Using cpp_dbc::expected polyfill (C++11+)")
endif()

# The CPP_DBC_USE_EXPECTED definition is always 1
target_compile_definitions(cpp_dbc PUBLIC CPP_DBC_USE_EXPECTED=1)
```

## Files to Modify

### Phase 1: Base Infrastructure (Complete)
1. ✅ `libs/cpp_dbc/include/cpp_dbc/core/db_expected.hpp` - Already created
2. ⚠️ `libs/cpp_dbc/CMakeLists.txt` - Update with automatic detection
3. ⚠️ `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp` - Include db_expected.hpp

### Phase 2: Core Interfaces
4. ⚠️ `libs/cpp_dbc/include/cpp_dbc/core/relational/relational_db_connection.hpp`
5. ⚠️ `libs/cpp_dbc/include/cpp_dbc/core/relational/relational_db_prepared_statement.hpp`
6. ⚠️ `libs/cpp_dbc/include/cpp_dbc/core/relational/relational_db_result_set.hpp`

### Phase 3: MySQL Implementation (Prototype)
7. ⚠️ `libs/cpp_dbc/include/cpp_dbc/drivers/relational/driver_mysql.hpp`
8. ⚠️ `libs/cpp_dbc/src/drivers/relational/driver_mysql.cpp`

### Phase 4+: Other Drivers
9. ⚠️ PostgreSQL, SQLite, Firebird, MongoDB, Redis

### Final Phase: Tests and Examples
10. ⚠️ Tests for nothrow API
11. ⚠️ Usage examples

## Interface Example

```cpp
// libs/cpp_dbc/include/cpp_dbc/core/relational/relational_db_connection.hpp

#ifndef CPP_DBC_RELATIONAL_DB_CONNECTION_HPP
#define CPP_DBC_RELATIONAL_DB_CONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <new>  // For std::nothrow_t
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "relational_db_prepared_statement.hpp"
#include "relational_db_result_set.hpp"

namespace cpp_dbc
{
    class RelationalDBConnection : public DBConnection
    {
    public:
        virtual ~RelationalDBConnection() = default;

        // ============================================================
        // Nothrow API (REAL IMPLEMENTATIONS - in derived classes)
        // ============================================================
        
        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &sql) noexcept = 0;

        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &sql) noexcept = 0;

        virtual cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &sql) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
        setAutoCommit(std::nothrow_t, bool autoCommit) noexcept = 0;

        virtual cpp_dbc::expected<bool, DBException>
        getAutoCommit(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<bool, DBException>
        beginTransaction(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<bool, DBException>
        transactionActive(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
        commit(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
        rollback(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept = 0;

        virtual cpp_dbc::expected<TransactionIsolationLevel, DBException>
        getTransactionIsolation(std::nothrow_t) noexcept = 0;

        // ============================================================
        // Exception API (WRAPPERS - default implementation)
        // ============================================================
        
        virtual std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) {
            auto result = prepareStatement(std::nothrow, sql);
            if (!result) throw result.error();
            return *result;
        }

        virtual std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) {
            auto result = executeQuery(std::nothrow, sql);
            if (!result) throw result.error();
            return *result;
        }

        virtual uint64_t executeUpdate(const std::string &sql) {
            auto result = executeUpdate(std::nothrow, sql);
            if (!result) throw result.error();
            return *result;
        }

        virtual void setAutoCommit(bool autoCommit) {
            auto result = setAutoCommit(std::nothrow, autoCommit);
            if (!result) throw result.error();
        }

        virtual bool getAutoCommit() {
            auto result = getAutoCommit(std::nothrow);
            if (!result) throw result.error();
            return *result;
        }

        virtual bool beginTransaction() {
            auto result = beginTransaction(std::nothrow);
            if (!result) throw result.error();
            return *result;
        }

        virtual bool transactionActive() {
            auto result = transactionActive(std::nothrow);
            if (!result) throw result.error();
            return *result;
        }

        virtual void commit() {
            auto result = commit(std::nothrow);
            if (!result) throw result.error();
        }

        virtual void rollback() {
            auto result = rollback(std::nothrow);
            if (!result) throw result.error();
        }

        virtual void setTransactionIsolation(TransactionIsolationLevel level) {
            auto result = setTransactionIsolation(std::nothrow, level);
            if (!result) throw result.error();
        }

        virtual TransactionIsolationLevel getTransactionIsolation() {
            auto result = getTransactionIsolation(std::nothrow);
            if (!result) throw result.error();
            return *result;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_CONNECTION_HPP
```

## Effort Estimation

| Phase | Files | Approx. methods | Effort (hours) |
|------|----------|----------------|------------------|
| 1. Infrastructure | 3 | - | 4 |
| 2. Core Interfaces | 3 | ~40 wrappers | 8 |
| 3. MySQL (Prototype) | 2 | ~60 | 24 |
| 4. PostgreSQL | 2 | ~60 | 20 |
| 5. SQLite | 2 | ~60 | 20 |
| 6. Firebird | 2 | ~60 | 20 |
| 7. MongoDB | 2 | ~60 | 20 |
| 8. Redis | 2 | ~60 | 20 |
| 9. Tests and Examples | 4 | - | 16 |
| **TOTAL** | **22** | **~380** | **152 hours** |

**Effort reduction:** ~36 hours less than the original proposal (no logic duplication)

## Visual Comparison

```cpp
// ==========================================
// BEFORE: Logic duplication
// ==========================================
uint64_t executeUpdate(const std::string& sql) {
    if (m_closed) throw DBException(...);
    if (mysql_query(...) != 0) throw DBException(...);
    return mysql_affected_rows(...);
}

expected<uint64_t, DBError> executeUpdateSafe(const std::string& sql) noexcept {
    if (m_closed) return unexpected(DBError{...});
    if (mysql_query(...) != 0) return unexpected(DBError{...});
    return mysql_affected_rows(...);
}

// ==========================================
// NOW: Single implementation
// ==========================================
expected<uint64_t, DBException> executeUpdate(
    std::nothrow_t, const std::string& sql) noexcept {
    if (m_closed) return unexpected(DBException(...));
    if (mysql_query(...) != 0) return unexpected(DBException(...));
    return mysql_affected_rows(...);
}

uint64_t executeUpdate(const std::string& sql) {
    auto result = executeUpdate(std::nothrow, sql);
    if (!result) throw result.error();
    return *result;
}
```

## Application to Document DB and Key-Value DB

The same architecture applies to the other cpp_dbc interfaces:

### Document DB (MongoDB)

```cpp
class DocumentDBCollection {
public:
    // ============================================================
    // Nothrow API (REAL IMPLEMENTATIONS)
    // ============================================================
    
    virtual cpp_dbc::expected<DocumentInsertResult, DBException>
    insertOne(std::nothrow_t, std::shared_ptr<DocumentDBData> document,
              const DocumentWriteOptions& options = DocumentWriteOptions()) noexcept = 0;

    virtual cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException>
    findOne(std::nothrow_t, const std::string& filter = "") noexcept = 0;

    virtual cpp_dbc::expected<std::shared_ptr<DocumentDBCursor>, DBException>
    find(std::nothrow_t, const std::string& filter = "") noexcept = 0;

    virtual cpp_dbc::expected<DocumentUpdateResult, DBException>
    updateOne(std::nothrow_t, const std::string& filter, const std::string& update,
              const DocumentUpdateOptions& options = DocumentUpdateOptions()) noexcept = 0;

    virtual cpp_dbc::expected<DocumentDeleteResult, DBException>
    deleteOne(std::nothrow_t, const std::string& filter) noexcept = 0;

    virtual cpp_dbc::expected<std::string, DBException>
    createIndex(std::nothrow_t, const std::string& keys,
                const std::string& options = "") noexcept = 0;

    virtual cpp_dbc::expected<void, DBException>
    drop(std::nothrow_t) noexcept = 0;

    // ============================================================
    // Exception API (WRAPPERS)
    // ============================================================
    
    virtual DocumentInsertResult insertOne(
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions& options = DocumentWriteOptions()) {
        auto result = insertOne(std::nothrow, document, options);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::shared_ptr<DocumentDBData> findOne(const std::string& filter = "") {
        auto result = findOne(std::nothrow, filter);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::shared_ptr<DocumentDBCursor> find(const std::string& filter = "") {
        auto result = find(std::nothrow, filter);
        if (!result) throw result.error();
        return *result;
    }

    virtual DocumentUpdateResult updateOne(
        const std::string& filter, const std::string& update,
        const DocumentUpdateOptions& options = DocumentUpdateOptions()) {
        auto result = updateOne(std::nothrow, filter, update, options);
        if (!result) throw result.error();
        return *result;
    }

    virtual DocumentDeleteResult deleteOne(const std::string& filter) {
        auto result = deleteOne(std::nothrow, filter);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::string createIndex(const std::string& keys, const std::string& options = "") {
        auto result = createIndex(std::nothrow, keys, options);
        if (!result) throw result.error();
        return *result;
    }

    virtual void drop() {
        auto result = drop(std::nothrow);
        if (!result) throw result.error();
    }
};
```

**Usage - Document DB:**
```cpp
// Exception API (no changes):
auto coll = conn->getCollection("users");
auto doc = coll->findOne("{\"name\": \"John\"}");

// Exception-free API (new):
auto coll_res = conn->getCollection(std::nothrow, "users");
if (coll_res) {
    auto doc_res = (*coll_res)->findOne(std::nothrow, "{\"name\": \"John\"}");
    if (doc_res && *doc_res) {
        std::cout << "Found: " << (*doc_res)->toJson() << std::endl;
    }
}
```

### Key-Value DB (Redis)

```cpp
class KVDBConnection : public DBConnection {
public:
    // ============================================================
    // Nothrow API (REAL IMPLEMENTATIONS)
    // ============================================================
    
    virtual cpp_dbc::expected<bool, DBException>
    setString(std::nothrow_t, const std::string& key, const std::string& value,
              std::optional<int64_t> expirySeconds = std::nullopt) noexcept = 0;

    virtual cpp_dbc::expected<std::string, DBException>
    getString(std::nothrow_t, const std::string& key) noexcept = 0;

    virtual cpp_dbc::expected<bool, DBException>
    exists(std::nothrow_t, const std::string& key) noexcept = 0;

    virtual cpp_dbc::expected<bool, DBException>
    deleteKey(std::nothrow_t, const std::string& key) noexcept = 0;

    virtual cpp_dbc::expected<int64_t, DBException>
    increment(std::nothrow_t, const std::string& key, int64_t by = 1) noexcept = 0;

    virtual cpp_dbc::expected<int64_t, DBException>
    listPushRight(std::nothrow_t, const std::string& key, const std::string& value) noexcept = 0;

    virtual cpp_dbc::expected<std::string, DBException>
    listPopLeft(std::nothrow_t, const std::string& key) noexcept = 0;

    virtual cpp_dbc::expected<bool, DBException>
    hashSet(std::nothrow_t, const std::string& key, const std::string& field,
            const std::string& value) noexcept = 0;

    virtual cpp_dbc::expected<std::string, DBException>
    hashGet(std::nothrow_t, const std::string& key, const std::string& field) noexcept = 0;

    virtual cpp_dbc::expected<bool, DBException>
    setAdd(std::nothrow_t, const std::string& key, const std::string& member) noexcept = 0;

    virtual cpp_dbc::expected<bool, DBException>
    sortedSetAdd(std::nothrow_t, const std::string& key, double score,
                 const std::string& member) noexcept = 0;

    // ============================================================
    // Exception API (WRAPPERS)
    // ============================================================
    
    virtual bool setString(const std::string& key, const std::string& value,
                          std::optional<int64_t> expirySeconds = std::nullopt) {
        auto result = setString(std::nothrow, key, value, expirySeconds);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::string getString(const std::string& key) {
        auto result = getString(std::nothrow, key);
        if (!result) throw result.error();
        return *result;
    }

    virtual bool exists(const std::string& key) {
        auto result = exists(std::nothrow, key);
        if (!result) throw result.error();
        return *result;
    }

    virtual bool deleteKey(const std::string& key) {
        auto result = deleteKey(std::nothrow, key);
        if (!result) throw result.error();
        return *result;
    }

    virtual int64_t increment(const std::string& key, int64_t by = 1) {
        auto result = increment(std::nothrow, key, by);
        if (!result) throw result.error();
        return *result;
    }

    virtual int64_t listPushRight(const std::string& key, const std::string& value) {
        auto result = listPushRight(std::nothrow, key, value);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::string listPopLeft(const std::string& key) {
        auto result = listPopLeft(std::nothrow, key);
        if (!result) throw result.error();
        return *result;
    }

    virtual bool hashSet(const std::string& key, const std::string& field,
                        const std::string& value) {
        auto result = hashSet(std::nothrow, key, field, value);
        if (!result) throw result.error();
        return *result;
    }

    virtual std::string hashGet(const std::string& key, const std::string& field) {
        auto result = hashGet(std::nothrow, key, field);
        if (!result) throw result.error();
        return *result;
    }

    virtual bool setAdd(const std::string& key, const std::string& member) {
        auto result = setAdd(std::nothrow, key, member);
        if (!result) throw result.error();
        return *result;
    }

    virtual bool sortedSetAdd(const std::string& key, double score, const std::string& member) {
        auto result = sortedSetAdd(std::nothrow, key, score, member);
        if (!result) throw result.error();
        return *result;
    }
};
```

**Usage - Key-Value DB:**
```cpp
// Exception API (no changes):
auto conn = DriverManager::getConnection("redis://localhost:6379", "", "");
conn->setString("user:1000", "John Doe");
std::string value = conn->getString("user:1000");

// Exception-free API (new):
auto conn_res = DriverManager::getConnection(std::nothrow, "redis://localhost:6379", "", "");
if (conn_res) {
    auto set_res = (*conn_res)->setString(std::nothrow, "user:1000", "John Doe");
    if (set_res && *set_res) {
        auto get_res = (*conn_res)->getString(std::nothrow, "user:1000");
        if (get_res) {
            std::cout << "Value: " << *get_res << std::endl;
        }
    }
}
```

## Summary by Database Type

| Type | Main methods to migrate | Complexity |
|------|------------------------------|-------------|
| **Relational** | ~40 methods (Connection, PreparedStatement, ResultSet) | High |
| **Document** | ~20 methods (Collection, Cursor, Data) | Medium |
| **Key-Value** | ~35 methods (String, List, Hash, Set, SortedSet ops) | Medium |

**Total estimate:** ~95 methods × 4 relational drivers + 20 document + 35 kv = **~435 methods**

## Next Steps

1. ✅ Validate architecture with team
2. ✅ Implementation of `expected` completed and tested
3. ⚠️ Update CMakeLists.txt with automatic detection
4. ⚠️ Implement core interfaces with default wrappers
5. ✅ Implement MySQL as prototype (Relational)
6. ✅ Implement PostgreSQL driver (Relational)
7. ⚠️ Implement MongoDB (Document)
8. ✅ Implement Redis (Key-Value)
9. ⚠️ Apply to remaining drivers (SQLite, Firebird)
10. ⚠️ Comprehensive tests
11. ⚠️ Documentation and examples

## References

- [C++23 std::expected](https://en.cppreference.com/w/cpp/utility/expected)
- [P0323R12: std::expected](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0323r12.html)
- [operator new with nothrow](https://en.cppreference.com/w/cpp/memory/new/operator_new)
