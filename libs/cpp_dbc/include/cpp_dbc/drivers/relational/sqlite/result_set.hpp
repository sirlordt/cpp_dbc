#pragma once

#include "handles.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace cpp_dbc::SQLite
{
    class SQLiteDBPreparedStatement; // Forward declaration for prepared statement tracking

    /**
     * @brief SQLite ResultSet implementation
     *
     * IMPORTANT: Thread-Safety and Shared Mutex Design
     * ================================================
     *
     * Unlike MySQL and PostgreSQL, SQLite ResultSets REQUIRE a shared mutex with the
     * Connection because SQLite uses a CURSOR-BASED model that maintains ongoing
     * communication with the database connection.
     *
     * WHY SQLITE/FIREBIRD NEED GLOBAL FILE-LEVEL MUTEX (but MySQL/PostgreSQL don't):
     *
     * MySQL/PostgreSQL ("Store Result" model):
     * - mysql_store_result() / PQexec() fetch ALL data into client memory (MYSQL_RES* / PGresult*)
     * - next() just reads from in-memory structures, no DB communication
     * - close() only frees client memory (mysql_free_result / PQclear)
     * - The result is completely INDEPENDENT of the connection handle
     * - Therefore, no shared mutex is needed
     *
     * SQLite/Firebird ("Cursor" model):
     * - sqlite3_step() / isc_dsql_fetch() communicate with DB for EACH row
     * - sqlite3_column_*() functions access the connection handle internally
     * - sqlite3_finalize() / isc_dsql_free_statement() access the connection handle
     * - Concurrent access from multiple threads causes undefined behavior/crashes
     *
     * RACE CONDITION SCENARIO (without global file-level mutex):
     * Thread A: resultSet->next() -> sqlite3_step() [uses sqlite3* handle]
     * Thread B: connection->isValid() -> SELECT 1 [uses same sqlite3* handle]
     * Result: Memory corruption, crashes, undefined behavior
     *
     * SOLUTION:
     * ResultSet uses the GLOBAL file-level mutex from FileMutexRegistry.
     * ALL connections, PreparedStatements, and ResultSets to the SAME database file
     * share the SAME mutex, ensuring file-level synchronization and eliminating
     * ThreadSanitizer false positives.
     */
    class SQLiteDBResultSet final : public RelationalDBResultSet, public std::enable_shared_from_this<SQLiteDBResultSet>
    {
        friend class SQLiteDBConnection;
        friend class SQLiteDBPreparedStatement;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        /**
         * @brief Raw pointer to sqlite3_stmt
         *
         * IMPORTANT: This is intentionally a raw pointer, NOT a smart pointer, because:
         *
         * 1. When m_ownStatement is true, we own the statement and must finalize it
         *    BUT only if the connection is still valid (not closed).
         *
         * 2. When m_ownStatement is false, the statement is owned by a PreparedStatement
         *    and should not be finalized by the ResultSet.
         *
         * 3. The connection's close() method uses sqlite3_next_stmt() to finalize ALL
         *    statements, so if we try to finalize after connection close, we get double-free.
         *
         * 4. Protection is provided through:
         *    - m_ownStatement flag that determines ownership
         *    - m_connection weak_ptr to check if connection is still valid
         *    - Only finalize if we own AND connection is still valid
         */
        sqlite3_stmt *m_stmt{nullptr};

        bool m_ownStatement{false};
        size_t m_rowPosition{0};
        size_t m_rowCount{0};
        size_t m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, size_t> m_columnMap;
        bool m_hasData{false};
        std::atomic<bool> m_closed{true};
        std::weak_ptr<SQLiteDBConnection> m_connection;               // Weak reference to the connection
        std::weak_ptr<SQLiteDBPreparedStatement> m_preparedStatement; // Optional weak reference to prepared statement (if created from PreparedStatement::executeQuery)

        /**
         * @brief Global file-level mutex shared across all connections to the same database file
         *
         * CRITICAL: This is shared with Connection and PreparedStatements because
         * SQLite uses cursor-based iteration. Unlike MySQL/PostgreSQL where results
         * are fully loaded into client memory, SQLite's sqlite3_step() and
         * sqlite3_column_*() functions communicate with the sqlite3* connection
         * handle on every call.
         *
         * This mutex is obtained from FileMutexRegistry and ensures that ALL operations
         * on the SAME database file (across all connections) are serialized, eliminating
         * ThreadSanitizer false positives and "database is locked" errors.
         *
         * See class documentation above for detailed explanation.
         */
        std::shared_ptr<std::recursive_mutex> m_globalFileMutex;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Helper method to get the active statement pointer
         * @return The active statement pointer
         */
        sqlite3_stmt *getStmt(std::nothrow_t) const noexcept
        {
            return m_stmt;
        }

        /**
         * @brief Initialize and register with parent objects
         *
         * CRITICAL: Must be called AFTER construction is complete, when shared_from_this() is valid.
         * Cannot be called in constructor because weak_from_this() requires the shared_ptr to exist.
         * Returns error if registration with the connection or prepared statement fails.
         */
        cpp_dbc::expected<void, DBException> initialize(std::nothrow_t) noexcept;

    public:
        // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
        SQLiteDBResultSet(PrivateCtorTag,
                          std::nothrow_t,
                          sqlite3_stmt *stmt,
                          bool ownStatement,
                          std::shared_ptr<SQLiteDBConnection> conn,
                          std::shared_ptr<SQLiteDBPreparedStatement> prepStmt,
                          std::shared_ptr<std::recursive_mutex> globalFileMutex) noexcept;

        ~SQLiteDBResultSet() override;

        SQLiteDBResultSet(const SQLiteDBResultSet &) = delete;
        SQLiteDBResultSet &operator=(const SQLiteDBResultSet &) = delete;
        SQLiteDBResultSet(SQLiteDBResultSet &&) = delete;
        SQLiteDBResultSet &operator=(SQLiteDBResultSet &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<SQLiteDBResultSet>
        create(sqlite3_stmt *stmt,
               bool ownStatement,
               std::shared_ptr<SQLiteDBConnection> conn,
               std::shared_ptr<SQLiteDBPreparedStatement> prepStmt,
               std::shared_ptr<std::recursive_mutex> globalFileMutex)
        {
            auto r = create(std::nothrow, stmt, ownStatement, std::move(conn), std::move(prepStmt), std::move(globalFileMutex));
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        bool next() override;
        bool isBeforeFirst() override;
        bool isAfterLast() override;
        uint64_t getRow() override;

        int getInt(size_t columnIndex) override;
        int getInt(const std::string &columnName) override;

        int64_t getLong(size_t columnIndex) override;
        int64_t getLong(const std::string &columnName) override;

        double getDouble(size_t columnIndex) override;
        double getDouble(const std::string &columnName) override;

        std::string getString(size_t columnIndex) override;
        std::string getString(const std::string &columnName) override;

        bool getBoolean(size_t columnIndex) override;
        bool getBoolean(const std::string &columnName) override;

        bool isNull(size_t columnIndex) override;
        bool isNull(const std::string &columnName) override;

        std::string getDate(size_t columnIndex) override;
        std::string getDate(const std::string &columnName) override;

        std::string getTimestamp(size_t columnIndex) override;
        std::string getTimestamp(const std::string &columnName) override;

        std::string getTime(size_t columnIndex) override;
        std::string getTime(const std::string &columnName) override;

        std::vector<std::string> getColumnNames() override;
        size_t getColumnCount() override;
        void close() override;
        bool isEmpty() override;

        // BLOB support methods
        std::shared_ptr<Blob> getBlob(size_t columnIndex) override;
        std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

        std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) override;
        std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;

        std::vector<uint8_t> getBytes(size_t columnIndex) override;
        std::vector<uint8_t> getBytes(const std::string &columnName) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<SQLiteDBResultSet>, DBException>
        create(std::nothrow_t,
               sqlite3_stmt *stmt,
               bool ownStatement,
               std::shared_ptr<SQLiteDBConnection> conn,
               std::shared_ptr<SQLiteDBPreparedStatement> prepStmt,
               std::shared_ptr<std::recursive_mutex> globalFileMutex) noexcept
        {
            // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
            auto rs = std::make_shared<SQLiteDBResultSet>(
                PrivateCtorTag{}, std::nothrow, stmt, ownStatement,
                std::move(conn), std::move(prepStmt), std::move(globalFileMutex));
            if (rs->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*rs->m_initError));
            }
            // Must be called after make_shared (requires shared_ptr to exist for shared_from_this())
            auto initResult = rs->initialize(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return rs;
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isEmpty(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
        cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
    };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
