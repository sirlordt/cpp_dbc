#pragma once

#include "handles.hpp"

#if USE_MYSQL

#include <atomic>
#include <vector>
#include <string>

namespace cpp_dbc::MySQL
{
    class MySQLDBConnection; // Forward declaration for friend

    /**
     * @brief MySQL prepared statement implementation
     *
     * Concrete RelationalDBPreparedStatement for MySQL/MariaDB.
     * Uses mysql_stmt_* API for parameter binding and execution.
     *
     * ```cpp
     * auto stmt = conn->prepareStatement("INSERT INTO users (name, age) VALUES (?, ?)");
     * stmt->setString(1, "Alice");
     * stmt->setInt(2, 30);
     * stmt->executeUpdate();
     * stmt->close();
     * ```
     *
     * @see MySQLDBConnection, MySQLDBResultSet
     */
    class MySQLDBPreparedStatement final : public RelationalDBPreparedStatement
    {
        friend class MySQLDBConnection;
        friend class MySQLConnectionLock;

        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Member variables ──────────────────────────────────────────────────
        std::weak_ptr<MySQLDBConnection> m_connection; // Weak reference to parent connection
        std::string m_sql;
        MySQLStmtHandle m_stmt{nullptr}; // Smart pointer for MYSQL_STMT - automatically calls mysql_stmt_close
        std::atomic<bool> m_closed{true};
        std::vector<MYSQL_BIND> m_binds;
        std::vector<std::string> m_stringValues;                   // To keep string values alive
        std::vector<std::string> m_parameterValues;                // To store parameter values for query reconstruction
        std::vector<int> m_intValues;                              // To keep int values alive
        std::vector<int64_t> m_longValues;                         // To keep long values alive
        std::vector<double> m_doubleValues;                        // To keep double values alive
        std::vector<char> m_nullFlags;                             // To keep null flags alive (char instead of bool for pointer access)
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

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
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Private helper methods ────────────────────────────────────────────

        // Internal method called by connection when closing
        cpp_dbc::expected<void, DBException> notifyConnClosing(std::nothrow_t) noexcept;

        // Helper method to get MYSQL* safely, returns unexpected if connection is closed
        cpp_dbc::expected<MYSQL *, DBException> getMySQLConnection(std::nothrow_t) const noexcept;

    public:
        // ── PrivateCtorTag constructor ────────────────────────────────────────
        /**
         * @brief Nothrow constructor — contains all initialization logic.
         *
         * Calls mysql_stmt_init, mysql_stmt_prepare, and parameter allocation
         * internally. On failure, sets m_initFailed and m_initError instead of
         * throwing. Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        MySQLDBPreparedStatement(PrivateCtorTag,
                                 std::nothrow_t,
                                 std::weak_ptr<MySQLDBConnection> conn,
                                 const std::string &sql) noexcept;

        // ── Destructor ────────────────────────────────────────────────────────
        ~MySQLDBPreparedStatement() override;

        // ── Deleted copy/move — non-copyable, non-movable ─────────────────────
        MySQLDBPreparedStatement(const MySQLDBPreparedStatement &) = delete;
        MySQLDBPreparedStatement &operator=(const MySQLDBPreparedStatement &) = delete;
        MySQLDBPreparedStatement(MySQLDBPreparedStatement &&) = delete;
        MySQLDBPreparedStatement &operator=(MySQLDBPreparedStatement &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<MySQLDBPreparedStatement>
        create(std::weak_ptr<MySQLDBConnection> conn, const std::string &sql)
        {
            auto r = create(std::nothrow, std::move(conn), sql);
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

        static cpp_dbc::expected<std::shared_ptr<MySQLDBPreparedStatement>, DBException>
        create(std::nothrow_t,
               std::weak_ptr<MySQLDBConnection> conn,
               const std::string &sql) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. Catching it would hide a catastrophic failure as a silent error
            // return. Letting std::terminate fire is safer and more debuggable.
            auto obj = std::make_shared<MySQLDBPreparedStatement>(
                PrivateCtorTag{}, std::nothrow, std::move(conn), sql);
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

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
