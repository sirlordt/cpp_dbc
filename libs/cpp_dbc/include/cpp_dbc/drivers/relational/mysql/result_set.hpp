#pragma once

#include "handles.hpp"

#if USE_MYSQL

#include <map>
#include <vector>
#include <string>

namespace cpp_dbc::MySQL
{
        /**
         * @brief MySQL ResultSet implementation using the "Store Result" model
         *
         * @details
         * **IMPORTANT ARCHITECTURAL NOTE - "Store Result" Model:**
         *
         * MySQL uses a "store result" model where mysql_store_result() fetches ALL rows
         * from the server into client memory at query execution time. This is fundamentally
         * different from SQLite/Firebird's cursor-based iteration.
         *
         * **HOW IT WORKS:**
         *
         * 1. Query execution calls mysql_store_result() which:
         *    - Fetches ALL rows from the MySQL server
         *    - Stores them in a client-side MYSQL_RES* structure
         *    - This structure is INDEPENDENT of the MYSQL* connection handle
         *
         * 2. ResultSet operations (next(), getString(), etc.):
         *    - mysql_fetch_row() reads from local memory (MYSQL_RES*), NOT from server
         *    - mysql_data_seek() repositions within local memory
         *    - These operations do NOT communicate with the database connection
         *
         * 3. ResultSet close:
         *    - mysql_free_result() only frees the local MYSQL_RES* memory
         *    - Does NOT communicate with the connection or server
         *
         * **WHY THE MUTEX IS INDEPENDENT (NOT SHARED WITH CONNECTION):**
         *
         * Unlike SQLite/Firebird where each next() call communicates with the connection,
         * MySQL ResultSet operations are purely local memory operations on MYSQL_RES*.
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
         * 2. All data is already in the MYSQL_RES* structure (client memory)
         * 3. next(), getString(), getInt(), etc. continue to work normally
         * 4. close() still works (just frees MYSQL_RES* memory)
         *
         * This is in stark contrast to SQLite/Firebird where closing the connection
         * would invalidate the ResultSet because cursor iteration requires the connection.
         *
         * **COMPARISON WITH CURSOR-BASED DRIVERS (SQLite/Firebird):**
         *
         * | Aspect                    | MySQL/PostgreSQL          | SQLite/Firebird           |
         * |---------------------------|---------------------------|---------------------------|
         * | Data location             | Client memory (MYSQL_RES*)| Server-side cursor        |
         * | next() communication      | Local memory read         | Connection handle call    |
         * | Connection dependency     | Only at query time        | Throughout iteration      |
         * | Shared mutex needed       | NO                        | YES                       |
         * | Valid after conn close    | YES (data in memory)      | NO (cursor invalidated)   |
         *
         * @see MySQLDBConnection - Creates ResultSets via executeQuery()
         * @see SQLiteDBResultSet - Contrast: Uses shared mutex due to cursor model
         * @see FirebirdDBResultSet - Contrast: Uses shared mutex due to cursor model
         */
        class MySQLDBResultSet final : public RelationalDBResultSet
        {
        private:
            /**
             * @brief Smart pointer for MYSQL_RES - automatically calls mysql_free_result
             *
             * This is an OWNING pointer that manages the lifecycle of the MySQL result set.
             * When this pointer is reset or destroyed, mysql_free_result() is called automatically.
             *
             * @note This structure contains ALL result data in client memory, independent
             * of the MYSQL* connection handle. The connection can be closed and this
             * ResultSet remains valid.
             */
            MySQLResHandle m_result;

            /**
             * @brief Non-owning pointer to internal data within m_result
             *
             * IMPORTANT: This is intentionally a raw pointer, NOT a smart pointer, because:
             *
             * 1. MYSQL_ROW is a typedef for char** - it points to internal memory managed
             *    by the MYSQL_RES structure, not separately allocated memory.
             *
             * 2. This memory is automatically managed by the MySQL library:
             *    - It is invalidated when mysql_fetch_row() is called again
             *    - It is freed automatically when mysql_free_result() is called on m_result
             *
             * 3. Using a smart pointer here would cause DOUBLE-FREE errors because:
             *    - The smart pointer would try to free memory it doesn't own
             *    - mysql_free_result() would also try to free the same memory
             *
             * 4. Protection is provided through:
             *    - validateCurrentRow() method that checks both m_result and m_currentRow
             *    - Explicit nullification in close() and next() when appropriate
             *    - Exception throwing when accessing invalid state
             */
            MYSQL_ROW m_currentRow{nullptr};

            size_t m_rowPosition{0};
            size_t m_rowCount{0};
            size_t m_fieldCount{0};
            std::vector<std::string> m_columnNames;
            std::map<std::string, size_t> m_columnMap;

#if DB_DRIVER_THREAD_SAFE
            /**
             * @brief Independent mutex for thread-safe ResultSet operations
             *
             * @details
             * This mutex is INDEPENDENT of the connection's mutex (m_connMutex) because:
             *
             * 1. **No connection communication**: All ResultSet operations (next(), getString(),
             *    etc.) only access the MYSQL_RES* structure in client memory. They do NOT
             *    communicate with the MYSQL* connection handle.
             *
             * 2. **No race condition possible**: Since we never touch the connection, there's
             *    no risk of racing with connection operations (pool validation, new queries, etc.)
             *
             * 3. **Self-contained protection**: This mutex only needs to protect the internal
             *    state of THIS ResultSet (m_currentRow, m_rowPosition) from concurrent access
             *    to THIS same ResultSet instance.
             *
             * **CONTRAST WITH SQLite/Firebird:**
             *
             * SQLite and Firebird use cursor-based iteration where each next() call invokes
             * sqlite3_step() or isc_dsql_fetch() which communicate with the connection handle.
             * Those drivers MUST share the connection mutex to prevent race conditions.
             *
             * MySQL's "store result" model eliminates this coupling entirely.
             */
            mutable std::recursive_mutex m_mutex;
#endif

            /**
             * @brief Validates that the result set is still valid (not closed)
             * @throws DBException if m_result is nullptr
             */
            void validateResultState() const;

            /**
             * @brief Validates that there is a current row to read from
             * @throws DBException if m_result is nullptr or m_currentRow is nullptr
             */
            void validateCurrentRow() const;

        public:
            explicit MySQLDBResultSet(MYSQL_RES *res);
            ~MySQLDBResultSet() override;

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

            long getLong(size_t columnIndex) override;
            long getLong(const std::string &columnName) override;

            double getDouble(size_t columnIndex) override;
            double getDouble(const std::string &columnName) override;

            std::string getString(size_t columnIndex) override;
            std::string getString(const std::string &columnName) override;

            bool getBoolean(size_t columnIndex) override;
            bool getBoolean(const std::string &columnName) override;

            bool isNull(size_t columnIndex) override;
            bool isNull(const std::string &columnName) override;

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
            cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
            cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;
            cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
        };

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
