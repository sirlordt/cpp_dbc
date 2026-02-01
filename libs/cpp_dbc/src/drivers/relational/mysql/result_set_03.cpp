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

 @file result_set_03.cpp
 @brief MySQL database driver implementation - MySQLDBResultSet nothrow methods (part 2 - blob/binary)

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

    cpp_dbc::expected<double, DBException> MySQLDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("71685784D1EB", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getDouble(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E1F7A3B9C6D9",
                                                   std::string("getDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E1F7A3B9C6DA",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("45B8E019C425", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getString(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7EA",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7EB",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("94A1D34DC156", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBoolean(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8FB",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8FC",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("DA3E45676022", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0BD",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0BE",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> MySQLDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_columnNames;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2D9",
                                                   std::string("getColumnNames failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2DA",
                                                   "getColumnNames failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

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

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("B7C8D9E0F1G2", "Invalid column index for getBlob", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("H3I4J5K6L7M8", "Failed to get BLOB data length", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1CE",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1CF",
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
                return cpp_dbc::unexpected(DBException("N9O0P1Q2R3S4", "Column not found: " + columnName, system_utils::captureCallStack()));
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

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("T5U6V7W8X9Y0", "Invalid column index for getBinaryStream", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("Z1A2B3C4D5E6", "Failed to get BLOB data length", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("F7G8H9I0J1K2", "Column not found: " + columnName, system_utils::captureCallStack()));
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

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("L3M4N5O6P7Q8", "Invalid column index for getBytes", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("R9S0T1U2V3W4", "Failed to get BLOB data length", system_utils::captureCallStack()));
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
