#pragma once

#include "handles.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <vector>
#include <string>
#include <memory>
#include <atomic>

namespace cpp_dbc::Firebird
{
    // Forward declaration
    class FirebirdDBConnection;

    /**
     * @brief Firebird PreparedStatement implementation
     */
    class FirebirdDBPreparedStatement final : public RelationalDBPreparedStatement
    {
        friend class FirebirdDBConnection;

    private:
        std::weak_ptr<isc_db_handle> m_dbHandle;
        std::weak_ptr<FirebirdDBConnection> m_connection; // Reference to connection for autocommit
        isc_tr_handle *m_trPtr{nullptr};                  // Non-owning pointer to transaction handle (owned by Connection)
        isc_stmt_handle m_stmt;
        std::string m_sql;
        XSQLDAHandle m_inputSqlda;
        XSQLDAHandle m_outputSqlda;
        bool m_closed{true};
        bool m_prepared{false};
        std::atomic<bool> m_invalidated{false}; // Set to true when connection invalidates this statement due to DDL

        // Parameter storage
        std::vector<std::vector<char>> m_paramBuffers;
        std::vector<short> m_paramNullIndicators;
        std::vector<std::vector<uint8_t>> m_blobValues;
        std::vector<std::shared_ptr<Blob>> m_blobObjects;
        std::vector<std::shared_ptr<InputStream>> m_streamObjects;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent Connection
         *
         * This mutex is shared between the Connection and all PreparedStatements created from it.
         * This ensures that when close() calls isc_dsql_free_statement(), no other thread can
         * simultaneously use the same database handle.
         */
        SharedConnMutex m_connMutex;
#endif

        void notifyConnClosing();
        isc_db_handle *getFirebirdConnection() const;
        void prepareStatement();
        void allocateInputSqlda();
        void setParameter(int parameterIndex, const void *data, size_t length, short sqlType);

        /**
         * @brief Invalidate this prepared statement (called by connection before DDL operations)
         * After invalidation, any attempt to use this statement will throw/return an error.
         */
        void invalidate();

    public:
#if DB_DRIVER_THREAD_SAFE
        FirebirdDBPreparedStatement(std::weak_ptr<isc_db_handle> db, isc_tr_handle *trPtr, const std::string &sql,
                                    SharedConnMutex connMutex,
                                    std::weak_ptr<FirebirdDBConnection> conn = std::weak_ptr<FirebirdDBConnection>());
#else
        FirebirdDBPreparedStatement(std::weak_ptr<isc_db_handle> db, isc_tr_handle *trPtr, const std::string &sql,
                                    std::weak_ptr<FirebirdDBConnection> conn = std::weak_ptr<FirebirdDBConnection>());
#endif
        ~FirebirdDBPreparedStatement() override;

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

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
