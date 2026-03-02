#pragma once

#include "handles.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace cpp_dbc::Firebird
{
    // Forward declaration
    class FirebirdDBConnection;

    /**
     * @brief Firebird prepared statement implementation
     *
     * Concrete RelationalDBPreparedStatement for Firebird.
     * Uses isc_dsql_prepare/isc_dsql_execute/isc_dsql_execute2 APIs.
     *
     * ```cpp
     * auto stmt = conn->prepareStatement("INSERT INTO users (name, age) VALUES (?, ?)");
     * stmt->setString(1, "Alice");
     * stmt->setInt(2, 30);
     * stmt->executeUpdate();
     * stmt->close();
     * ```
     *
     * @see FirebirdDBConnection, FirebirdDBResultSet
     */
    class FirebirdDBPreparedStatement final : public RelationalDBPreparedStatement,
                                              public std::enable_shared_from_this<FirebirdDBPreparedStatement>
    {
        friend class FirebirdDBConnection;
        friend class FirebirdConnectionLock;

    private:
        // ── Member variables ──────────────────────────────────────────────────
        std::weak_ptr<isc_db_handle> m_dbHandle;

        // ============================================================================
        // FIX #1: Eliminate m_trPtr Dangling Pointer (USE-AFTER-FREE Bug)
        // ============================================================================
        // PROBLEM (BEFORE):
        //   isc_tr_handle *m_trPtr{nullptr};  // Raw pointer to connection's m_tr
        //
        // This raw pointer caused USE-AFTER-FREE bugs:
        //   - If the connection was destroyed while PreparedStatement existed,
        //     m_trPtr pointed to FREED MEMORY (m_tr is a stack member of Connection)
        //   - When executeUpdate() dereferenced it: isc_dsql_execute(..., m_trPtr, ...)
        //     → SEGMENTATION FAULT or memory corruption
        //
        // SOLUTION (NOW):
        //   std::weak_ptr<FirebirdDBConnection> m_connection;
        //
        // Benefits:
        //   1. Lifecycle-safe: weak_ptr detects when connection is destroyed
        //   2. Access pattern: lock() → check → use conn->m_tr → safe
        //   3. Prevents dangling pointer bugs entirely
        //   4. Matches SQLite's pattern with FileMutexRegistry
        //
        // Usage in methods (executeUpdate, executeQuery, etc.):
        //   auto conn = m_connection.lock();
        //   if (!conn) return unexpected("Connection destroyed");
        //   isc_dsql_execute(status, &(conn->m_tr), ...);  // Safe access
        // ============================================================================
        std::weak_ptr<FirebirdDBConnection> m_connection;
        isc_stmt_handle m_stmt{};
        std::string m_sql;
        XSQLDAHandle m_inputSqlda;
        XSQLDAHandle m_outputSqlda;
        std::atomic<bool> m_closed{true};
        bool m_prepared{false};
        std::atomic<bool> m_invalidated{false}; // Set to true when connection invalidates this statement due to DDL

        // Parameter storage
        std::vector<std::vector<char>> m_paramBuffers;
        std::vector<short> m_paramNullIndicators;
        std::vector<std::vector<uint8_t>> m_blobValues;
        std::vector<std::shared_ptr<Blob>> m_blobObjects;
        std::vector<std::shared_ptr<InputStream>> m_streamObjects;

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the private nothrow constructor when statement preparation fails.
         * Inspected by create(nothrow_t) to propagate the error via expected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         */
        DBException m_initError{"W5NGKYRJZB0X", "", {}};

        // ── Private nothrow constructor ───────────────────────────────────────
        /**
         * @brief Private nothrow constructor — contains all initialization logic
         *
         * Calls prepareStatement(std::nothrow) internally. On failure, sets
         * m_initFailed and m_initError instead of throwing. Only intended to be
         * called from the static create() factory methods via `new`.
         *
         * @note create(nothrow_t) uses `new` (not std::make_shared) to access this private constructor.
         */
        FirebirdDBPreparedStatement(std::nothrow_t,
                                    std::weak_ptr<isc_db_handle> db,
                                    const std::string &sql,
                                    std::weak_ptr<FirebirdDBConnection> conn) noexcept;

        // ── Private helper methods ────────────────────────────────────────────
        /**
         * @brief Notify that the connection is closing or a DDL operation is about to execute.
         * Sets m_invalidated and m_closed by calling close(), releasing metadata locks.
         * After this call, any attempt to use this statement will return an error.
         */
        cpp_dbc::expected<void, DBException> notifyConnClosing(std::nothrow_t) noexcept;
        cpp_dbc::expected<isc_db_handle *, DBException> getFirebirdConnection(std::nothrow_t) const noexcept;
        cpp_dbc::expected<void, DBException> prepareStatement(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> allocateInputSqlda(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> setParameter(std::nothrow_t, int parameterIndex, const void *data, size_t length, short sqlType) noexcept;

    public:
        // ── Destructor ────────────────────────────────────────────────────────
        ~FirebirdDBPreparedStatement() override;

        // ── Deleted copy/move — non-copyable, non-movable ─────────────────────
        FirebirdDBPreparedStatement(const FirebirdDBPreparedStatement &) = delete;
        FirebirdDBPreparedStatement &operator=(const FirebirdDBPreparedStatement &) = delete;
        FirebirdDBPreparedStatement(FirebirdDBPreparedStatement &&) = delete;
        FirebirdDBPreparedStatement &operator=(FirebirdDBPreparedStatement &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<FirebirdDBPreparedStatement>
        create(std::weak_ptr<isc_db_handle> db,
               const std::string &sql,
               std::weak_ptr<FirebirdDBConnection> conn)
        {
            auto r = create(std::nothrow, db, sql, conn);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void setInt(int parameterIndex, int value) override;
        void setLong(int parameterIndex, int64_t value) override;
        void setDouble(int parameterIndex, double value) override;
        void setString(int parameterIndex, const std::string &value) override;
        void setBoolean(int parameterIndex, bool value) override;
        void setNull(int parameterIndex, Types type) override;
        void setDate(int parameterIndex, const std::string &value) override;
        void setTimestamp(int parameterIndex, const std::string &value) override;
        void setTime(int parameterIndex, const std::string &value) override;

        // BLOB support methods
        void setBlob(int parameterIndex, std::shared_ptr<Blob> x) override;
        void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) override;
        void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) override;
        void setBytes(int parameterIndex, const std::vector<uint8_t> &x) override;
        void setBytes(int parameterIndex, const uint8_t *x, size_t length) override;

        std::shared_ptr<RelationalDBResultSet> executeQuery() override;
        uint64_t executeUpdate() override;
        bool execute() override;
        void close() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<FirebirdDBPreparedStatement>, DBException>
        create(std::nothrow_t,
               std::weak_ptr<isc_db_handle> db,
               const std::string &sql,
               std::weak_ptr<FirebirdDBConnection> conn) noexcept
        {
            // Use `new` instead of std::make_shared: std::make_shared cannot access private constructors,
            // but a static class member function can. The private nothrow constructor stores init
            // errors in m_initFailed/m_initError rather than throwing, so no try/catch is needed here.
            auto obj = std::shared_ptr<FirebirdDBPreparedStatement>(
                new FirebirdDBPreparedStatement(std::nothrow, db, sql, conn));
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(obj->m_initError);
            }
            return obj;
        }

        [[nodiscard]] cpp_dbc::expected<void, DBException> setInt(std::nothrow_t, int parameterIndex, int value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setDouble(std::nothrow_t, int parameterIndex, double value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setNull(std::nothrow_t, int parameterIndex, Types type) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setTime(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept override;
        [[nodiscard]] cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept override;
        [[nodiscard]] cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
