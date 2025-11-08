// mysql_blob.hpp
// MySQL implementation of BLOB support for cpp_dbc

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
            const std::vector<uint8_t> data;
            size_t position;

        public:
            MySQLInputStream(const char *buffer, size_t length)
                : data(buffer, buffer + length), position(0) {}

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

        // MySQL implementation of Blob
        class MySQLBlob : public MemoryBlob
        {
        private:
            MYSQL *mysql;
            std::string tableName;
            std::string columnName;
            std::string whereClause;
            bool loaded;

        public:
            // Constructor for creating a new BLOB
            MySQLBlob(MYSQL *mysql)
                : mysql(mysql), loaded(true) {}

            // Constructor for loading an existing BLOB
            MySQLBlob(MYSQL *mysql, const std::string &tableName,
                      const std::string &columnName, const std::string &whereClause)
                : mysql(mysql), tableName(tableName), columnName(columnName),
                  whereClause(whereClause), loaded(false) {}

            // Constructor for creating a BLOB from existing data
            MySQLBlob(MYSQL *mysql, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), mysql(mysql), loaded(true) {}

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded()
            {
                if (loaded)
                    return;

                // Construct a query to fetch the BLOB data
                std::string query = "SELECT " + columnName + " FROM " + tableName;
                if (!whereClause.empty())
                    query += " WHERE " + whereClause;

                // Execute the query
                if (mysql_query(mysql, query.c_str()) != 0)
                {
                    throw DBException("Failed to fetch BLOB data: " +
                                      std::string(mysql_error(mysql)));
                }

                // Get the result
                MYSQL_RES *result = mysql_store_result(mysql);
                if (!result)
                {
                    throw DBException("Failed to get result set for BLOB data: " +
                                      std::string(mysql_error(mysql)));
                }

                // Get the row
                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    throw DBException("No data found for BLOB");
                }

                // Get the BLOB data
                unsigned long *lengths = mysql_fetch_lengths(result);
                if (!lengths)
                {
                    mysql_free_result(result);
                    throw DBException("Failed to get BLOB data length");
                }

                // Copy the BLOB data
                if (row[0] && lengths[0] > 0)
                {
                    data.resize(lengths[0]);
                    std::memcpy(data.data(), row[0], lengths[0]);
                }
                else
                {
                    data.clear();
                }

                // Free the result
                mysql_free_result(result);
                loaded = true;
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
                if (tableName.empty() || columnName.empty() || whereClause.empty())
                    return; // Nothing to save

                // Prepare a statement to update the BLOB data
                std::string query = "UPDATE " + tableName + " SET " + columnName + " = ? WHERE " + whereClause;

                MYSQL_STMT *stmt = mysql_stmt_init(mysql);
                if (!stmt)
                {
                    throw DBException("Failed to initialize statement for BLOB update: " +
                                      std::string(mysql_error(mysql)));
                }

                if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("Failed to prepare statement for BLOB update: " + error);
                }

                // Bind the BLOB data
                MYSQL_BIND bind;
                memset(&bind, 0, sizeof(bind));
                bind.buffer_type = MYSQL_TYPE_BLOB;
                bind.buffer = data.data();
                bind.buffer_length = data.size();
                bind.length_value = data.size();

                if (mysql_stmt_bind_param(stmt, &bind) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("Failed to bind BLOB data: " + error);
                }

                // Execute the statement
                if (mysql_stmt_execute(stmt) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    throw DBException("Failed to update BLOB data: " + error);
                }

                // Clean up
                mysql_stmt_close(stmt);
            }

            void free() override
            {
                MemoryBlob::free();
                loaded = false;
            }
        };

    } // namespace MySQL
} // namespace cpp_dbc

#endif // USE_MYSQL

#endif // CPP_DBC_MYSQL_BLOB_HPP