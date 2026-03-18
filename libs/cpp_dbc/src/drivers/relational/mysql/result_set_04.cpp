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

 @file result_set_04.cpp
 @brief MySQL database driver implementation - MySQLDBResultSet nothrow methods (part 3 - blob/binary)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

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

    cpp_dbc::expected<size_t, DBException> MySQLDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_fieldCount;
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> MySQLDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("LB0WW5CDQLVI", "Invalid column index for getBlob", system_utils::captureCallStack()));
        }

        // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            // Return an empty blob with no connection (data is already loaded)
            auto blobResult = MySQL::MySQLBlob::create(std::nothrow, std::weak_ptr<MySQLDBConnection>{});
            if (!blobResult.has_value())
            {
                return cpp_dbc::unexpected(blobResult.error());
            }
            return std::shared_ptr<Blob>(blobResult.value());
        }

        // Get the length of the BLOB data
        const unsigned long *lengths = m_materialized
            ? m_currentLengths.data()
            : mysql_fetch_lengths(m_result.get());
        if (!lengths)
        {
            return cpp_dbc::unexpected(DBException("8P8KDB7HDBNE", "Failed to get BLOB data length", system_utils::captureCallStack()));
        }

        // Create a new BLOB object with the data
        // Note: We pass an empty shared_ptr because the data is already loaded
        // and the blob won't need to query the database
        std::vector<uint8_t> data;
        if (lengths[idx] > 0)
        {
            data.resize(lengths[idx]);
            std::memcpy(data.data(), m_currentRow[idx], lengths[idx]);
        }

        auto blobResult = MySQL::MySQLBlob::create(std::nothrow, m_connection, data);
        if (!blobResult.has_value())
        {
            return cpp_dbc::unexpected(blobResult.error());
        }
        return std::shared_ptr<Blob>(blobResult.value());
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> MySQLDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("MDWX3TY3ZNN4", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> MySQLDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("BARSF3FXQ52P", "Invalid column index for getBinaryStream", system_utils::captureCallStack()));
        }

        // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            // Return an empty stream
            auto streamResult = MySQL::MySQLInputStream::create(std::nothrow, "", 0);
            if (!streamResult.has_value())
            {
                return cpp_dbc::unexpected(streamResult.error());
            }
            return std::shared_ptr<InputStream>(streamResult.value());
        }

        // Get the length of the BLOB data
        const unsigned long *lengths = m_materialized
            ? m_currentLengths.data()
            : mysql_fetch_lengths(m_result.get());
        if (!lengths)
        {
            return cpp_dbc::unexpected(DBException("MKQTE8BTZAJY", "Failed to get BLOB data length", system_utils::captureCallStack()));
        }

        // Create a new input stream with the data
        auto streamResult = MySQL::MySQLInputStream::create(std::nothrow, m_currentRow[idx], lengths[idx]);
        if (!streamResult.has_value())
        {
            return cpp_dbc::unexpected(streamResult.error());
        }
        return std::shared_ptr<InputStream>(streamResult.value());
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> MySQLDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("0WMIY8KYPYQA", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> MySQLDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("3XVHIXH3HKXS", "Invalid column index for getBytes", system_utils::captureCallStack()));
        }

        // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            return std::vector<uint8_t>();
        }

        // Get the length of the BLOB data
        const unsigned long *lengths = m_materialized
            ? m_currentLengths.data()
            : mysql_fetch_lengths(m_result.get());
        if (!lengths)
        {
            return cpp_dbc::unexpected(DBException("9KU99I1V16QW", "Failed to get BLOB data length", system_utils::captureCallStack()));
        }

        // Create a vector with the data
        std::vector<uint8_t> data;
        if (lengths[idx] > 0)
        {
            data.resize(lengths[idx]);
            std::memcpy(data.data(), m_currentRow[idx], lengths[idx]);
        }

        return data;
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> MySQLDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("5DB9EL8OWXMD", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
