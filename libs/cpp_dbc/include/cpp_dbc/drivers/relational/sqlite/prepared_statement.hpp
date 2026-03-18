#pragma once

#include "handles.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include <mutex>

namespace cpp_dbc::SQLite
{
    class SQLiteDBConnection; // Forward declaration for friend
    class SQLiteDBResultSet;  // Forward declaration for result set tracking

    /**
     * @brief SQLite prepared statement implementation
     *
     * Concrete RelationalDBPreparedStatement for SQLite.
     * Uses sqlite3_prepare_v2, sqlite3_bind_*, and sqlite3_step APIs.
     *
     * ```cpp
     * auto stmt = conn->prepareStatement("INSERT INTO users (name, age) VALUES (?, ?)");
     * stmt->setString(1, "Alice");
     * stmt->setInt(2, 30);
     * stmt->executeUpdate();
     * stmt->close();
     * ```
     *
     * @see SQLiteDBConnection, SQLiteDBResultSet
     */
    class SQLiteDBPreparedStatement final : public RelationalDBPreparedStatement, public std::enable_shared_from_this<SQLiteDBPreparedStatement>
    {
        friend class SQLiteDBConnection;
        friend class SQLiteDBResultSet;
        friend class SQLiteConnectionLock;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        /**
         * @brief Safe weak reference to connection - detects when connection is closed
         *
         * Uses weak_ptr to safely detect when the connection has been closed,
         * preventing use-after-free errors.
         */
        std::weak_ptr<sqlite3> m_conn;

        /**
         * @brief Weak reference to parent connection
         *
         * Used to register/unregister this statement and to register result sets
         */
        std::weak_ptr<SQLiteDBConnection> m_connection;

        std::string m_sql;

        /**
         * @brief Smart pointer for sqlite3_stmt - automatically calls sqlite3_finalize
         *
         * This is an OWNING pointer that manages the lifecycle of the SQLite statement.
         * When this pointer is reset or destroyed, sqlite3_finalize() is called automatically.
         */
        SQLiteStmtHandle m_stmt;

        std::atomic<bool> m_closed{true};
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

        // Registry of active result sets created by this statement
        std::set<std::weak_ptr<SQLiteDBResultSet>, std::owner_less<std::weak_ptr<SQLiteDBResultSet>>> m_activeResultSets;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // Internal method called by connection when closing
        void notifyConnClosing(std::nothrow_t) noexcept;

        // Helper method to get sqlite3* safely, returns unexpected if connection is closed
        cpp_dbc::expected<sqlite3 *, DBException> getSQLiteConnection(std::nothrow_t) const noexcept;

        // Internal methods for result set registry
        cpp_dbc::expected<void, DBException> registerResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> unregisterResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> closeAllResultSets(std::nothrow_t) noexcept;

    public:
        // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
        SQLiteDBPreparedStatement(PrivateCtorTag,
                                  std::nothrow_t,
                                  std::weak_ptr<sqlite3> db,
                                  std::weak_ptr<SQLiteDBConnection> conn,
                                  const std::string &sql) noexcept;

        ~SQLiteDBPreparedStatement() override;

        SQLiteDBPreparedStatement(const SQLiteDBPreparedStatement &) = delete;
        SQLiteDBPreparedStatement &operator=(const SQLiteDBPreparedStatement &) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<SQLiteDBPreparedStatement>
        create(std::weak_ptr<sqlite3> db,
               std::weak_ptr<SQLiteDBConnection> conn,
               const std::string &sql)
        {
            auto r = create(std::nothrow, std::move(db), std::move(conn), sql);
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
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<SQLiteDBPreparedStatement>, DBException>
        create(std::nothrow_t,
               std::weak_ptr<sqlite3> db,
               std::weak_ptr<SQLiteDBConnection> conn,
               const std::string &sql) noexcept
        {
            // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
            auto obj = std::make_shared<SQLiteDBPreparedStatement>(
                PrivateCtorTag{}, std::nothrow, std::move(db), std::move(conn), sql);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
