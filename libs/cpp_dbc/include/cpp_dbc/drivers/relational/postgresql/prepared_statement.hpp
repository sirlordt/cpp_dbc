#pragma once

#include "handles.hpp"

#if USE_POSTGRESQL

#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBConnection;

    /**
     * @brief PostgreSQL prepared statement implementation
     *
     * Concrete RelationalDBPreparedStatement for PostgreSQL.
     * Uses server-side prepared statements via PQprepare/PQexecPrepared.
     *
     * ```cpp
     * auto stmt = conn->prepareStatement("INSERT INTO users (name, age) VALUES ($1, $2)");
     * stmt->setString(1, "Alice");
     * stmt->setInt(2, 30);
     * stmt->executeUpdate();
     * stmt->close();
     * ```
     *
     * @note PostgreSQL uses $1, $2, ... for parameter placeholders internally,
     *       but cpp_dbc converts ? placeholders to $N format automatically.
     *
     * @see PostgreSQLDBConnection, PostgreSQLDBResultSet
     */
    class PostgreSQLDBPreparedStatement final : public RelationalDBPreparedStatement,
                                                 public std::enable_shared_from_this<PostgreSQLDBPreparedStatement>
    {
        friend class PostgreSQLDBConnection;
        friend class PostgreSQLConnectionLock;

        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::weak_ptr<PostgreSQLDBConnection> m_connection;
        std::atomic<bool> m_closed{true};
        std::string m_sql;
        std::string m_stmtName;
        std::vector<std::string> m_paramValues;
        std::vector<bool> m_paramIsNull;
        std::vector<size_t> m_paramLengths;
        std::vector<int> m_paramFormats;
        std::vector<Oid> m_paramTypes;
        bool m_prepared{false};
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // Internal method called by connection when closing
        void notifyConnClosing(std::nothrow_t) noexcept;

        // Helper method to process SQL and count parameters
        cpp_dbc::expected<int, DBException> processSQL(std::nothrow_t, std::string &sqlQuery) const noexcept;

        // Helper method to get PGconn* safely through the connection, returns unexpected if connection is closed
        cpp_dbc::expected<PGconn *, DBException> getPGConnection(std::nothrow_t) const noexcept;

    public:
        PostgreSQLDBPreparedStatement(PrivateCtorTag, std::nothrow_t,
                                      std::weak_ptr<PostgreSQLDBConnection> conn,
                                      const std::string &sql, const std::string &stmt_name) noexcept;
        ~PostgreSQLDBPreparedStatement() override;

        // Rule of 5: Non-copyable and non-movable (shares connection mutex and manages server-side state)
        PostgreSQLDBPreparedStatement(const PostgreSQLDBPreparedStatement &) = delete;
        PostgreSQLDBPreparedStatement &operator=(const PostgreSQLDBPreparedStatement &) = delete;
        PostgreSQLDBPreparedStatement(PostgreSQLDBPreparedStatement &&) = delete;
        PostgreSQLDBPreparedStatement &operator=(PostgreSQLDBPreparedStatement &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<PostgreSQLDBPreparedStatement>
        create(std::weak_ptr<PostgreSQLDBConnection> conn, const std::string &sql, const std::string &stmt_name)
        {
            auto r = create(std::nothrow, std::move(conn), sql, stmt_name);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }
#endif

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLDBPreparedStatement>, DBException>
        create(std::nothrow_t,
               std::weak_ptr<PostgreSQLDBConnection> conn,
               const std::string &sql,
               const std::string &stmt_name) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. The constructor is noexcept and captures errors in m_initFailed.
            auto obj = std::make_shared<PostgreSQLDBPreparedStatement>(
                PrivateCtorTag{}, std::nothrow, std::move(conn), sql, stmt_name);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

#ifdef __cpp_exceptions
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

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
