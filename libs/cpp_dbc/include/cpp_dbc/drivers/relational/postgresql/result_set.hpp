#pragma once

#include "handles.hpp"

#if USE_POSTGRESQL

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace cpp_dbc::PostgreSQL
{
    /**
     * @brief PostgreSQL ResultSet implementation using the "Store Result" model
     *
     * @details
     * **IMPORTANT ARCHITECTURAL NOTE - "Store Result" Model:**
     *
     * PostgreSQL uses a "store result" model where PQexec()/PQexecParams() fetches ALL rows
     * from the server into client memory at query execution time. This is fundamentally
     * different from SQLite/Firebird's cursor-based iteration.
     *
     * **HOW IT WORKS:**
     *
     * 1. Query execution calls PQexec() or PQexecParams() which:
     *    - Fetches ALL rows from the PostgreSQL server
     *    - Stores them in a client-side PGresult* structure
     *    - This structure is INDEPENDENT of the PGconn* connection handle
     *
     * 2. ResultSet operations (next(), getString(), etc.):
     *    - next() simply increments m_rowPosition counter (no server communication)
     *    - PQgetvalue() reads from local memory (PGresult*), NOT from server
     *    - These operations do NOT communicate with the database connection
     *
     * 3. ResultSet close:
     *    - PQclear() only frees the local PGresult* memory
     *    - Does NOT communicate with the connection or server
     *
     * **WHY THE MUTEX IS INDEPENDENT (NOT SHARED WITH CONNECTION):**
     *
     * Unlike SQLite/Firebird where each next() call communicates with the connection,
     * PostgreSQL ResultSet operations are purely local memory operations on PGresult*.
     * Therefore:
     *
     * - No race condition with connection operations (pool validation, queries, etc.)
     * - The ResultSet mutex (m_mutex) only protects internal state consistency
     * - It does NOT need to be the same mutex as the connection's m_connMutex
     *
     * **WHAT HAPPENS IF THE CONNECTION IS CLOSED:**
     *
     * If the parent connection is closed while a ResultSet is still open:
     *
     * 1. The ResultSet REMAINS FULLY VALID and usable
     * 2. All data is already in the PGresult* structure (client memory)
     * 3. next(), getString(), getInt(), etc. continue to work normally
     * 4. close() still works (just frees PGresult* memory)
     *
     * This is in stark contrast to SQLite/Firebird where closing the connection
     * would invalidate the ResultSet because cursor iteration requires the connection.
     *
     * **COMPARISON WITH CURSOR-BASED DRIVERS (SQLite/Firebird):**
     *
     * | Aspect                    | MySQL/PostgreSQL          | SQLite/Firebird           |
     * |---------------------------|---------------------------|---------------------------|
     * | Data location             | Client memory (PGresult*) | Server-side cursor        |
     * | next() communication      | Local counter increment   | Connection handle call    |
     * | Connection dependency     | Only at query time        | Throughout iteration      |
     * | Shared mutex needed       | NO                        | YES                       |
     * | Valid after conn close    | YES (data in memory)      | NO (cursor invalidated)   |
     *
     * @see PostgreSQLDBConnection - Creates ResultSets via executeQuery()
     * @see SQLiteDBResultSet - Contrast: Uses shared mutex due to cursor model
     * @see FirebirdDBResultSet - Contrast: Uses shared mutex due to cursor model
     */
    class PostgreSQLDBResultSet final : public RelationalDBResultSet
    {
    private:
        /**
         * @brief Smart pointer for PGresult - automatically calls PQclear
         *
         * This is an OWNING pointer that manages the lifecycle of the PostgreSQL result set.
         * When this pointer is reset or destroyed, PQclear() is called automatically.
         *
         * @note This structure contains ALL result data in client memory, independent
         * of the PGconn* connection handle. The connection can be closed and this
         * ResultSet remains valid.
         */
        PGresultHandle m_result;
        int m_rowPosition{0};
        int m_rowCount{0};
        int m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, int> m_columnMap;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Independent mutex for thread-safe ResultSet operations
         *
         * @details
         * This mutex is INDEPENDENT of the connection's mutex (m_connMutex) because:
         *
         * 1. **No connection communication**: All ResultSet operations (next(), getString(),
         *    etc.) only access the PGresult* structure in client memory. They do NOT
         *    communicate with the PGconn* connection handle.
         *
         * 2. **No race condition possible**: Since we never touch the connection, there's
         *    no risk of racing with connection operations (pool validation, new queries, etc.)
         *
         * 3. **Self-contained protection**: This mutex only needs to protect the internal
         *    state of THIS ResultSet (m_rowPosition) from concurrent access
         *    to THIS same ResultSet instance.
         *
         * **CONTRAST WITH SQLite/Firebird:**
         *
         * SQLite and Firebird use cursor-based iteration where each next() call invokes
         * sqlite3_step() or isc_dsql_fetch() which communicate with the connection handle.
         * Those drivers MUST share the connection mutex to prevent race conditions.
         *
         * PostgreSQL's "store result" model eliminates this coupling entirely.
         */
        mutable std::recursive_mutex m_mutex;
#endif

    public:
        explicit PostgreSQLDBResultSet(PGresult *res);
        ~PostgreSQLDBResultSet() override;

        PostgreSQLDBResultSet(const PostgreSQLDBResultSet &) = delete;
        PostgreSQLDBResultSet &operator=(const PostgreSQLDBResultSet &) = delete;
        PostgreSQLDBResultSet(PostgreSQLDBResultSet &&) = delete;
        PostgreSQLDBResultSet &operator=(PostgreSQLDBResultSet &&) = delete;

        // DBResultSet interface
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

        // Nothrow API
        cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
        cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
        cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
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
