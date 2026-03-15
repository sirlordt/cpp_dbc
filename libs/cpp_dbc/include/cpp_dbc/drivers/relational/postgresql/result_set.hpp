#pragma once

#include "handles.hpp"

#if USE_POSTGRESQL

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBConnection;

    /**
     * @brief PostgreSQL ResultSet implementation using the "Store Result" model
     *
     * @details
     * PostgreSQL uses a "store result" model where PQexec()/PQexecParams() fetches ALL rows
     * from the server into client memory at query execution time. ResultSet operations
     * (next(), getString(), etc.) are purely local memory operations on PGresult*.
     *
     * After the shared-mutex refactoring, ResultSets hold a weak_ptr to their parent
     * Connection and share the connection's recursive_mutex. When the connection closes,
     * all active ResultSets are closed via notifyConnClosing(). This provides consistent
     * lifecycle management across all drivers (Firebird, MySQL, SQLite, PostgreSQL).
     *
     * @see PostgreSQLDBConnection - Creates ResultSets via executeQuery()
     */
    class PostgreSQLDBResultSet final : public RelationalDBResultSet,
                                         public std::enable_shared_from_this<PostgreSQLDBResultSet>
    {
        friend class PostgreSQLDBConnection;
        friend class PostgreSQLConnectionLock;

        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::weak_ptr<PostgreSQLDBConnection> m_connection;
        std::atomic<bool> m_closed{true};

        /**
         * @brief Smart pointer for PGresult - automatically calls PQclear
         *
         * This is an OWNING pointer that manages the lifecycle of the PostgreSQL result set.
         * When this pointer is reset or destroyed, PQclear() is called automatically.
         */
        PGresultHandle m_result;
        int m_rowPosition{0};
        int m_rowCount{0};
        int m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, int> m_columnMap;

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // Internal method called by connection when closing
        void notifyConnClosing(std::nothrow_t) noexcept;

    public:
        PostgreSQLDBResultSet(PrivateCtorTag, std::nothrow_t,
                              std::weak_ptr<PostgreSQLDBConnection> conn,
                              PGresult *res) noexcept;
        ~PostgreSQLDBResultSet() override;

        PostgreSQLDBResultSet(const PostgreSQLDBResultSet &) = delete;
        PostgreSQLDBResultSet &operator=(const PostgreSQLDBResultSet &) = delete;
        PostgreSQLDBResultSet(PostgreSQLDBResultSet &&) = delete;
        PostgreSQLDBResultSet &operator=(PostgreSQLDBResultSet &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<PostgreSQLDBResultSet>
        create(std::weak_ptr<PostgreSQLDBConnection> conn, PGresult *res)
        {
            auto r = create(std::nothrow, std::move(conn), res);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }
#endif

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLDBResultSet>, DBException>
        create(std::nothrow_t,
               std::weak_ptr<PostgreSQLDBConnection> conn,
               PGresult *res) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. The constructor is noexcept and captures errors in m_initFailed.
            auto obj = std::make_shared<PostgreSQLDBResultSet>(
                PrivateCtorTag{}, std::nothrow, std::move(conn), res);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

// DBResultSet interface
#ifdef __cpp_exceptions
        void close() override;
        bool isEmpty() override;

        // RelationalDBResultSet interface
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

        // BLOB support methods
        std::shared_ptr<Blob> getBlob(size_t columnIndex) override;
        std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

        std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) override;
        std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;

        std::vector<uint8_t> getBytes(size_t columnIndex) override;
        std::vector<uint8_t> getBytes(const std::string &columnName) override;

#endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isEmpty(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
        cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
    };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
