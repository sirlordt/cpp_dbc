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
 @brief PostgreSQL database driver implementation - PostgreSQLDBResultSet (constructor, destructor, close, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>
#include <ranges>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // PostgreSQLDBResultSet implementation — private helpers in header declaration order

    void PostgreSQLDBResultSet::notifyConnClosing(std::nothrow_t) noexcept
    {
        // Called by connection when closing — mark as closed and release PGresult
        m_closed.store(true, std::memory_order_seq_cst);
        if (m_result)
        {
            m_result.reset();
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBResultSet::initialize(std::nothrow_t) noexcept
    {
        // Register with Connection — mandatory; every ResultSet must be tracked
        // so closeAllResultSets()/notifyConnClosing() can reach it.
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("5K1LVNZIRU67",
                "Connection expired before result set could be registered",
                system_utils::captureCallStack()));
        }
        auto regResult = conn->registerResultSet(std::nothrow, weak_from_this());
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }
        return {};
    }

    // ── PrivateCtorTag Constructor ──────────────────────────────────────────

    PostgreSQLDBResultSet::PostgreSQLDBResultSet(PrivateCtorTag, std::nothrow_t,
                                                  std::weak_ptr<PostgreSQLDBConnection> conn,
                                                  PGresult *res) noexcept
        : m_connection(std::move(conn)), m_result(res)
    {
        if (m_result)
        {
            m_rowCount = PQntuples(m_result.get());
            m_fieldCount = PQnfields(m_result.get());

            // Store column names and create column name to index mapping
            for (int i : std::views::iota(0, m_fieldCount))
            {
                std::string name = PQfname(m_result.get(), i);
                m_columnNames.push_back(name);
                m_columnMap[name] = i;
            }

            m_closed.store(false, std::memory_order_seq_cst);
        }
        else
        {
            m_rowCount = 0;
            m_fieldCount = 0;
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("F32TX52NEFOR",
                "ResultSet created with null PGresult",
                system_utils::captureCallStack());
        }
    }

    PostgreSQLDBResultSet::~PostgreSQLDBResultSet()
    {
        // If not already closed by notifyConnClosing, close now
        if (!m_closed.load(std::memory_order_seq_cst))
        {
            [[maybe_unused]] auto closeResult = PostgreSQLDBResultSet::close(std::nothrow);
        }
    }

#ifdef __cpp_exceptions
    void PostgreSQLDBResultSet::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool PostgreSQLDBResultSet::isEmpty()
    {
        auto result = isEmpty(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::next()
    {
        auto result = this->next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isBeforeFirst()
    {
        auto result = this->isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isAfterLast()
    {
        auto result = this->isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t PostgreSQLDBResultSet::getRow()
    {
        auto result = this->getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int PostgreSQLDBResultSet::getInt(size_t columnIndex)
    {
        auto result = this->getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int PostgreSQLDBResultSet::getInt(const std::string &columnName)
    {
        auto result = this->getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t PostgreSQLDBResultSet::getLong(size_t columnIndex)
    {
        auto result = this->getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t PostgreSQLDBResultSet::getLong(const std::string &columnName)
    {
        auto result = this->getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double PostgreSQLDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = this->getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double PostgreSQLDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = this->getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getString(size_t columnIndex)
    {
        auto result = this->getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getString(const std::string &columnName)
    {
        auto result = this->getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = this->getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = this->getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isNull(size_t columnIndex)
    {
        auto result = this->isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isNull(const std::string &columnName)
    {
        auto result = this->isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getDate(size_t columnIndex)
    {
        auto result = this->getDate(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getDate(const std::string &columnName)
    {
        auto result = this->getDate(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = this->getTimestamp(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = this->getTimestamp(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getTime(size_t columnIndex)
    {
        auto result = this->getTime(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getTime(const std::string &columnName)
    {
        auto result = this->getTime(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> PostgreSQLDBResultSet::getColumnNames()
    {
        auto result = this->getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t PostgreSQLDBResultSet::getColumnCount()
    {
        auto result = this->getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    // BLOB support methods
    std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = this->getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = this->getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = this->getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = this->getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = this->getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = this->getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
