/**

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file sqlite_blob.hpp
 @brief Tests for SQLite database operations

*/

#ifndef CPP_DBC_SQLITE_BLOB_HPP
#define CPP_DBC_SQLITE_BLOB_HPP

#include "../../cpp_dbc.hpp"
#include "../../blob.hpp"

#if USE_SQLITE
#include <sqlite3.h>

namespace cpp_dbc::SQLite
{
        // SQLite implementation of InputStream
        class SQLiteInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            SQLiteInputStream(const void *buffer, size_t length)
                : m_data(static_cast<const uint8_t *>(buffer), static_cast<const uint8_t *>(buffer) + length), m_position(0) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (m_position >= m_data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min(length, m_data.size() - m_position);
                std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
                m_position += bytesToRead;
                return static_cast<int>(bytesToRead);
            }

            void skip(size_t n) override
            {
                m_position = std::min(m_position + n, m_data.size());
            }

            void close() override
            {
                // Nothing to do for memory stream
            }
        };

        // SQLite implementation of Blob
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

        public:
            /**
             * @brief Check if the database connection is still valid
             * @return true if connection is valid, false if it has been closed
             */
            bool isConnectionValid() const
            {
                return !m_db.expired();
            }

            // Constructor for creating a new BLOB (empty shared_ptr for data-only blobs)
            SQLiteBlob(std::shared_ptr<sqlite3> db)
                : m_db(db), m_loaded(true) {}

            // Constructor for loading an existing BLOB
            SQLiteBlob(std::shared_ptr<sqlite3> db, const std::string &tableName,
                       const std::string &columnName, const std::string &rowId)
                : m_db(db), m_tableName(tableName), m_columnName(columnName),
                  m_rowId(rowId), m_loaded(false) {}

            // Constructor for creating a BLOB from existing data
            SQLiteBlob(std::shared_ptr<sqlite3> db, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_db(db), m_loaded(true) {}

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded() const
            {
                if (m_loaded || m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
                    return;

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *db = getSQLiteConnection();

                // Construct a query to fetch the BLOB data
                std::string query = "SELECT " + m_columnName + " FROM " + m_tableName + " WHERE rowid = " + m_rowId;

                // Prepare the statement
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    throw DBException("4AE05442DB70", "Failed to prepare statement for BLOB loading: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
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

            // Save the BLOB data to the database
            void save()
            {
                if (m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
                    return; // Nothing to save

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *db = getSQLiteConnection();

                // Construct a query to update the BLOB data
                std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE rowid = " + m_rowId;

                // Prepare the statement
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    throw DBException("78BBDB81BED9", "Failed to prepare statement for BLOB saving: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Bind the BLOB data
                result = sqlite3_bind_blob(stmt, 1, m_data.data(), static_cast<int>(m_data.size()), SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    sqlite3_finalize(stmt);
                    throw DBException("6C9619BE36A2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
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

#endif // CPP_DBC_SQLITE_BLOB_HPP