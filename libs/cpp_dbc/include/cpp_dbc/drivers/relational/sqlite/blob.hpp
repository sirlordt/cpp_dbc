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
        private:
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
            sqlite3 *getSQLiteConnection() const
            {
                auto conn = m_db.lock();
                if (!conn)
                {
                    throw DBException("SQLITE_BLOB_CONN_CLOSED", "SQLite connection has been closed", system_utils::captureCallStack());
                }
                return conn.get();
            }

            /**
             * @brief Validate SQL identifier to prevent injection attacks
             * @param identifier The table or column name to validate
             * @throws DBException if the identifier contains invalid characters
             *
             * Only allows alphanumeric characters and underscores to prevent SQL injection.
             * This follows the cpp_dbc project convention for schema name validation.
             */
            static void validateIdentifier(const std::string &identifier)
            {
                if (identifier.empty())
                {
                    throw DBException("9Z4K7W2P5NXF", "Empty SQL identifier not allowed", system_utils::captureCallStack());
                }

                for (char c : identifier)
                {
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                    {
                        throw DBException("3H8T6Q9R2VCJ",
                            "Invalid character in SQL identifier '" + identifier + "': only alphanumeric and underscore allowed",
                            system_utils::captureCallStack());
                    }
                }
            }

        public:
            /**
             * @brief Check if the database connection is still valid
             * @return true if connection is valid, false if it has been closed
             */
            bool isConnectionValid() const
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

            /**
             * @brief Load the BLOB data from the database if not already loaded
             * @throws DBException if the connection is closed or loading fails
             */
            void ensureLoaded() const
            {
                if (m_loaded || m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
                    return;

                // Validate identifiers to prevent SQL injection
                validateIdentifier(m_tableName);
                validateIdentifier(m_columnName);

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *db = getSQLiteConnection();

                // Construct a query with parameterized rowid to prevent SQL injection
                std::string query = "SELECT " + m_columnName + " FROM " + m_tableName + " WHERE rowid = ?";

                // Prepare the statement
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    throw DBException("4AE05442DB70", "Failed to prepare statement for BLOB loading: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Bind the rowid parameter
                result = sqlite3_bind_text(stmt, 1, m_rowId.c_str(), -1, SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    sqlite3_finalize(stmt);
                    throw DBException("2F9K4N8V7QXW", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Execute the statement
                result = sqlite3_step(stmt);
                if (result == SQLITE_ROW)
                {
                    // Get the BLOB data
                    const void *blobData = sqlite3_column_blob(stmt, 0);
                    int blobSize = sqlite3_column_bytes(stmt, 0);

                    // Copy the BLOB data
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
                    throw DBException("D281D99D6FAC", "Failed to fetch BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Finalize the statement
                sqlite3_finalize(stmt);
                m_loaded = true;
            }

            // Override methods that need to ensure the BLOB is loaded
            size_t length() const override
            {
                ensureLoaded();
                return MemoryBlob::length();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                ensureLoaded();
                return MemoryBlob::getBytes(pos, length);
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                ensureLoaded();
                return MemoryBlob::getBinaryStream();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                ensureLoaded();
                return MemoryBlob::setBinaryStream(pos);
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes);
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes, length);
            }

            void truncate(size_t len) override
            {
                ensureLoaded();
                MemoryBlob::truncate(len);
            }

            /**
             * @brief Save the BLOB data back to the database
             * @throws DBException if the connection is closed or saving fails
             */
            void save()
            {
                if (m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
                    return; // Nothing to save

                // Ensure BLOB data is loaded before saving to prevent overwriting with empty data
                ensureLoaded();

                // Validate identifiers to prevent SQL injection
                validateIdentifier(m_tableName);
                validateIdentifier(m_columnName);

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *db = getSQLiteConnection();

                // Construct a query with parameterized rowid to prevent SQL injection
                std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE rowid = ?";

                // Prepare the statement
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    throw DBException("78BBDB81BED9", "Failed to prepare statement for BLOB saving: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Bind the BLOB data (parameter 1)
                result = sqlite3_bind_blob(stmt, 1, m_data.data(), static_cast<int>(m_data.size()), SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    sqlite3_finalize(stmt);
                    throw DBException("6C9619BE36A2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Bind the rowid parameter (parameter 2)
                result = sqlite3_bind_text(stmt, 2, m_rowId.c_str(), -1, SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    sqlite3_finalize(stmt);
                    throw DBException("5L7M3P9K8TJV", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Execute the statement
                result = sqlite3_step(stmt);
                if (result != SQLITE_DONE)
                {
                    sqlite3_finalize(stmt);
                    throw DBException("8DB1A784821C", "Failed to save BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Finalize the statement
                sqlite3_finalize(stmt);
            }

            void free() override
            {
                MemoryBlob::free();
                m_loaded = false;
            }
        };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
