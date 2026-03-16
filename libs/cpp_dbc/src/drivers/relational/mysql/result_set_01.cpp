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

 @file result_set_01.cpp
 @brief MySQL database driver implementation - MySQLDBResultSet (constructor, destructor, close, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <span>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    // MySQLDBResultSet implementation

    cpp_dbc::expected<void, DBException> MySQLDBResultSet::validateResultState(std::nothrow_t) const noexcept
    {
        // Note: This is called from other methods that already hold the lock,
        // so we don't acquire the lock here.
        if (m_materialized)
        {
            // In materialized mode, m_result is nullptr by design.
            // The ResultSet is valid as long as it has not been closed.
            if (m_closed.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("LXRZWNZ6SGFE", "ResultSet has been closed", system_utils::captureCallStack()));
            }
            return {};
        }
        if (!m_result)
        {
            return cpp_dbc::unexpected(DBException("RUZI7TWB4Y3G", "ResultSet has been closed or is invalid", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBResultSet::validateCurrentRow(std::nothrow_t) const noexcept
    {
        // Note: This is called from other methods that already hold the lock,
        // so we don't acquire the lock here.
        auto result = validateResultState(std::nothrow);
        if (!result.has_value())
        {
            return result;
        }
        if (!m_currentRow)
        {
            return cpp_dbc::unexpected(DBException("F200B1E69DA7", "No current row - call next() first", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBResultSet::notifyConnClosing(std::nothrow_t) noexcept
    {
        m_closed.store(true, std::memory_order_release);
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBResultSet::initialize(std::nothrow_t) noexcept
    {
        // Register with Connection — mandatory; every ResultSet must be tracked
        // so closeAllResultSets()/notifyConnClosing() can reach it.
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("CSTY1MYIFZ4E",
                "Connection expired before result set could be registered",
                system_utils::captureCallStack()));
        }
        auto regResult = conn->registerResultSet(std::nothrow, std::weak_ptr<MySQLDBResultSet>(shared_from_this()));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }
        return {};
    }

    // ── PrivateCtorTag Constructor ──────────────────────────────────────────

    MySQLDBResultSet::MySQLDBResultSet(PrivateCtorTag, std::nothrow_t, MYSQL_RES *res, std::shared_ptr<MySQLDBConnection> conn) noexcept
        : m_result(res), m_connection(conn)
    {
        if (m_result)
        {
            m_rowCount = mysql_num_rows(m_result.get());
            m_fieldCount = mysql_num_fields(m_result.get());

            // Store column names and create column name to index mapping
            const MYSQL_FIELD *fields = mysql_fetch_fields(m_result.get());
            size_t idx = 0;
            for (const auto &field : std::span(fields, m_fieldCount))
            {
                std::string name = field.name;
                m_columnNames.push_back(name);
                m_columnMap[name] = idx++;
            }
        }
        else
        {
            m_rowCount = 0;
            m_fieldCount = 0;
        }

        m_closed.store(false, std::memory_order_release);
    }

    // ── Materialized-mode Constructor ──────────────────────────────────────
    MySQLDBResultSet::MySQLDBResultSet(PrivateCtorTag,
                                       std::nothrow_t,
                                       std::vector<std::string> columnNames,
                                       std::vector<std::vector<std::optional<std::string>>> rows,
                                       std::shared_ptr<MySQLDBConnection> conn) noexcept
        : m_connection(conn), m_materialized(true),
          m_materializedRows(std::move(rows))
    {
        m_columnNames = std::move(columnNames);
        m_fieldCount = m_columnNames.size();
        m_rowCount = m_materializedRows.size();

        size_t idx = 0;
        for (const auto &name : m_columnNames)
        {
            m_columnMap[name] = idx++;
        }

        m_currentRowPtrs.resize(m_fieldCount, nullptr);
        m_currentLengths.resize(m_fieldCount, 0);
        m_closed.store(false, std::memory_order_release);
    }

    MySQLDBResultSet::~MySQLDBResultSet()
    {
        auto closeResult = MySQLDBResultSet::close(std::nothrow);
        if (!closeResult.has_value())
        {
            MYSQL_DEBUG("MySQLDBResultSet::destructor - close() failed: %s", closeResult.error().what_s().data());
        }
    }

#ifdef __cpp_exceptions
    void MySQLDBResultSet::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MySQLDBResultSet::isEmpty()
    {
        auto result = isEmpty(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool MySQLDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t MySQLDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int MySQLDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int MySQLDBResultSet::getInt(const std::string &columnName)
    {
        auto result = getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t MySQLDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t MySQLDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double MySQLDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double MySQLDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getDate(const std::string &columnName)
    {
        auto result = getDate(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = getTimestamp(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getTime(size_t columnIndex)
    {
        auto result = getTime(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getTime(const std::string &columnName)
    {
        auto result = getTime(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> MySQLDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    size_t MySQLDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // BLOB support methods for MySQLDBResultSet
    std::shared_ptr<Blob> MySQLDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<Blob> MySQLDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<uint8_t> MySQLDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<uint8_t> MySQLDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
