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
 * @file result_set_01.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBResultSet throwing methods
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
    // ====================================================================
    // ScyllaDBResultSet
    // ====================================================================

    cpp_dbc::expected<void, DBException> ScyllaDBResultSet::validateResultState(std::nothrow_t) const noexcept
    {
        if (!m_iterator)
        {
            SCYLLADB_DEBUG("ScyllaDBResultSet::validateResultState - ResultSet is closed");
            return cpp_dbc::unexpected(DBException("98907CB0524D", "ResultSet is closed", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBResultSet::validateCurrentRow(std::nothrow_t) const noexcept
    {
        auto result = validateResultState(std::nothrow);
        if (!result.has_value())
        {
            return result;
        }
        if (!m_currentRow)
        {
            SCYLLADB_DEBUG("ScyllaDBResultSet::validateCurrentRow - No current row available");
            return cpp_dbc::unexpected(DBException("U2V3W4X5Y6Z7", "No current row available", system_utils::captureCallStack()));
        }
        return {};
    }

    ScyllaDBResultSet::ScyllaDBResultSet(const CassResult *res)
        : m_result(res), // Takes ownership via unique_ptr
          m_iterator(cass_iterator_from_result(res))
    {
        SCYLLADB_DEBUG("ScyllaDBResultSet::constructor - Creating result set");
        m_rowCount = cass_result_row_count(res);
        m_columnCount = cass_result_column_count(res);
        SCYLLADB_DEBUG("ScyllaDBResultSet::constructor - Row count: " << m_rowCount << ", Column count: " << m_columnCount);

        for (size_t i = 0; i < m_columnCount; ++i)
        {
            const char *name;
            size_t name_length;
            if (cass_result_column_name(res, i, &name, &name_length) == CASS_OK)
            {
                std::string colName(name, name_length);
                m_columnNames.emplace_back(colName);
                m_columnMap[colName] = i;
            }
        }
    }

    ScyllaDBResultSet::~ScyllaDBResultSet()
    {
        SCYLLADB_DEBUG("ScyllaDBResultSet::destructor - Destroying result set");
        ScyllaDBResultSet::close();
    }

    void ScyllaDBResultSet::close()
    {
        SCYLLADB_DEBUG("ScyllaDBResultSet::close - Closing result set");
        DB_DRIVER_LOCK_GUARD(m_mutex);
        m_iterator.reset();
        m_result.reset();
        m_currentRow = nullptr;
    }

    bool ScyllaDBResultSet::isEmpty()
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        SCYLLADB_DEBUG("ScyllaDBResultSet::isEmpty - Result is " << (m_rowCount == 0 ? "empty" : "not empty"));
        return m_rowCount == 0;
    }

    bool ScyllaDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t ScyllaDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int ScyllaDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // String overload delegates to map lookup then index
    int ScyllaDBResultSet::getInt(const std::string &columnName)
    {
        auto result = getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    long ScyllaDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    long ScyllaDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double ScyllaDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double ScyllaDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getUUID(size_t columnIndex)
    {
        auto result = getUUID(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getUUID(const std::string &columnName)
    {
        auto result = getUUID(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getDate(const std::string &columnName)
    {
        auto result = getDate(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = getTimestamp(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> ScyllaDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    size_t ScyllaDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Stream not fully supported in simple mapping, returning in-memory stream
    std::shared_ptr<InputStream> ScyllaDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<InputStream> ScyllaDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Binary support - stubbed or implemented using bytes
    std::vector<uint8_t> ScyllaDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<uint8_t> ScyllaDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
