#pragma once

#include "handles.hpp"

#if USE_MYSQL

#include <vector>
#include <string>

namespace cpp_dbc::MySQL
{
        class MySQLDBConnection; // Forward declaration for friend

        class MySQLDBPreparedStatement final : public RelationalDBPreparedStatement
        {
            friend class MySQLDBConnection;

            std::weak_ptr<MYSQL> m_mysql; // Safe weak reference to connection - detects when connection is closed
            std::string m_sql;
            MySQLStmtHandle m_stmt{nullptr}; // Smart pointer for MYSQL_STMT - automatically calls mysql_stmt_close
            std::vector<MYSQL_BIND> m_binds;
            std::vector<std::string> m_stringValues;                   // To keep string values alive
            std::vector<std::string> m_parameterValues;                // To store parameter values for query reconstruction
            std::vector<int> m_intValues;                              // To keep int values alive
            std::vector<long> m_longValues;                            // To keep long values alive
            std::vector<double> m_doubleValues;                        // To keep double values alive
            std::vector<char> m_nullFlags;                             // To keep null flags alive (char instead of bool for pointer access)
            std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

#if DB_DRIVER_THREAD_SAFE
            /**
             * @brief Shared mutex with the parent connection
             *
             * This is the SAME mutex instance as the connection's m_connMutex.
             * All operations on both Connection and PreparedStatement lock this mutex,
             * ensuring mysql_stmt_close() in the destructor never races with other
             * connection operations.
             */
            SharedConnMutex m_connMutex;
#endif

            // Internal method called by connection when closing
            void notifyConnClosing();

            // Helper method to get MYSQL* safely, throws if connection is closed
            MYSQL *getMySQLConnection() const;

        public:
#if DB_DRIVER_THREAD_SAFE
            MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql, SharedConnMutex connMutex, const std::string &sql);
#else
            MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql, const std::string &sql);
#endif
            ~MySQLDBPreparedStatement() override;

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

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
