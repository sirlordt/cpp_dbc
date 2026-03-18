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
 @brief SQLite database driver implementation - SQLiteDBResultSet (constructor, destructor, close, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{
    // No static members needed - connection tracking is done via weak_ptr in PreparedStatement

    // SQLiteDBResultSet implementation
    //
    // IMPORTANT: SQLite uses a CURSOR-BASED model where sqlite3_step() and sqlite3_column_*()
    // communicate with the sqlite3* connection handle on every call. This is different from
    // MySQL/PostgreSQL which load all results into client memory (MYSQL_RES*/PGresult*).
    //
    // Because of this, SQLiteDBResultSet MUST use the global file-level mutex from FileMutexRegistry
    // to prevent race conditions when multiple threads access the same database file
    // (e.g., one thread iterating results while another does pool validation or other operations).
    //
    // ── Public nothrow constructor (PrivateCtorTag) ───────────────────────────────
    // sqlite3_column_count and sqlite3_column_name are C APIs that never throw.
    // No recoverable exceptions → no try/catch needed.
    SQLiteDBResultSet::SQLiteDBResultSet(
        SQLiteDBResultSet::PrivateCtorTag,
        std::nothrow_t,
        sqlite3_stmt *stmt,
        bool ownStatement,
        std::shared_ptr<SQLiteDBConnection> conn,
        std::shared_ptr<SQLiteDBPreparedStatement> prepStmt) noexcept
        : m_stmt(stmt), m_ownStatement(ownStatement),
          m_connection(conn), m_preparedStatement(prepStmt)
    {
        m_closed.store(false, std::memory_order_seq_cst);
        if (m_stmt)
        {
            // Get column count
            m_fieldCount = static_cast<size_t>(sqlite3_column_count(m_stmt));

            // Store column names and create column name to index mapping
            for (size_t i = 0; i < m_fieldCount; i++)
            {
                std::string name = sqlite3_column_name(m_stmt, static_cast<int>(i));
                m_columnNames.push_back(name);
                m_columnMap[name] = i;
            }
        }
        // NOTE: Registration with Connection/PreparedStatement is done in initialize()
        // Cannot be done here because weak_from_this() requires shared_ptr to exist
    }

    cpp_dbc::expected<void, DBException> SQLiteDBResultSet::initialize(std::nothrow_t) noexcept
    {
        // Register with Connection — mandatory; every ResultSet must be tracked.
        // If registration fails the ResultSet is NOT returned — destructor closes it.
        auto connShared = m_connection.lock();
        if (!connShared)
        {
            return cpp_dbc::unexpected(DBException("Z28J65UB2E5Q",
                "Connection expired before result set could be registered",
                system_utils::captureCallStack()));
        }
        auto connRegResult = connShared->registerResultSet(std::nothrow, std::weak_ptr<SQLiteDBResultSet>(shared_from_this()));
        if (!connRegResult.has_value())
        {
            return cpp_dbc::unexpected(connRegResult.error());
        }

        // Register with PreparedStatement if applicable
        if (auto prepStmtShared = m_preparedStatement.lock())
        {
            auto stmtRegResult = prepStmtShared->registerResultSet(std::nothrow, std::weak_ptr<SQLiteDBResultSet>(shared_from_this()));
            if (!stmtRegResult.has_value())
            {
                return cpp_dbc::unexpected(stmtRegResult.error());
            }
        }

        return {};
    }

    // ── Destructor ────────────────────────────────────────────────────────────────
    SQLiteDBResultSet::~SQLiteDBResultSet()
    {
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

#ifdef __cpp_exceptions
    bool SQLiteDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t SQLiteDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int SQLiteDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int SQLiteDBResultSet::getInt(const std::string &columnName)
    {
        auto result = getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t SQLiteDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t SQLiteDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double SQLiteDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double SQLiteDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getDate(const std::string &columnName)
    {
        auto result = getDate(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = getTimestamp(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getTime(size_t columnIndex)
    {
        auto result = getTime(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBResultSet::getTime(const std::string &columnName)
    {
        auto result = getTime(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> SQLiteDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t SQLiteDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBResultSet::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool SQLiteDBResultSet::isEmpty()
    {
        auto result = isEmpty(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> SQLiteDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> SQLiteDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> SQLiteDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> SQLiteDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> SQLiteDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> SQLiteDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
