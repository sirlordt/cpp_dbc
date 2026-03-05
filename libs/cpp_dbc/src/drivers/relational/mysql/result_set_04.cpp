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
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_fieldCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F8A4B0C6D3EA",
                                                   std::string("getColumnCount failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F8A4B0C6D3EB",
                                                   "getColumnCount failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> MySQLDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
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
                return std::shared_ptr<Blob>(std::make_shared<MySQL::MySQLBlob>(std::shared_ptr<MYSQL>()));
            }

            // Get the length of the BLOB data
            const unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                return cpp_dbc::unexpected(DBException("Q0Z1A2B3C4D5", "Failed to get BLOB data length", system_utils::captureCallStack()));
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

            return std::shared_ptr<Blob>(std::make_shared<MySQL::MySQLBlob>(std::shared_ptr<MYSQL>(), data));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("MY7G8H9I0J1K",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MY1A2B3C4D5E",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> MySQLDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("Q1Z2A3B4C5D6", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A9B5C1D7E4F7",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A9B5C1D7E4F8",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> MySQLDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("Q2Z3A4B5C6D7", "Invalid column index for getBinaryStream", system_utils::captureCallStack()));
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                // Return an empty stream
                return std::shared_ptr<InputStream>(std::make_shared<MySQL::MySQLInputStream>("", 0));
            }

            // Get the length of the BLOB data
            const unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                return cpp_dbc::unexpected(DBException("Q3Z4A5B6C7D8", "Failed to get BLOB data length", system_utils::captureCallStack()));
            }

            // Create a new input stream with the data
            return std::shared_ptr<InputStream>(std::make_shared<MySQL::MySQLInputStream>(m_currentRow[idx], lengths[idx]));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E8F4A0B6C3D9",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E8F4A0B6C3DA",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> MySQLDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("Q4Z5A6B7C8D9", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B0C6D2E8F5A8",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B0C6D2E8F5A9",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> MySQLDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("Q5Z6A7B8C9D0", "Invalid column index for getBytes", system_utils::captureCallStack()));
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return std::vector<uint8_t>();
            }

            // Get the length of the BLOB data
            const unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                return cpp_dbc::unexpected(DBException("Q6Z7A8B9C0D1", "Failed to get BLOB data length", system_utils::captureCallStack()));
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F9A5B1C7D4E0",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F9A5B1C7D4E1",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> MySQLDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("X5Y6Z7A8B9C0", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C1D7E3F9A6B9",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C1D7E3F9A6BA",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
