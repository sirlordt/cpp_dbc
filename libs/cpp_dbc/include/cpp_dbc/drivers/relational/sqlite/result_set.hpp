#pragma once

#include "handles.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <map>
#include <vector>
#include <string>

namespace cpp_dbc::SQLite
{
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
     * WHY SQLITE/FIREBIRD NEED SharedConnMutex (but MySQL/PostgreSQL don't):
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
     * RACE CONDITION SCENARIO (without SharedConnMutex):
     * Thread A: resultSet->next() -> sqlite3_step() [uses sqlite3* handle]
     * Thread B: connection->isValid() -> SELECT 1 [uses same sqlite3* handle]
     * Result: Memory corruption, crashes, undefined behavior
     *
     * SOLUTION:
     * ResultSet shares the SAME mutex as Connection and PreparedStatements,
     * ensuring all operations on the sqlite3* handle are serialized.
     */
    class SQLiteDBResultSet final : public RelationalDBResultSet
    {
    private:
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
        bool m_closed{true};
        std::weak_ptr<SQLiteDBConnection> m_connection; // Weak reference to the connection

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent Connection
         *
         * CRITICAL: This is shared with Connection and PreparedStatements because
         * SQLite uses cursor-based iteration. Unlike MySQL/PostgreSQL where results
         * are fully loaded into client memory, SQLite's sqlite3_step() and
         * sqlite3_column_*() functions communicate with the sqlite3* connection
         * handle on every call. Without this shared mutex, concurrent operations
         * (e.g., pool validation while iterating results) would cause race conditions.
         *
         * See class documentation above for detailed explanation.
         */
        SharedConnMutex m_connMutex;
#endif

        /**
         * @brief Helper method to get the active statement pointer
         * @return The active statement pointer
         */
        sqlite3_stmt *getStmt() const
        {
            return m_stmt;
        }

    public:
#if DB_DRIVER_THREAD_SAFE
        SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteDBConnection> conn, SharedConnMutex connMutex);
#else
        SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement = true, std::shared_ptr<SQLiteDBConnection> conn = nullptr);
#endif
        ~SQLiteDBResultSet() override;

        bool next() override;
        bool isBeforeFirst() override;
        bool isAfterLast() override;
        uint64_t getRow() override;

        int getInt(size_t columnIndex) override;
        int getInt(const std::string &columnName) override;

        long getLong(size_t columnIndex) override;
        long getLong(const std::string &columnName) override;

        double getDouble(size_t columnIndex) override;
        double getDouble(const std::string &columnName) override;

        std::string getString(size_t columnIndex) override;
        std::string getString(const std::string &columnName) override;

        bool getBoolean(size_t columnIndex) override;
        bool getBoolean(const std::string &columnName) override;

        bool isNull(size_t columnIndex) override;
        bool isNull(const std::string &columnName) override;

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

        // Nothrow API
        cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
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
