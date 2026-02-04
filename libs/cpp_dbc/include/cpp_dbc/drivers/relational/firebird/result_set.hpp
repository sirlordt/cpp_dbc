#pragma once

#include "handles.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace cpp_dbc::Firebird
{
    /**
     * @brief Firebird ResultSet implementation
     *
     * IMPORTANT: Thread-Safety and Shared Mutex Design
     * ================================================
     *
     * Unlike MySQL and PostgreSQL, Firebird ResultSets REQUIRE a shared mutex with the
     * Connection because Firebird uses a CURSOR-BASED model that maintains ongoing
     * communication with the database connection.
     *
     * WHY FIREBIRD/SQLITE NEED SharedConnMutex (but MySQL/PostgreSQL don't):
     *
     * MySQL/PostgreSQL ("Store Result" model):
     * - mysql_store_result() / PQexec() fetch ALL data into client memory (MYSQL_RES* / PGresult*)
     * - next() just reads from in-memory structures, no DB communication
     * - close() only frees client memory (mysql_free_result / PQclear)
     * - The result is completely INDEPENDENT of the connection handle
     * - Therefore, no shared mutex is needed
     *
     * Firebird/SQLite ("Cursor" model):
     * - isc_dsql_fetch() / sqlite3_step() communicate with DB for EACH row
     * - getColumnValue() accesses data from internal Firebird structures
     * - isc_dsql_free_statement() / sqlite3_finalize() access the connection handle
     * - Concurrent access from multiple threads causes undefined behavior/crashes
     *
     * RACE CONDITION SCENARIO (without SharedConnMutex):
     * Thread A: resultSet->next() -> isc_dsql_fetch() [uses db/transaction handle]
     * Thread B: connection->isValid() -> SELECT 1 [uses same handles]
     * Result: Memory corruption, crashes, undefined behavior
     *
     * SOLUTION:
     * ResultSet shares the SAME mutex as Connection and PreparedStatements,
     * ensuring all operations on the database handle are serialized.
     */
    class FirebirdDBResultSet final : public RelationalDBResultSet
    {
        friend class FirebirdDBConnection;

    private:
        FirebirdStmtHandle m_stmt;
        XSQLDAHandle m_sqlda;
        bool m_ownStatement{false};
        size_t m_rowPosition{0};
        size_t m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, size_t> m_columnMap;
        bool m_hasData{false};
        bool m_closed{true};
        bool m_fetchedFirst{false};
        std::weak_ptr<FirebirdDBConnection> m_connection;

        // Buffer for SQLDA data
        std::vector<std::vector<char>> m_dataBuffers;
        std::vector<short> m_nullIndicators;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent Connection
         *
         * CRITICAL: This is shared with Connection and PreparedStatements because
         * Firebird uses cursor-based iteration. Unlike MySQL/PostgreSQL where results
         * are fully loaded into client memory, Firebird's isc_dsql_fetch() communicates
         * with the database connection handle on every call. Without this shared mutex,
         * concurrent operations (e.g., pool validation while iterating results) would
         * cause race conditions.
         *
         * See class documentation above for detailed explanation.
         */
        SharedConnMutex m_connMutex;
#endif

        /**
         * @brief Initialize column metadata from SQLDA
         */
        void initializeColumns();

        /**
         * @brief Get column value as string
         */
        std::string getColumnValue(size_t columnIndex) const;

        /**
         * @brief Get raw statement handle pointer for Firebird API calls
         */
        isc_stmt_handle *getStmtPtr() { return m_stmt.get(); }

        /**
         * @brief Notify this ResultSet that the connection is closing
         */
        void notifyConnClosing();

    public:
#if DB_DRIVER_THREAD_SAFE
        FirebirdDBResultSet(FirebirdStmtHandle stmt, XSQLDAHandle sqlda, bool ownStatement,
                            std::shared_ptr<FirebirdDBConnection> conn, SharedConnMutex connMutex);
#else
        FirebirdDBResultSet(FirebirdStmtHandle stmt, XSQLDAHandle sqlda, bool ownStatement = true,
                            std::shared_ptr<FirebirdDBConnection> conn = nullptr);
#endif
        ~FirebirdDBResultSet() override;

        FirebirdDBResultSet(const FirebirdDBResultSet &) = delete;
        FirebirdDBResultSet &operator=(const FirebirdDBResultSet &) = delete;
        FirebirdDBResultSet(FirebirdDBResultSet &&) = delete;
        FirebirdDBResultSet &operator=(FirebirdDBResultSet &&) = delete;

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

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
