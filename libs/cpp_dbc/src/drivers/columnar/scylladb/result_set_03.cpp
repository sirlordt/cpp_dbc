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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file result_set_03.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBResultSet nothrow methods (part 2)
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <array>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{
    cpp_dbc::expected<int, DBException> ScyllaDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("X4Y5Z6A7B8C9", columnName, system_utils::captureCallStack()));
        }
        return getInt(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<long, DBException> ScyllaDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("126BA85C92BC", columnName, system_utils::captureCallStack()));
        }
        return getLong(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<double, DBException> ScyllaDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("Y5Z6A7B8C9D0", columnName, system_utils::captureCallStack()));
        }
        return getDouble(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("Z6A7B8C9D0E1", columnName, system_utils::captureCallStack()));
        }
        return getString(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("A7B8C9D0E1F2", columnName, system_utils::captureCallStack()));
        }
        return getBoolean(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("B8C9D0E1F2G3", columnName, system_utils::captureCallStack()));
        }
        return isNull(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getUUID(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("C9D0E1F2G3H4", columnName, system_utils::captureCallStack()));
        }
        return getUUID(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("D0E1F2G3H4I5", columnName, system_utils::captureCallStack()));
        }
        return getDate(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("E1F2G3H4I5J6", columnName, system_utils::captureCallStack()));
        }
        return getTimestamp(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> ScyllaDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return m_columnNames;
    }

    cpp_dbc::expected<size_t, DBException> ScyllaDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return m_columnCount;
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> ScyllaDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        auto bytes = getBytes(std::nothrow, columnIndex);
        if (!bytes)
        {
            return cpp_dbc::unexpected(bytes.error());
        }

        return std::shared_ptr<InputStream>(std::make_shared<ScyllaMemoryInputStream>(*bytes));
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> ScyllaDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("F2G3H4I5J6K7", columnName, system_utils::captureCallStack()));
        }
        return getBinaryStream(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> ScyllaDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("G3H4I5J6K7L8", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return std::vector<uint8_t>();
        }

        const cass_byte_t *output;
        size_t output_size;
        if (cass_value_get_bytes(val, &output, &output_size) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("H4I5J6K7L8M9", "Failed to get bytes", system_utils::captureCallStack()));
        }

        std::vector<uint8_t> data(output_size);
        std::memcpy(data.data(), output, output_size);
        return data;
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> ScyllaDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("I5J6K7L8M9N0", columnName, system_utils::captureCallStack()));
        }
        return getBytes(std::nothrow, it->second + 1);
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
