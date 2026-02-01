/*

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

 @file prepared_statement_03.cpp
 @brief MySQL database driver implementation - MySQLDBPreparedStatement nothrow methods (part 2 - blob/binary and execute)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("J7K8L9M0N1O2", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult.has_value())
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            // Read all data from the stream
            std::vector<uint8_t> data;
            std::array<uint8_t, 4096> buffer{};
            int bytesRead = 0;
            while ((bytesRead = x->read(buffer.data(), buffer.size())) > 0)
            {
                data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D9E5F1A7B4C0",
                                                   std::string("setBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D9E5F1A7B4C1",
                                                   "setBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("P3Q4R5S6T7U8", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult.has_value())
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            // Read up to 'length' bytes from the stream
            std::vector<uint8_t> data;
            data.reserve(length);
            std::array<uint8_t, 4096> buffer{};
            size_t totalBytesRead = 0;
            int bytesRead = 0;
            while (totalBytesRead < length)
            {
                bytesRead = x->read(buffer.data(), std::min(buffer.size(), length - totalBytesRead));
                if (bytesRead <= 0)
                {
                    break;
                }
                data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
                totalBytesRead += bytesRead;
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E0F6A2B8C5D1",
                                                   std::string("setBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E0F6A2B8C5D2",
                                                   "setBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("B5C6D7E8F9G0", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult.has_value())
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx].resize(length);
            std::memcpy(m_blobValues[idx].data(), x, length);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F1A7B3C9D6E2",
                                                   std::string("setBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F1A7B3C9D6E3",
                                                   "setBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> MySQLDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_stmt)
            {
                return cpp_dbc::unexpected(DBException("3G4H5I6J7K8L", "Statement is applied", system_utils::captureCallStack()));
            }

            // Get the MySQL connection safely (throws if connection is closed)
            MYSQL *mysqlPtr = getMySQLConnection();

            // Reconstruct the query with bound parameters to avoid "Commands out of sync" issue
            std::string finalQuery = m_sql;

            // Replace each '?' with the corresponding parameter value
            for (const auto &paramValue : m_parameterValues)
            {
                size_t pos = finalQuery.find('?');
                if (pos != std::string::npos)
                {
                    finalQuery.replace(pos, 1, paramValue);
                }
            }

            // Execute the reconstructed query using the regular connection interface
            if (mysql_query(mysqlPtr, finalQuery.c_str()) != 0)
            {
                return cpp_dbc::unexpected(DBException("9M0N1O2P3Q4R", std::string("Query failed: ") + mysql_error(mysqlPtr), system_utils::captureCallStack()));
            }

            MYSQL_RES *result = mysql_store_result(mysqlPtr);
            if (!result && mysql_field_count(mysqlPtr) > 0)
            {
                return cpp_dbc::unexpected(DBException("H1I2J3K4L5M6", std::string("Failed to get result set: ") + mysql_error(mysqlPtr), system_utils::captureCallStack()));
            }

            auto resultSet = std::make_shared<MySQLDBResultSet>(result);

            // Close the statement after execution (single-use)
            // This is safe because mysql_store_result() copies all data to client memory
            // close();

            return std::shared_ptr<RelationalDBResultSet>(resultSet);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D4E0F6A2B9CC",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D4E0F6A2B9CD",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_stmt)
            {
                return cpp_dbc::unexpected(DBException("255F5A0C6008", "Statement is applied", system_utils::captureCallStack()));
            }

            // Bind parameters
            if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
            {
                return cpp_dbc::unexpected(DBException("9B7E537EB656", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
            }

            // Execute the query
            if (mysql_stmt_execute(m_stmt.get()) != 0)
            {
                return cpp_dbc::unexpected(DBException("547F7466347C", std::string("Failed to execute update: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
            }

            return mysql_stmt_affected_rows(m_stmt.get());
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F6A2B8C4D1EE",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F6A2B8C4D1EF",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_stmt)
            {
                return cpp_dbc::unexpected(DBException("5S6T7U8V9W0X", "Statement is not initialized", system_utils::captureCallStack()));
            }

            // Bind parameters
            if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
            {
                return cpp_dbc::unexpected(DBException("1Y2Z3A4B5C6D", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
            }

            // Execute the query
            if (mysql_stmt_execute(m_stmt.get()) != 0)
            {
                return cpp_dbc::unexpected(DBException("7E8F9G0H1I2J", std::string("Failed to execute statement: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
            }

            // Return whether there's a result set
            return mysql_stmt_field_count(m_stmt.get()) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B8C4D0E6F3B0",
                                                   std::string("execute failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B8C4D0E6F3B1",
                                                   "execute failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (m_stmt)
            {
                m_stmt.reset(); // Smart pointer will call mysql_stmt_close via deleter
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4B9",
                                                   std::string("close failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4BA",
                                                   "close failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
