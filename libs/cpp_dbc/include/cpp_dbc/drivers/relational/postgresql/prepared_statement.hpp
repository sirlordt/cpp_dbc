#pragma once

#include "handles.hpp"

#if USE_POSTGRESQL

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
    class PostgreSQLDBPreparedStatement final : public RelationalDBPreparedStatement
    {
        friend class PostgreSQLDBConnection;

        std::weak_ptr<PGconn> m_conn; // Safe weak reference to connection - detects when connection is closed
        std::string m_sql;
        std::string m_stmtName;
        std::vector<std::string> m_paramValues;
        std::vector<size_t> m_paramLengths;
        std::vector<int> m_paramFormats;
        std::vector<Oid> m_paramTypes;
        bool m_prepared{false};
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent connection
         *
         * This is the SAME mutex instance as the connection's m_connMutex.
         * All operations on both Connection and PreparedStatement lock this mutex,
         * ensuring DEALLOCATE in the destructor never races with other
         * connection operations.
         */
        SharedConnMutex m_connMutex;
#endif

        // Internal method called by connection when closing
        void notifyConnClosing();

        // Helper method to process SQL and count parameters
        int processSQL(std::string &sqlQuery) const;

        // Helper method to get PGconn* safely, throws if connection is closed
        PGconn *getPGConnection() const;

    public:
#if DB_DRIVER_THREAD_SAFE
        PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn, SharedConnMutex connMutex, const std::string &sql, const std::string &stmt_name);
#else
        PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn, const std::string &sql, const std::string &stmt_name);
#endif
        ~PostgreSQLDBPreparedStatement() override;

        // Rule of 5: Non-copyable and non-movable (shares connection mutex and manages server-side state)
        PostgreSQLDBPreparedStatement(const PostgreSQLDBPreparedStatement &) = delete;
        PostgreSQLDBPreparedStatement &operator=(const PostgreSQLDBPreparedStatement &) = delete;
        PostgreSQLDBPreparedStatement(PostgreSQLDBPreparedStatement &&) = delete;
        PostgreSQLDBPreparedStatement &operator=(PostgreSQLDBPreparedStatement &&) = delete;

        void setInt(int parameterIndex, int value) override;
        void setLong(int parameterIndex, long value) override;
        void setDouble(int parameterIndex, double value) override;
        void setString(int parameterIndex, const std::string &value) override;
        void setBoolean(int parameterIndex, bool value) override;
        void setNull(int parameterIndex, Types type) override;
        void setDate(int parameterIndex, const std::string &value) override;
        void setTimestamp(int parameterIndex, const std::string &value) override;

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

        // Nothrow API
        cpp_dbc::expected<void, DBException> setInt(std::nothrow_t, int parameterIndex, int value) noexcept override;
        cpp_dbc::expected<void, DBException> setLong(std::nothrow_t, int parameterIndex, long value) noexcept override;
        cpp_dbc::expected<void, DBException> setDouble(std::nothrow_t, int parameterIndex, double value) noexcept override;
        cpp_dbc::expected<void, DBException> setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        cpp_dbc::expected<void, DBException> setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept override;
        cpp_dbc::expected<void, DBException> setNull(std::nothrow_t, int parameterIndex, Types type) noexcept override;
        cpp_dbc::expected<void, DBException> setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        cpp_dbc::expected<void, DBException> setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
        cpp_dbc::expected<void, DBException> setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept override;
        cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept override;
        cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept override;
        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
