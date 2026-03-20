#pragma once

#include "handles.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <atomic>
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
    class FirebirdDBResultSet final : public RelationalDBResultSet,
                                      public std::enable_shared_from_this<FirebirdDBResultSet>
    {
        friend class FirebirdDBConnection;
        friend class FirebirdConnectionLock;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        FirebirdStmtHandle m_stmt;
        XSQLDAHandle m_sqlda;
        bool m_ownStatement{false};
        size_t m_rowPosition{0};
        size_t m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, size_t> m_columnMap;
        bool m_hasData{false};
        std::atomic<bool> m_closed{true};
        bool m_fetchedFirst{false};
        std::weak_ptr<FirebirdDBConnection> m_connection;

        // Buffer for SQLDA data
        std::vector<std::vector<char>> m_dataBuffers;
        std::vector<short> m_nullIndicators;

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the private nothrow constructor when SQLDA initialization fails.
         * Inspected by create(nothrow_t) to propagate the error via expected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         * Only allocated on the failure path (~256 bytes saved per successful instance).
         */
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Initialize column metadata from SQLDA
         */
        cpp_dbc::expected<void, DBException> initializeColumns(std::nothrow_t) noexcept;

        /**
         * @brief Get column value as string
         */
        cpp_dbc::expected<std::string, DBException> getColumnValue(std::nothrow_t, size_t columnIndex) const noexcept;

        /**
         * @brief Get raw statement handle pointer for Firebird API calls
         */
        [[nodiscard]] isc_stmt_handle *getStmtPtr(std::nothrow_t) const noexcept
        {
            return m_stmt.get();
        }

        /**
         * @brief Notify this ResultSet that the connection is closing
         */
        cpp_dbc::expected<void, DBException> notifyConnClosing(std::nothrow_t) noexcept;

        /**
         * @brief Register this ResultSet with the parent connection for lifecycle tracking.
         *
         * CRITICAL: Must be called AFTER construction is complete, when shared_from_this() is valid.
         * Cannot be called in the constructor because weak_from_this() requires the shared_ptr to exist.
         */
        cpp_dbc::expected<void, DBException> initialize(std::nothrow_t) noexcept;

    public:
        // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
        FirebirdDBResultSet(PrivateCtorTag,
                            std::nothrow_t,
                            FirebirdStmtHandle stmt,
                            XSQLDAHandle sqlda,
                            bool ownStatement,
                            std::shared_ptr<FirebirdDBConnection> conn) noexcept;

        ~FirebirdDBResultSet() override;

        FirebirdDBResultSet(const FirebirdDBResultSet &) = delete;
        FirebirdDBResultSet &operator=(const FirebirdDBResultSet &) = delete;
        FirebirdDBResultSet(FirebirdDBResultSet &&) = delete;
        FirebirdDBResultSet &operator=(FirebirdDBResultSet &&) = delete;

#ifdef __cpp_exceptions
        // ====================================================================
        // THROWING API — Requires exceptions enabled (-fno-exceptions: excluded)
        // ====================================================================

        static std::shared_ptr<FirebirdDBResultSet>
        create(FirebirdStmtHandle stmt,
               XSQLDAHandle sqlda,
               bool ownStatement,
               std::shared_ptr<FirebirdDBConnection> conn);

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

        std::shared_ptr<Blob> getBlob(size_t columnIndex) override;
        std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

        std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) override;
        std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;

        std::vector<uint8_t> getBytes(size_t columnIndex) override;
        std::vector<uint8_t> getBytes(const std::string &columnName) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — Exception-free interface (always compiled)
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<FirebirdDBResultSet>, DBException>
        create(std::nothrow_t,
               FirebirdStmtHandle stmt,
               XSQLDAHandle sqlda,
               bool ownStatement,
               std::shared_ptr<FirebirdDBConnection> conn) noexcept;

        [[nodiscard]] cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;

        [[nodiscard]] cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;

        [[nodiscard]] cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> isEmpty(std::nothrow_t) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;

        [[nodiscard]] cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
    };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
