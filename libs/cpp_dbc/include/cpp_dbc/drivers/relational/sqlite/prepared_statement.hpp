#pragma once

#include "handles.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

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

    private:
        /**
         * @brief Safe weak reference to connection - detects when connection is closed
         *
         * Uses weak_ptr to safely detect when the connection has been closed,
         * preventing use-after-free errors.
         */
        std::weak_ptr<sqlite3> m_db;

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

        bool m_closed{true};
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

        // Registry of active result sets created by this statement
        // NOTE (2026-02-15): m_activeResultSets is now protected by m_globalFileMutex in all methods
        // (registerResultSet, unregisterResultSet, closeAllResultSets) to avoid lock order violations.
        std::set<std::weak_ptr<SQLiteDBResultSet>, std::owner_less<std::weak_ptr<SQLiteDBResultSet>>> m_activeResultSets;
        // std::recursive_mutex m_resultSetsMutex;  // REMOVED (2026-02-15). Replaced by m_globalFileMutex to eliminate lock order violations.

        /**
         * @brief Global file-level mutex shared across all connections to the same database file
         *
         * This mutex is obtained from FileMutexRegistry via the parent Connection and is shared
         * among ALL connections, PreparedStatements, and ResultSets for the SAME database file.
         * This ensures complete file-level synchronization and eliminates ThreadSanitizer false positives.
         */
        std::shared_ptr<std::recursive_mutex> m_globalFileMutex;

        // Internal method called by connection when closing
        void notifyConnClosing();

        // Helper method to get sqlite3* safely, throws if connection is closed
        sqlite3 *getSQLiteConnection() const;

        // Internal methods for result set registry
        void registerResultSet(std::weak_ptr<SQLiteDBResultSet> rs);
        void unregisterResultSet(std::weak_ptr<SQLiteDBResultSet> rs);
        void closeAllResultSets();

    public:
        SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, std::weak_ptr<SQLiteDBConnection> conn, std::shared_ptr<std::recursive_mutex> globalFileMutex, const std::string &sql);
        ~SQLiteDBPreparedStatement() override;

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

        // Nothrow API
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
