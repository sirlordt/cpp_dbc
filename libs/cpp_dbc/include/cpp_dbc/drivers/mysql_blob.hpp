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

 @file mysql_blob.hpp
 @brief Tests for MySQL database operations

*/

#ifndef CPP_DBC_MYSQL_BLOB_HPP
#define CPP_DBC_MYSQL_BLOB_HPP

#include "../cpp_dbc.hpp"
#include "../blob.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>

namespace cpp_dbc
{
    namespace MySQL
    {
        // MySQL implementation of InputStream
        class MySQLInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position;

        public:
            MySQLInputStream(const char *buffer, size_t length)
                : m_data(buffer, buffer + length), m_position(0) {}

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

        // MySQL implementation of Blob
        class MySQLBlob : public MemoryBlob
        {
        private:
            MYSQL *m_mysql;
            std::string m_tableName;
            std::string m_columnName;
            std::string m_whereClause;
            bool m_loaded;

        public:
            // Constructor for creating a new BLOB
            MySQLBlob(MYSQL *mysql)
                : m_mysql(mysql), m_loaded(true) {}

            // Constructor for loading an existing BLOB
            MySQLBlob(MYSQL *mysql, const std::string &tableName,
                      const std::string &columnName, const std::string &whereClause)
                : m_mysql(mysql), m_tableName(tableName), m_columnName(columnName),
                  m_whereClause(whereClause), m_loaded(false) {}

            // Constructor for creating a BLOB from existing data
            MySQLBlob(MYSQL *mysql, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_mysql(mysql), m_loaded(true) {}

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded()
            {
                if (m_loaded)
                    return;

                // Construct a query to fetch the BLOB data
                std::string query = "SELECT " + m_columnName + " FROM " + m_tableName;
                if (!m_whereClause.empty())
                    query += " WHERE " + m_whereClause;

                // Execute the query
                if (mysql_query(m_mysql, query.c_str()) != 0)
                {
                    throw DBException("M1Y2S3Q4L5B", "Failed to fetch BLOB data: " + std::string(mysql_error(m_mysql)), system_utils::captureCallStack());
                }

                // Get the result
                MYSQL_RES *result = mysql_store_result(m_mysql);
                if (!result)
                {
                    throw DBException("L6O7B8R9E0S", "Failed to get result set for BLOB data: " + std::string(mysql_error(m_mysql)), system_utils::captureCallStack());
                }

                // Get the row
                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    throw DBException("U1L2T3N4O5D", "No data found for BLOB", system_utils::captureCallStack());
                }

                // Get the BLOB data
                unsigned long *lengths = mysql_fetch_lengths(result);
                if (!lengths)
                {
                    mysql_free_result(result);
                    throw DBException("A6T7A8L9E0N", "Failed to get BLOB data length", system_utils::captureCallStack());
                }

                // Copy the BLOB data
                if (row[0] && lengths[0] > 0)
                {
                    m_data.resize(lengths[0]);
                    std::memcpy(m_data.data(), row[0], lengths[0]);
                }
                else
                {
                    m_data.clear();
                }

                // Free the result
                mysql_free_result(result);
                m_loaded = true;
            }

            // Override methods that need to ensure the BLOB is loaded
            size_t length() const override
            {
                const_cast<MySQLBlob *>(this)->ensureLoaded();
                return MemoryBlob::length();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                const_cast<MySQLBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBytes(pos, length);
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                const_cast<MySQLBlob *>(this)->ensureLoaded();
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

            // Save the BLOB data back to the database
            void save()
            {
                if (m_tableName.empty() || m_columnName.empty() || m_whereClause.empty())
                    return; // Nothing to save

                // Prepare a statement to update the BLOB data
                std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE " + m_whereClause;

                MYSQL_STMT *stmt = mysql_stmt_init(m_mysql);
                if (!stmt)
                {
                    throw DBException("G1T2H3I4N5I", "Failed to initialize statement for BLOB update: " + std::string(mysql_error(m_mysql)), system_utils::captureCallStack());
                }

                if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("T6S7T8M9T0P", "Failed to prepare statement for BLOB update: " + error, system_utils::captureCallStack());
                }

                // Bind the BLOB data
                MYSQL_BIND bind;
                memset(&bind, 0, sizeof(bind));
                bind.buffer_type = MYSQL_TYPE_BLOB;
                bind.buffer = m_data.data();
                bind.buffer_length = m_data.size();
                bind.length_value = m_data.size();

                if (mysql_stmt_bind_param(stmt, &bind) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("R1E2P3A4R5E", "Failed to bind BLOB data: " + error, system_utils::captureCallStack());
                }

                // Execute the statement
                if (mysql_stmt_execute(stmt) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("B6L7O8B9U0P", "Failed to update BLOB data: " + error, system_utils::captureCallStack());
                }

                // Clean up
                mysql_stmt_close(stmt);
            }

            void free() override
            {
                MemoryBlob::free();
                m_loaded = false;
            }
        };

    } // namespace MySQL
} // namespace cpp_dbc

#endif // USE_MYSQL

#endif // CPP_DBC_MYSQL_BLOB_HPP