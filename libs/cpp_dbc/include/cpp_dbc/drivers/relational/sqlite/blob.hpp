#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
#include <cctype>
#include <cstring>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace cpp_dbc::SQLite
{
    /**
     * @brief SQLite Blob implementation with lazy loading from database
     *
     * Extends MemoryBlob with database-backed lazy loading via sqlite3 APIs.
     * Uses weak_ptr to safely detect when the connection has been closed.
     *
     * @see MemoryBlob, SQLiteDBResultSet
     */
    class SQLiteBlob : public MemoryBlob
    {
        /**
         * @brief Safe weak reference to SQLite connection - detects when connection is closed
         *
         * Uses weak_ptr to safely detect when the connection has been closed,
         * preventing use-after-free errors. The connection is managed by SQLiteConnection
         * using shared_ptr with custom deleter.
         */
        std::weak_ptr<sqlite3> m_db;
        std::string m_tableName;
        std::string m_columnName;
        std::string m_rowId;
        mutable bool m_loaded{false};

        /**
         * @brief Helper method to get sqlite3* safely, throws if connection is closed
         * @return The sqlite3 connection pointer
         * @throws DBException if the connection has been closed
         */
        cpp_dbc::expected<sqlite3 *, DBException> getSQLiteConnection(std::nothrow_t) const noexcept
        {
            auto conn = m_db.lock();
            if (!conn)
            {
                return cpp_dbc::unexpected(DBException("8VHM5QBP014Y", "SQLite connection has been closed", system_utils::captureCallStack()));
            }
            return conn.get();
        }

        sqlite3 *getSQLiteConnection() const
        {
            auto r = getSQLiteConnection(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        /**
         * @brief Validate SQL identifier to prevent injection attacks
         * @param identifier The table or column name to validate
         * @throws DBException if the identifier contains invalid characters
         *
         * Only allows alphanumeric characters and underscores to prevent SQL injection.
         * This follows the cpp_dbc project convention for schema name validation.
         */
        static cpp_dbc::expected<void, DBException> validateIdentifier(std::nothrow_t, const std::string &identifier) noexcept
        {
            if (identifier.empty())
            {
                return cpp_dbc::unexpected(DBException("SL0KQV7GOWPA", "Empty SQL identifier not allowed", system_utils::captureCallStack()));
            }
            for (char c : identifier)
            {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                {
                    return cpp_dbc::unexpected(DBException("UJBY12IVMBRI",
                                                           "Invalid character in SQL identifier '" + identifier + "': only alphanumeric and underscore allowed",
                                                           system_utils::captureCallStack()));
                }
            }
            return {};
        }

        static void validateIdentifier(const std::string &identifier)
        {
            auto r = validateIdentifier(std::nothrow, identifier);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

    public:
        /**
         * @brief Check if the database connection is still valid
         * @return true if connection is valid, false if it has been closed
         */
        bool isConnectionValid() const noexcept override
        {
            return !m_db.expired();
        }

        /** @brief Construct an empty BLOB for in-memory use */
        SQLiteBlob(std::shared_ptr<sqlite3> db)
            : m_db(db), m_loaded(true) {}

        /** @brief Construct a lazy-loading BLOB from a database row */
        SQLiteBlob(std::shared_ptr<sqlite3> db, const std::string &tableName,
                   const std::string &columnName, const std::string &rowId)
            : m_db(db), m_tableName(tableName), m_columnName(columnName),
              m_rowId(rowId), m_loaded(false) {}

        /** @brief Construct a BLOB from existing binary data */
        SQLiteBlob(std::shared_ptr<sqlite3> db, const std::vector<uint8_t> &initialData)
            : MemoryBlob(initialData), m_db(db), m_loaded(true) {}

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db) noexcept
        {
            return std::make_shared<SQLiteBlob>(db);
        }

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::string &tableName, const std::string &columnName, const std::string &rowId) noexcept
        {
            return std::make_shared<SQLiteBlob>(db, tableName, columnName, rowId);
        }

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::vector<uint8_t> &initialData) noexcept
        {
            return std::make_shared<SQLiteBlob>(db, initialData);
        }

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db)
        {
            auto r = create(std::nothrow, db);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db,
                                                  const std::string &tableName, const std::string &columnName, const std::string &rowId)
        {
            auto r = create(std::nothrow, db, tableName, columnName, rowId);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db,
                                                  const std::vector<uint8_t> &initialData)
        {
            auto r = create(std::nothrow, db, initialData);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const SQLiteBlob &other) noexcept
        {
            if (this == &other)
            {
                return {};
            }
            auto r = MemoryBlob::copyFrom(std::nothrow, other);
            if (!r.has_value())
            {
                return r;
            }
            m_db = other.m_db;
            m_tableName = other.m_tableName;
            m_columnName = other.m_columnName;
            m_rowId = other.m_rowId;
            m_loaded = other.m_loaded;
            return {};
        }

        void copyFrom(const SQLiteBlob &other)
        {
            auto r = copyFrom(std::nothrow, other);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        /**
         * @brief Load the BLOB data from the database if not already loaded
         * @throws DBException if the connection is closed or loading fails
         */
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept
        {
            if (m_loaded || m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
            {
                return {};
            }
            auto tableResult = validateIdentifier(std::nothrow, m_tableName);
            if (!tableResult.has_value())
            {
                return cpp_dbc::unexpected(tableResult.error());
            }
            auto colResult = validateIdentifier(std::nothrow, m_columnName);
            if (!colResult.has_value())
            {
                return cpp_dbc::unexpected(colResult.error());
            }

            auto dbResult = getSQLiteConnection(std::nothrow);
            if (!dbResult.has_value())
            {
                return cpp_dbc::unexpected(dbResult.error());
            }
            sqlite3 *db = dbResult.value();

            std::string query = "SELECT " + m_columnName + " FROM " + m_tableName + " WHERE rowid = ?";
            sqlite3_stmt *stmt = nullptr;
            int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("BCOMBRIZ3NOO", "Failed to prepare statement for BLOB loading: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }
            result = sqlite3_bind_text(stmt, 1, m_rowId.c_str(), -1, SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                sqlite3_finalize(stmt);
                return cpp_dbc::unexpected(DBException("2F9K4N8V7QXW", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }
            result = sqlite3_step(stmt);
            if (result == SQLITE_ROW)
            {
                const void *blobData = sqlite3_column_blob(stmt, 0);
                int blobSize = sqlite3_column_bytes(stmt, 0);
                if (blobData && blobSize > 0)
                {
                    m_data.resize(blobSize);
                    std::memcpy(m_data.data(), blobData, blobSize);
                }
                else
                {
                    m_data.clear();
                }
            }
            else
            {
                sqlite3_finalize(stmt);
                return cpp_dbc::unexpected(DBException("D281D99D6FAC", "Failed to fetch BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }
            sqlite3_finalize(stmt);
            m_loaded = true;
            return {};
        }

        void ensureLoaded() const
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        size_t length() const override
        {
            auto r = length(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
        {
            auto r = getBytes(std::nothrow, pos, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<InputStream> getBinaryStream() const override
        {
            auto r = getBinaryStream(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
        {
            auto r = setBinaryStream(std::nothrow, pos);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
        {
            auto r = setBytes(std::nothrow, pos, bytes);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
        {
            auto r = setBytes(std::nothrow, pos, bytes, length);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void truncate(size_t len) override
        {
            auto r = truncate(std::nothrow, len);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        /**
         * @brief Save the BLOB data back to the database
         * @throws DBException if the connection is closed or saving fails
         */
        void save()
        {
            auto r = save(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void free() override
        {
            auto r = free(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

#endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::length(std::nothrow);
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::getBytes(std::nothrow, pos, length);
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::getBinaryStream(std::nothrow);
        }

        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBinaryStream(std::nothrow, pos);
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBytes(std::nothrow, pos, bytes);
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
        }

        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::truncate(std::nothrow, len);
        }

        cpp_dbc::expected<void, DBException> save(std::nothrow_t) noexcept
        {
            if (m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
            {
                return {}; // Nothing to save
            }

            // Ensure BLOB data is loaded before saving to prevent overwriting with empty data
            auto loadResult = ensureLoaded(std::nothrow);
            if (!loadResult.has_value())
            {
                return cpp_dbc::unexpected(loadResult.error());
            }

            // Validate identifiers to prevent SQL injection
            auto tableResult = validateIdentifier(std::nothrow, m_tableName);
            if (!tableResult.has_value())
            {
                return cpp_dbc::unexpected(tableResult.error());
            }
            auto colResult = validateIdentifier(std::nothrow, m_columnName);
            if (!colResult.has_value())
            {
                return cpp_dbc::unexpected(colResult.error());
            }

            // Get the SQLite connection safely
            auto dbResult = getSQLiteConnection(std::nothrow);
            if (!dbResult.has_value())
            {
                return cpp_dbc::unexpected(dbResult.error());
            }
            sqlite3 *db = dbResult.value();

            // Construct a query with parameterized rowid to prevent SQL injection
            std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE rowid = ?";

            // Prepare the statement
            sqlite3_stmt *stmt = nullptr;
            int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("78BBDB81BED9", "Failed to prepare statement for BLOB saving: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }

            // Bind the BLOB data (parameter 1)
            result = sqlite3_bind_blob(stmt, 1, m_data.data(), static_cast<int>(m_data.size()), SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                sqlite3_finalize(stmt);
                return cpp_dbc::unexpected(DBException("ZIDTSWSKRVIY", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }

            // Bind the rowid parameter (parameter 2)
            result = sqlite3_bind_text(stmt, 2, m_rowId.c_str(), -1, SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                sqlite3_finalize(stmt);
                return cpp_dbc::unexpected(DBException("5L7M3P9K8TJV", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }

            // Execute the statement
            result = sqlite3_step(stmt);
            if (result != SQLITE_DONE)
            {
                sqlite3_finalize(stmt);
                return cpp_dbc::unexpected(DBException("LPJVG75T44E5", "Failed to save BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
            }

            // Finalize the statement
            sqlite3_finalize(stmt);
            return {};
        }

        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override
        {
            auto r = MemoryBlob::free(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            m_loaded = false;
            return {};
        }
    };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
