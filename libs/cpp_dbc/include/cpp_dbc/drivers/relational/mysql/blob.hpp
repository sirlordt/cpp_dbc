#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>
#include <memory>
#include <cstring>

namespace cpp_dbc::MySQL
{
        /**
         * @brief MySQL implementation of Blob using smart pointers for memory safety
         *
         * This class uses std::weak_ptr to safely reference the MySQL connection handle,
         * preventing dangling pointer issues if the connection is closed while the
         * blob is still in use. All operations that require database access will
         * check if the connection is still valid before proceeding.
         */
        class MySQLBlob : public MemoryBlob
        {
        private:
            /**
             * @brief Weak reference to the MySQL connection handle
             *
             * Using weak_ptr allows us to detect when the connection has been closed
             * and avoid use-after-free errors. The connection owns the MYSQL handle,
             * so we must ensure it's still valid before using it.
             */
            std::weak_ptr<MYSQL> m_mysql;
            std::string m_tableName;
            std::string m_columnName;
            std::string m_whereClause;
            mutable bool m_loaded{false};

            /**
             * @brief Get a locked pointer to the MySQL handle
             * @return Raw pointer to the MYSQL handle
             * @throws DBException if the connection has been closed
             */
            cpp_dbc::expected<MYSQL *, DBException> getMySQLHandle(std::nothrow_t) const noexcept
            {
                auto mysql = m_mysql.lock();
                if (!mysql)
                {
                    return cpp_dbc::unexpected(DBException("SWUSJLDWXH53", "Connection has been closed", system_utils::captureCallStack()));
                }
                return mysql.get();
            }

            MYSQL *getMySQLHandle() const
            {
                auto r = getMySQLHandle(std::nothrow);
                if (!r.has_value())
                {
                    throw r.error();
                }
                return r.value();
            }

        public:
            /**
             * @brief Constructor for creating a new BLOB
             * @param mysql Shared pointer to the MySQL connection handle
             */
            explicit MySQLBlob(std::shared_ptr<MYSQL> mysql)
                : m_mysql(mysql), m_loaded(true) {}

            /**
             * @brief Constructor for loading an existing BLOB
             * @param mysql Shared pointer to the MySQL connection handle
             * @param tableName The table containing the BLOB
             * @param columnName The column containing the BLOB
             * @param whereClause The WHERE clause to identify the row
             */
            MySQLBlob(std::shared_ptr<MYSQL> mysql, const std::string &tableName,
                      const std::string &columnName, const std::string &whereClause)
                : m_mysql(mysql), m_tableName(tableName), m_columnName(columnName),
                  m_whereClause(whereClause) {}

            /**
             * @brief Constructor for creating a BLOB from existing data
             * @param mysql Shared pointer to the MySQL connection handle
             * @param initialData The initial data for the BLOB
             */
            MySQLBlob(std::shared_ptr<MYSQL> mysql, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_mysql(mysql), m_loaded(true) {}

            static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<MYSQL> mysql) noexcept
            {
                return std::make_shared<MySQLBlob>(mysql);
            }

            static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<MYSQL> mysql,
                const std::string &tableName, const std::string &columnName, const std::string &whereClause) noexcept
            {
                return std::make_shared<MySQLBlob>(mysql, tableName, columnName, whereClause);
            }

            static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<MYSQL> mysql,
                const std::vector<uint8_t> &initialData) noexcept
            {
                return std::make_shared<MySQLBlob>(mysql, initialData);
            }

            static std::shared_ptr<MySQLBlob> create(std::shared_ptr<MYSQL> mysql)
            {
                auto r = create(std::nothrow, mysql);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<MySQLBlob> create(std::shared_ptr<MYSQL> mysql,
                const std::string &tableName, const std::string &columnName, const std::string &whereClause)
            {
                auto r = create(std::nothrow, mysql, tableName, columnName, whereClause);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<MySQLBlob> create(std::shared_ptr<MYSQL> mysql,
                const std::vector<uint8_t> &initialData)
            {
                auto r = create(std::nothrow, mysql, initialData);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const MySQLBlob &other) noexcept
            {
                if (this == &other) { return {}; }
                auto r = MemoryBlob::copyFrom(std::nothrow, other);
                if (!r.has_value()) { return r; }
                m_mysql = other.m_mysql;
                m_tableName = other.m_tableName;
                m_columnName = other.m_columnName;
                m_whereClause = other.m_whereClause;
                m_loaded = other.m_loaded;
                return {};
            }

            void copyFrom(const MySQLBlob &other)
            {
                auto r = copyFrom(std::nothrow, other);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Check if the connection is still valid
             * @return true if the connection is still valid
             */
            bool isConnectionValid() const
            {
                return !m_mysql.expired();
            }

            /**
             * @brief Load the BLOB data from the database if not already loaded
             *
             * This method safely accesses the connection through the weak_ptr,
             * ensuring the connection is still valid before attempting to read.
             */
            cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept
            {
                if (m_loaded)
                {
                    return {};
                }
                auto mysqlResult = getMySQLHandle(std::nothrow);
                if (!mysqlResult.has_value())
                {
                    return cpp_dbc::unexpected(mysqlResult.error());
                }
                MYSQL *mysql = mysqlResult.value();

                std::string query = "SELECT " + m_columnName + " FROM " + m_tableName;
                if (!m_whereClause.empty())
                {
                    query += " WHERE " + m_whereClause;
                }
                if (mysql_query(mysql, query.c_str()) != 0)
                {
                    return cpp_dbc::unexpected(DBException("6VDNWQ8STSAG", "Failed to fetch BLOB data: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
                }
                MYSQL_RES *result = mysql_store_result(mysql);
                if (!result)
                {
                    return cpp_dbc::unexpected(DBException("476QLR8BRXQK", "Failed to get result set for BLOB data: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
                }
                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    return cpp_dbc::unexpected(DBException("QQ7NT0640BWW", "No data found for BLOB", system_utils::captureCallStack()));
                }
                const unsigned long *lengths = mysql_fetch_lengths(result);
                if (!lengths)
                {
                    mysql_free_result(result);
                    return cpp_dbc::unexpected(DBException("VKZ2WW134M1Q", "Failed to get BLOB data length", system_utils::captureCallStack()));
                }
                if (row[0] && lengths[0] > 0)
                {
                    m_data.resize(lengths[0]);
                    std::memcpy(m_data.data(), row[0], lengths[0]);
                }
                else
                {
                    m_data.clear();
                }
                mysql_free_result(result);
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
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                auto r = getBytes(std::nothrow, pos, length);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                auto r = getBinaryStream(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                auto r = setBinaryStream(std::nothrow, pos);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                auto r = setBytes(std::nothrow, pos, bytes);
                if (!r.has_value()) { throw r.error(); }
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                auto r = setBytes(std::nothrow, pos, bytes, length);
                if (!r.has_value()) { throw r.error(); }
            }

            void truncate(size_t len) override
            {
                auto r = truncate(std::nothrow, len);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Save the BLOB data back to the database
             *
             * This method safely accesses the connection through the weak_ptr,
             * ensuring the connection is still valid before attempting to write.
             *
             * @throws DBException if the connection has been closed or if writing fails
             */
            cpp_dbc::expected<void, DBException> save(std::nothrow_t) noexcept
            {
                if (m_tableName.empty() || m_columnName.empty() || m_whereClause.empty())
                {
                    return {}; // Nothing to save
                }
                auto mysqlResult = getMySQLHandle(std::nothrow);
                if (!mysqlResult.has_value())
                {
                    return cpp_dbc::unexpected(mysqlResult.error());
                }
                MYSQL *mysql = mysqlResult.value();

                std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE " + m_whereClause;
                MYSQL_STMT *stmt = mysql_stmt_init(mysql);
                if (!stmt)
                {
                    return cpp_dbc::unexpected(DBException("F5W35AZHZKK7", "Failed to initialize statement for BLOB update: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
                }
                if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    return cpp_dbc::unexpected(DBException("1M7I6MJ8UHON", "Failed to prepare statement for BLOB update: " + error, system_utils::captureCallStack()));
                }
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
                    return cpp_dbc::unexpected(DBException("K4PXEPQPAR99", "Failed to bind BLOB data: " + error, system_utils::captureCallStack()));
                }
                if (mysql_stmt_execute(stmt) != 0)
                {
                    std::string error = mysql_stmt_error(stmt);
                    mysql_stmt_close(stmt);
                    return cpp_dbc::unexpected(DBException("SMT5X3RWBNV7", "Failed to update BLOB data: " + error, system_utils::captureCallStack()));
                }
                mysql_stmt_close(stmt);
                return {};
            }

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
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::length(std::nothrow);
            }

            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::getBytes(std::nothrow, pos, length);
            }

            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::getBinaryStream(std::nothrow);
            }

            cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBinaryStream(std::nothrow, pos);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
            }

            cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::truncate(std::nothrow, len);
            }

            cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override
            {
                auto r = MemoryBlob::free(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                m_loaded = false;
                return {};
            }
        };

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
