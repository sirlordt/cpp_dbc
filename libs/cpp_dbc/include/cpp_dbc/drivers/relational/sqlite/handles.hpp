#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
#include <sqlite3.h>
#include <memory>
#include <mutex>

namespace cpp_dbc::SQLite
{
#if DB_DRIVER_THREAD_SAFE
    /**
     * @brief Shared mutex type for connection and prepared statements
     *
     * This shared_ptr to a recursive_mutex ensures that the Connection and all its
     * PreparedStatements share the SAME mutex. This prevents race conditions when:
     * - PreparedStatement destructor calls sqlite3_finalize()
     * - Another thread uses the connection for queries or pool validation
     *
     * Although SQLite is an embedded database without network protocol concerns,
     * concurrent access to the sqlite3* handle from multiple threads is still unsafe.
     */
    using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;
#endif

    /**
     * @brief Custom deleter for sqlite3_stmt* to use with unique_ptr
     *
     * This deleter ensures that sqlite3_finalize() is called automatically
     * when the unique_ptr goes out of scope, preventing memory leaks.
     */
    struct SQLiteStmtDeleter
    {
        void operator()(sqlite3_stmt *stmt) const noexcept
        {
            if (stmt)
            {
                sqlite3_finalize(stmt);
            }
        }
    };

    /**
     * @brief Type alias for the smart pointer managing sqlite3_stmt
     *
     * Uses unique_ptr with custom deleter to ensure automatic cleanup
     * of SQLite statements, even in case of exceptions.
     */
    using SQLiteStmtHandle = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;

    /**
     * @brief Custom deleter for sqlite3* to use with shared_ptr
     *
     * This deleter ensures that sqlite3_close_v2() is called automatically
     * when the shared_ptr reference count reaches zero, preventing resource leaks.
     */
    struct SQLiteDbDeleter
    {
        void operator()(sqlite3 *db) const noexcept
        {
            if (db)
            {
                // Finalize all remaining statements before closing
                sqlite3_stmt *stmt;
                while ((stmt = sqlite3_next_stmt(db, nullptr)) != nullptr)
                {
                    sqlite3_finalize(stmt);
                }
                sqlite3_close_v2(db);
            }
        }
    };

    // Forward declaration
    class SQLiteDBConnection;

    /**
     * @brief Type alias for the smart pointer managing sqlite3 connection (shared_ptr for weak_ptr support)
     *
     * Uses shared_ptr to allow PreparedStatements to use weak_ptr for safe connection detection.
     * Note: The deleter is passed to the constructor, not as a template parameter.
     */
    using SQLiteDbHandle = std::shared_ptr<sqlite3>;

    /**
     * @brief Factory helper to create SQLiteDbHandle with proper deleter
     *
     * This ensures that sqlite3_close_v2() is always called correctly when
     * the handle is destroyed. Use this instead of constructing SQLiteDbHandle directly.
     *
     * @param db Raw pointer to sqlite3 connection (takes ownership)
     * @return SQLiteDbHandle with proper deleter attached
     */
    inline SQLiteDbHandle makeSQLiteDbHandle(sqlite3 *db)
    {
        return SQLiteDbHandle(db, SQLiteDbDeleter{});
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
