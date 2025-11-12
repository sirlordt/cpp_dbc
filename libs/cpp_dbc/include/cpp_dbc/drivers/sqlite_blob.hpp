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

#include "../cpp_dbc.hpp"
#include "../blob.hpp"

#if USE_SQLITE
#include <sqlite3.h>

namespace cpp_dbc
{
    namespace SQLite
    {
        // SQLite implementation of InputStream
        class SQLiteInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> data;
            size_t position;

        public:
            SQLiteInputStream(const void *buffer, size_t length)
                : data(static_cast<const uint8_t *>(buffer), static_cast<const uint8_t *>(buffer) + length), position(0) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (position >= data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min(length, data.size() - position);
                std::memcpy(buffer, data.data() + position, bytesToRead);
                position += bytesToRead;
                return static_cast<int>(bytesToRead);
            }

            void skip(size_t n) override
            {
                position = std::min(position + n, data.size());
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
            sqlite3 *db;
            std::string tableName;
            std::string columnName;
            std::string rowId;
            bool loaded;

        public:
            // Constructor for creating a new BLOB
            SQLiteBlob(sqlite3 *db)
                : db(db), loaded(true) {}

            // Constructor for loading an existing BLOB
            SQLiteBlob(sqlite3 *db, const std::string &tableName,
                       const std::string &columnName, const std::string &rowId)
                : db(db), tableName(tableName), columnName(columnName),
                  rowId(rowId), loaded(false) {}

            // Constructor for creating a BLOB from existing data
            SQLiteBlob(sqlite3 *db, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), db(db), loaded(true) {}

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded()
            {
                if (loaded || tableName.empty() || columnName.empty() || rowId.empty())
                    return;

                // Construct a query to fetch the BLOB data
                std::string query = "SELECT " + columnName + " FROM " + tableName + " WHERE rowid = " + rowId;

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
                        data.resize(blobSize);
                        std::memcpy(data.data(), blobData, blobSize);
                    }
                    else
                    {
                        data.clear();
                    }
                }
                else
                {
                    sqlite3_finalize(stmt);
                    throw DBException("D281D99D6FAC", "Failed to fetch BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Finalize the statement
                sqlite3_finalize(stmt);
                loaded = true;
            }

            // Override methods that need to ensure the BLOB is loaded
            size_t length() const override
            {
                const_cast<SQLiteBlob *>(this)->ensureLoaded();
                return MemoryBlob::length();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                const_cast<SQLiteBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBytes(pos, length);
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                const_cast<SQLiteBlob *>(this)->ensureLoaded();
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
                if (tableName.empty() || columnName.empty() || rowId.empty())
                    return; // Nothing to save

                // Construct a query to update the BLOB data
                std::string query = "UPDATE " + tableName + " SET " + columnName + " = ? WHERE rowid = " + rowId;

                // Prepare the statement
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    throw DBException("78BBDB81BED9", "Failed to prepare statement for BLOB saving: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack());
                }

                // Bind the BLOB data
                result = sqlite3_bind_blob(stmt, 1, data.data(), data.size(), SQLITE_STATIC);
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
                loaded = false;
            }
        };

    } // namespace SQLite
} // namespace cpp_dbc

#endif // USE_SQLITE

#endif // CPP_DBC_SQLITE_BLOB_HPP