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

 @file driver_sqlite.cpp
 @brief SQLite database driver implementation

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include "cpp_dbc/drivers/relational/sqlite_blob.hpp"
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

// Debug output is controlled by -DDEBUG_SQLITE=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_SQLITE) && DEBUG_SQLITE) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define SQLITE_DEBUG(x) std::cout << "[SQLite] " << x << std::endl
#else
#define SQLITE_DEBUG(x)
#endif

#if USE_SQLITE

namespace cpp_dbc
{
    namespace SQLite
    {
        // No static members needed - connection tracking is done via weak_ptr in PreparedStatement

        // SQLiteDBResultSet implementation
        SQLiteDBResultSet::SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteDBConnection> conn)
            : m_stmt(stmt), m_ownStatement(ownStatement), m_rowPosition(0), m_rowCount(0), m_fieldCount(0),
              m_columnNames(), m_columnMap(), m_hasData(false), m_closed(false), m_connection(conn)
        {
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
        }

        SQLiteDBResultSet::~SQLiteDBResultSet()
        {
            try
            {
                // Solo llamar a close() si no está ya cerrado
                if (!m_closed)
                {
                    close();
                }
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }

            // Sleep briefly to ensure resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::next(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed)
                {
                    return false;
                }

                int result = sqlite3_step(stmt);

                if (result == SQLITE_ROW)
                {
                    m_rowPosition++;
                    m_hasData = true;
                    return true;
                }
                else if (result == SQLITE_DONE)
                {
                    m_rowPosition++;
                    m_hasData = false;
                    return false;
                }
                else
                {
                    // Error occurred
                    return cpp_dbc::unexpected(DBException("A1B2C3D4E5F6", "Error stepping through SQLite result set: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))),
                                                           system_utils::captureCallStack()));
                }
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("A1B2C3D4E5F7",
                                                       std::string("next failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("A1B2C3D4E5F8",
                                                       "next failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::next()
        {
            auto result = next(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
        {
            try
            {
                return m_rowPosition == 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("IBFA1B2C3D4E",
                                                       std::string("isBeforeFirst failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("IBFA1B2C3D4F",
                                                       "isBeforeFirst failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::isBeforeFirst()
        {
            auto result = isBeforeFirst(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isAfterLast(std::nothrow_t) noexcept
        {
            try
            {
                return m_rowPosition > 0 && !m_hasData;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("IALA1B2C3D4E",
                                                       std::string("isAfterLast failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("IALA1B2C3D4F",
                                                       "isAfterLast failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::isAfterLast()
        {
            auto result = isAfterLast(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<uint64_t, DBException> SQLiteDBResultSet::getRow(std::nothrow_t) noexcept
        {
            try
            {
                return m_rowPosition;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GR1A1B2C3D4E",
                                                       std::string("getRow failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GR1A1B2C3D4F",
                                                       "getRow failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        uint64_t SQLiteDBResultSet::getRow()
        {
            auto result = getRow(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("7A8B9C0D1E2F", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return 0; // Return 0 for NULL values (similar to JDBC)
                }

                return sqlite3_column_int(stmt, idx);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GI1A1B2C3D4E",
                                                       std::string("getInt failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GI1A1B2C3D4F",
                                                       "getInt failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        int SQLiteDBResultSet::getInt(size_t columnIndex)
        {
            auto result = getInt(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("3G4H5I6J7K8L", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GIN1A2B3C4D5",
                                                       std::string("getInt failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GIN1A2B3C4D6",
                                                       "getInt failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        int SQLiteDBResultSet::getInt(const std::string &columnName)
        {
            auto result = getInt(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<long, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("DDAABD02C9D3", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return 0;
                }

                return sqlite3_column_int64(stmt, idx);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GL1A1B2C3D4E",
                                                       std::string("getLong failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GL1A1B2C3D4F",
                                                       "getLong failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        long SQLiteDBResultSet::getLong(size_t columnIndex)
        {
            auto result = getLong(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<long, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("20C1324B8D71", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getLong(std::nothrow, it->second + 1);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GLN1A2B3C4D5",
                                                       std::string("getLong failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GLN1A2B3C4D6",
                                                       "getLong failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        long SQLiteDBResultSet::getLong(const std::string &columnName)
        {
            auto result = getLong(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("9M0N1O2P3Q4R", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return 0.0;
                }

                return sqlite3_column_double(stmt, idx);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GD1A1B2C3D4E",
                                                       std::string("getDouble failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GD1A1B2C3D4F",
                                                       "getDouble failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        double SQLiteDBResultSet::getDouble(size_t columnIndex)
        {
            auto result = getDouble(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("5S6T7U8V9W0X", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getDouble(std::nothrow, it->second + 1);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GDN1A2B3C4D5",
                                                       std::string("getDouble failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GDN1A2B3C4D6",
                                                       "getDouble failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        double SQLiteDBResultSet::getDouble(const std::string &columnName)
        {
            auto result = getDouble(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("1Y2Z3A4B5C6D", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return std::string("");
                }

                const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
                return text ? std::string(text) : std::string("");
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GS1A1B2C3D4E",
                                                       std::string("getString failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GS1A1B2C3D4F",
                                                       "getString failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::string SQLiteDBResultSet::getString(size_t columnIndex)
        {
            auto result = getString(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("93A82C42FA7B", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getString(std::nothrow, it->second + 1);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GSN1A2B3C4D5",
                                                       std::string("getString failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GSN1A2B3C4D6",
                                                       "getString failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::string SQLiteDBResultSet::getString(const std::string &columnName)
        {
            auto result = getString(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("7A8B9C0D1E2F", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return false;
                }

                return sqlite3_column_int(stmt, idx) != 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GB1A1B2C3D4E",
                                                       std::string("getBoolean failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GB1A1B2C3D4F",
                                                       "getBoolean failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::getBoolean(size_t columnIndex)
        {
            auto result = getBoolean(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto intResult = getInt(std::nothrow, columnName);
                if (!intResult)
                {
                    return cpp_dbc::unexpected(intResult.error());
                }
                return *intResult != 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBN1A2B3C4D5",
                                                       std::string("getBoolean failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBN1A2B3C4D6",
                                                       "getBoolean failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::getBoolean(const std::string &columnName)
        {
            auto result = getBoolean(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("407EBCBBE843", "Invalid column index or row position",
                                                           system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);
                return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("IN1A1B2C3D4E",
                                                       std::string("isNull failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("IN1A1B2C3D4F",
                                                       "isNull failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::isNull(size_t columnIndex)
        {
            auto result = isNull(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("8BAE4B58A947", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return isNull(std::nothrow, it->second + 1);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("INN1A2B3C4D5",
                                                       std::string("isNull failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("INN1A2B3C4D6",
                                                       "isNull failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBResultSet::isNull(const std::string &columnName)
        {
            auto result = isNull(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::vector<std::string>, DBException> SQLiteDBResultSet::getColumnNames(std::nothrow_t) noexcept
        {
            try
            {
                return m_columnNames;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GCN1A2B3C4D5",
                                                       std::string("getColumnNames failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GCN1A2B3C4D6",
                                                       "getColumnNames failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::vector<std::string> SQLiteDBResultSet::getColumnNames()
        {
            auto result = getColumnNames(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<size_t, DBException> SQLiteDBResultSet::getColumnCount(std::nothrow_t) noexcept
        {
            try
            {
                return m_fieldCount;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GCC1A2B3C4D5",
                                                       std::string("getColumnCount failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GCC1A2B3C4D6",
                                                       "getColumnCount failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        size_t SQLiteDBResultSet::getColumnCount()
        {
            auto result = getColumnCount(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        void SQLiteDBResultSet::close()
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Evitar cerrar dos veces o si ya está cerrado
            if (m_closed)
            {
                return;
            }

            if (m_ownStatement && m_stmt)
            {
                try
                {
                    // Verificar si la conexión todavía está activa
                    auto conn = m_connection.lock();
                    bool connectionValid = conn && !conn->isClosed();

                    if (connectionValid)
                    {
                        // Reset the statement first to ensure all data is cleared
                        int resetResult = sqlite3_reset(m_stmt);
                        if (resetResult != SQLITE_OK)
                        {
                            SQLITE_DEBUG("7A8B9C0D1E2F: Error resetting SQLite statement: "
                                         << sqlite3_errstr(resetResult));
                        }

                        // Finalize the statement - only if connection is still valid
                        // If connection is closed, it already finalized all statements via sqlite3_next_stmt()
                        int finalizeResult = sqlite3_finalize(m_stmt);
                        if (finalizeResult != SQLITE_OK)
                        {
                            SQLITE_DEBUG("8H9I0J1K2L3M: Error finalizing SQLite statement: "
                                         << sqlite3_errstr(finalizeResult));
                        }
                    }
                    else
                    {
                        SQLITE_DEBUG("5M6N7O8P9Q0R: SQLiteResultSet::close - Connection is closed or invalid, skipping statement finalization");
                        // Don't finalize - connection already did it
                    }
                }
                catch (const std::exception &e)
                {
                    SQLITE_DEBUG("9S0T1U2V3W4X: Exception during SQLite statement close: " << e.what());
                }
                catch (...)
                {
                    SQLITE_DEBUG("5Y6Z7A8B9C0D: Unknown exception during SQLite statement close");
                }
            }

            // Clear the pointer
            m_stmt = nullptr;

            // Marcar como cerrado antes de limpiar los datos
            m_closed = true;

            // Clear any cached data
            m_columnNames.clear();
            m_columnMap.clear();

            // Clear the connection reference
            m_connection.reset();

            // Sleep briefly to ensure resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        bool SQLiteDBResultSet::isEmpty()
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_rowPosition == 0 && !m_hasData;
        }

        // SQLiteDBPreparedStatement implementation
        sqlite3 *SQLiteDBPreparedStatement::getSQLiteConnection() const
        {
            auto conn = m_db.lock();
            if (!conn)
            {
                throw DBException("471F2E35F962", "SQLite connection has been closed", system_utils::captureCallStack());
            }
            return conn.get();
        }

        SQLiteDBPreparedStatement::SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, const std::string &sql)
            : m_db(db), m_sql(sql), m_stmt(nullptr), m_closed(false), m_blobValues(), m_blobObjects(), m_streamObjects()
        {
            sqlite3 *dbPtr = getSQLiteConnection();

            sqlite3_stmt *rawStmt = nullptr;
            int result = sqlite3_prepare_v2(dbPtr, m_sql.c_str(), -1, &rawStmt, nullptr);
            if (result != SQLITE_OK)
            {
                throw DBException("3K4L5M6N7O8P", "Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }

            // Transfer ownership to smart pointer
            m_stmt.reset(rawStmt);

            // Initialize BLOB-related vectors
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            m_blobValues.resize(paramCount);
            m_blobObjects.resize(paramCount);
            m_streamObjects.resize(paramCount);
        }

        SQLiteDBPreparedStatement::~SQLiteDBPreparedStatement()
        {
            try
            {
                // Make sure to close the statement and clean up resources
                if (!m_closed)
                {
                    close();
                }
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }

            // Smart pointer m_stmt will automatically call sqlite3_finalize via SQLiteStmtDeleter
            // No need for manual cleanup - the destructor handles it automatically

            // Mark as closed
            m_closed = true;

            // Sleep briefly to ensure resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        void SQLiteDBPreparedStatement::close()
        {
            if (!m_closed && m_stmt)
            {
                // Check if connection is still valid before resetting
                auto dbPtr = m_db.lock();
                if (dbPtr)
                {
                    // Reset the statement first to ensure all bindings are cleared
                    int resetResult = sqlite3_reset(m_stmt.get());
                    if (resetResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("7K8L9M0N1O2P: Error resetting SQLite statement: "
                                     << sqlite3_errstr(resetResult));
                    }

                    int clearResult = sqlite3_clear_bindings(m_stmt.get());
                    if (clearResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("3Q4R5S6T7U8V: Error clearing SQLite statement bindings: "
                                     << sqlite3_errstr(clearResult));
                    }

                    // Smart pointer will automatically call sqlite3_finalize via SQLiteStmtDeleter
                    m_stmt.reset();
                }
                else
                {
                    // Connection is closed, release without finalizing (connection already did it)
                    m_stmt.release();
                    SQLITE_DEBUG("5C6D7E8F9G0H: Connection closed, releasing statement without finalize");
                }
            }
            m_closed = true;

            // Sleep briefly to ensure resources are released
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        void SQLiteDBPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, release the statement pointer without calling sqlite3_finalize
            // since the connection is already being destroyed and will clean up all statements
            m_stmt.release(); // Release without calling deleter
            m_closed = true;
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("9Q0R1S2T3U4V", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("5W6X7Y8Z9A0B", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                // Get the number of parameters in the statement
                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("1C2D3E4F5G6H", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, value);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("7I8J9K0L1M2N", "Failed to bind integer parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SI1A1B2C3D4E",
                                                       std::string("setInt failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SI1A1B2C3D4F",
                                                       "setInt failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setInt(int parameterIndex, int value)
        {
            auto result = setInt(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, long value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("3O4P5Q6R7S8T", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("9U0V1W2X3Y4Z", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int result = sqlite3_bind_int64(m_stmt.get(), parameterIndex, value);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("5A6B7C8D9E0F", "Failed to bind long parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SL1A1B2C3D4E",
                                                       std::string("setLong failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SL1A1B2C3D4F",
                                                       "setLong failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setLong(int parameterIndex, long value)
        {
            auto result = setLong(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("1G2H3I4J5K6L", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("7M8N9O0P1Q2R", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                // Get the number of parameters in the statement
                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("3S4T5U6V7W8X", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                int result = sqlite3_bind_double(m_stmt.get(), parameterIndex, value);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("9Y0Z1A2B3C4D", "Failed to bind double parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SD1A1B2C3D4E",
                                                       std::string("setDouble failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SD1A1B2C3D4F",
                                                       "setDouble failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setDouble(int parameterIndex, double value)
        {
            auto result = setDouble(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("5E6F7G8H9I0J", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("1K2L3M4N5O6P", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                // Get the number of parameters in the statement
                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("7Q8R9S0T1U2V", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                // SQLITE_TRANSIENT tells SQLite to make its own copy of the data
                int result = sqlite3_bind_text(m_stmt.get(), parameterIndex, value.c_str(), -1, SQLITE_TRANSIENT);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("3W4X5Y6Z7A8B", "Failed to bind string parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value='" + value + "'" + ", result=" + std::to_string(result) + ")",
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SS1A1B2C3D4E",
                                                       std::string("setString failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SS1A1B2C3D4F",
                                                       "setString failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            auto result = setString(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("9C0D1E2F3G4H", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("5I6J7K8L9M0N", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                // Get the number of parameters in the statement
                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("1O2P3Q4R5S6T", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                int intValue = value ? 1 : 0;
                int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, intValue);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("7U8V9W0X1Y2Z", "Failed to bind boolean parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + (value ? "true" : "false") + ", result=" + std::to_string(result) + ")",
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SB1A1B2C3D4E",
                                                       std::string("setBoolean failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SB1A1B2C3D4F",
                                                       "setBoolean failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            auto result = setBoolean(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, [[maybe_unused]] Types type) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("3A4B5C6D7E8F", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // SQLite parameter indices are 1-based, which matches our API
                // Make sure parameterIndex is valid
                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("9G0H1I2J3K4L", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("5M6N7O8P9Q0R", "Failed to bind null parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SN1A1B2C3D4E",
                                                       std::string("setNull failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SN1A1B2C3D4F",
                                                       "setNull failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setNull(int parameterIndex, [[maybe_unused]] Types type)
        {
            auto result = setNull(std::nothrow, parameterIndex, type);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
        {
            return setString(std::nothrow, parameterIndex, value);
        }

        void SQLiteDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            auto result = setDate(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
        {
            return setString(std::nothrow, parameterIndex, value);
        }

        void SQLiteDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            auto result = setTimestamp(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("1S2T3U4V5W6X", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // Reset the statement to ensure it's ready for execution
                int resetResult = sqlite3_reset(m_stmt.get());
                if (resetResult != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("7Y8Z9A0B1C2D", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                           system_utils::captureCallStack()));
                }

                auto resultSet = std::make_shared<SQLiteDBResultSet>(m_stmt.get(), false, nullptr);
                return std::shared_ptr<RelationalDBResultSet>(resultSet);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("EQ1A1B2C3D4E",
                                                       std::string("executeQuery failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("EQ1A1B2C3D4F",
                                                       "executeQuery failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<RelationalDBResultSet> SQLiteDBPreparedStatement::executeQuery()
        {
            auto result = executeQuery(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<uint64_t, DBException> SQLiteDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("3E4F5G6H7I8J", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // Reset the statement to ensure it's ready for execution
                int resetResult = sqlite3_reset(m_stmt.get());
                if (resetResult != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("9K0L1M2N3O4P", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                           system_utils::captureCallStack()));
                }

                // Execute the statement
                int result = sqlite3_step(m_stmt.get());
                if (result != SQLITE_DONE)
                {
                    return cpp_dbc::unexpected(DBException("5Q6R7S8T9U0V", "Failed to execute update: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(result) + ")",
                                                           system_utils::captureCallStack()));
                }

                // Get the number of affected rows
                uint64_t changes = sqlite3_changes(dbPtr);

                // Reset the statement after execution to allow reuse
                resetResult = sqlite3_reset(m_stmt.get());
                if (resetResult != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("1W2X3Y4Z5A6B", "Failed to reset statement after execution: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                                           system_utils::captureCallStack()));
                }

                return changes;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("EU1A1B2C3D4E",
                                                       std::string("executeUpdate failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("EU1A1B2C3D4F",
                                                       "executeUpdate failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        uint64_t SQLiteDBPreparedStatement::executeUpdate()
        {
            auto result = executeUpdate(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBPreparedStatement::execute(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("7C8D9E0F1G2H", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                // Reset the statement to ensure it's ready for execution
                sqlite3_reset(m_stmt.get());

                // Execute the statement
                int result = sqlite3_step(m_stmt.get());

                if (result == SQLITE_ROW)
                {
                    // There are rows to fetch, so this is a query
                    sqlite3_reset(m_stmt.get());
                    return true;
                }
                else if (result == SQLITE_DONE)
                {
                    // No rows to fetch, so this is an update
                    return false;
                }
                else
                {
                    return cpp_dbc::unexpected(DBException("3I4J5K6L7M8N", "Failed to execute statement: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("EX1A1B2C3D4E",
                                                       std::string("execute failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("EX1A1B2C3D4F",
                                                       "execute failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBPreparedStatement::execute()
        {
            auto result = execute(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::close(std::nothrow_t) noexcept
        {
            try
            {
                if (!m_closed && m_stmt)
                {
                    auto dbPtr = m_db.lock();
                    if (dbPtr)
                    {
                        int resetResult = sqlite3_reset(m_stmt.get());
                        if (resetResult != SQLITE_OK)
                        {
                            SQLITE_DEBUG("7K8L9M0N1O2P: Error resetting SQLite statement: "
                                         << sqlite3_errstr(resetResult));
                        }

                        int clearResult = sqlite3_clear_bindings(m_stmt.get());
                        if (clearResult != SQLITE_OK)
                        {
                            SQLITE_DEBUG("3Q4R5S6T7U8V: Error clearing SQLite statement bindings: "
                                         << sqlite3_errstr(clearResult));
                        }

                        m_stmt.reset();
                    }
                    else
                    {
                        m_stmt.release();
                        SQLITE_DEBUG("5C6D7E8F9G0H: Connection closed, releasing statement without finalize");
                    }
                }
                m_closed = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("CL1A1B2C3D4E",
                                                       std::string("close failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("CL1A1B2C3D4F",
                                                       "close failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        // SQLiteDBConnection implementation

        SQLiteDBConnection::SQLiteDBConnection(const std::string &database,
                                               const std::map<std::string, std::string> &options)
            : m_db(nullptr), m_closed(false), m_autoCommit(true), m_transactionActive(false),
              m_isolationLevel(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE), // SQLite default
              m_url("cpp_dbc:sqlite://" + database)
        {
            try
            {
                SQLITE_DEBUG("Creating connection to: " << database);

                // Verificar si el archivo existe (para bases de datos de archivo)
                if (database != ":memory:")
                {
                    std::ifstream fileCheck(database.c_str());
                    if (!fileCheck)
                    {
                        SQLITE_DEBUG("Database file does not exist, will be created: " << database);
                    }
                    else
                    {
                        SQLITE_DEBUG("Database file exists: " << database);
                        fileCheck.close();
                    }
                }
                else
                {
                    SQLITE_DEBUG("Using in-memory database");
                }

                SQLITE_DEBUG("Calling sqlite3_open_v2");
                sqlite3 *rawDb = nullptr;
                int result = sqlite3_open_v2(database.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
                if (result != SQLITE_OK)
                {
                    std::string error = sqlite3_errmsg(rawDb);
                    SQLITE_DEBUG("1I2J3K4L5M6N: Failed to open database: " << error);
                    sqlite3_close_v2(rawDb);
                    throw DBException("9O0P1Q2R3S4T", "Failed to connect to SQLite database: " + error,
                                      system_utils::captureCallStack());
                }

                // Create shared_ptr with custom deleter for sqlite3*
                m_db = std::shared_ptr<sqlite3>(rawDb, SQLiteDbDeleter());

                SQLITE_DEBUG("Database opened successfully");

                // Aplicar opciones de configuración
                SQLITE_DEBUG("Applying configuration options");
                for (const auto &option : options)
                {
                    SQLITE_DEBUG("Processing option: " << option.first << "=" << option.second);
                    if (option.first == "foreign_keys" && option.second == "true")
                    {
                        executeUpdate("PRAGMA foreign_keys = ON");
                    }
                    else if (option.first == "journal_mode" && option.second == "WAL")
                    {
                        executeUpdate("PRAGMA journal_mode = WAL");
                    }
                    else if (option.first == "synchronous" && option.second == "FULL")
                    {
                        executeUpdate("PRAGMA synchronous = FULL");
                    }
                    else if (option.first == "synchronous" && option.second == "NORMAL")
                    {
                        executeUpdate("PRAGMA synchronous = NORMAL");
                    }
                    else if (option.first == "synchronous" && option.second == "OFF")
                    {
                        executeUpdate("PRAGMA synchronous = OFF");
                    }
                }

                // Si no se especificó foreign_keys en las opciones, habilitarlo por defecto
                if (options.find("foreign_keys") == options.end())
                {
                    SQLITE_DEBUG("Enabling foreign keys by default");
                    executeUpdate("PRAGMA foreign_keys = ON");
                }

                SQLITE_DEBUG("Connection created successfully");
            }
            catch (const DBException &e)
            {
                SQLITE_DEBUG("3U4V5W6X7Y8Z: DBException: " << e.what_s());
                throw;
            }
            catch (const std::exception &e)
            {
                SQLITE_DEBUG("9A0B1C2D3E4F: std::exception: " << e.what());
                throw DBException("F1262039BA12", "SQLiteConnection constructor exception: " + std::string(e.what()),
                                  system_utils::captureCallStack());
            }
            catch (...)
            {
                SQLITE_DEBUG("5G6H7I8J9K0L: Unknown exception");
                throw DBException("D68199523A23", "SQLiteConnection constructor unknown exception",
                                  system_utils::captureCallStack());
            }
        }

        SQLiteDBConnection::~SQLiteDBConnection()
        {
            // Make sure to close the connection and clean up resources
            try
            {
                close();
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }
        }

        void SQLiteDBConnection::close()
        {
            if (!m_closed && m_db)
            {
                try
                {
                    // Notify all active statements that connection is closing
                    {
                        std::lock_guard<std::mutex> lock(m_statementsMutex);
                        for (auto &weak_stmt : m_activeStatements)
                        {
                            auto stmt = weak_stmt.lock();
                            if (stmt)
                            {
                                stmt->notifyConnClosing();
                            }
                        }
                        m_activeStatements.clear();
                    }

                    // Explicitly finalize all prepared statements
                    // This is a more aggressive approach to ensure all statements are properly cleaned up
                    sqlite3_stmt *stmt;
                    while ((stmt = sqlite3_next_stmt(m_db.get(), nullptr)) != nullptr)
                    {
                        int result = sqlite3_finalize(stmt);
                        if (result != SQLITE_OK)
                        {
                            SQLITE_DEBUG("1M2N3O4P5Q6R: Error finalizing SQLite statement during connection close: "
                                         << sqlite3_errstr(result));
                        }
                    }

                    // Call sqlite3_release_memory to free up caches and unused memory
                    [[maybe_unused]]
                    int releasedMemory = sqlite3_release_memory(1000000); // Try to release up to 1MB of memory
                    SQLITE_DEBUG("Released " << releasedMemory << " bytes of SQLite memory");

                    // Smart pointer will automatically call sqlite3_close_v2 via SQLiteDbDeleter
                    m_db.reset();
                    m_closed = true;

                    // Sleep for 10ms to avoid problems with concurrency and memory stability
                    // This helps ensure all resources are properly released
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                catch (const std::exception &e)
                {
                    SQLITE_DEBUG("3Y4Z5A6B7C8D: Exception during SQLite connection close: " << e.what());
                    // Asegurarse de que el db se establece a nullptr y closed a true incluso si hay una excepción
                    m_db.reset();
                    m_closed = true;
                }
            }
        }

        bool SQLiteDBConnection::isClosed()
        {
            return m_closed;
        }

        void SQLiteDBConnection::returnToPool()
        {
            // Don't physically close the connection, just mark it as available
            // so it can be reused by the pool

            // Reset the connection state if necessary
            try
            {
                // Make sure autocommit is enabled for the next time the connection is used
                if (!m_autoCommit)
                {
                    setAutoCommit(true);
                }

                // We don't set closed = true because we want to keep the connection open
                // Just mark it as available for reuse
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
        }

        bool SQLiteDBConnection::isPooled()
        {
            return false;
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> SQLiteDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("5U6V7W8X9Y0Z", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                auto stmt = std::make_shared<SQLiteDBPreparedStatement>(std::weak_ptr<sqlite3>(m_db), sql);
                registerStatement(std::weak_ptr<SQLiteDBPreparedStatement>(stmt));
                return std::shared_ptr<RelationalDBPreparedStatement>(stmt);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("PS1A1B2C3D4E",
                                                       std::string("prepareStatement failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("PS1A1B2C3D4F",
                                                       "prepareStatement failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<RelationalDBPreparedStatement> SQLiteDBConnection::prepareStatement(const std::string &sql)
        {
            auto result = prepareStatement(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("1A2B3C4D5E6F", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    std::string errorMsg = sqlite3_errmsg(m_db.get());
                    return cpp_dbc::unexpected(DBException("7G8H9I0J1K2L", "Failed to prepare query: " + errorMsg,
                                                           system_utils::captureCallStack()));
                }

                if (!stmt)
                {
                    return cpp_dbc::unexpected(DBException("1DEA86F65A95", "Statement is null after successful preparation",
                                                           system_utils::captureCallStack()));
                }

                auto self = std::dynamic_pointer_cast<SQLiteDBConnection>(shared_from_this());
                auto resultSet = std::make_shared<SQLiteDBResultSet>(stmt, true, self);
                return std::shared_ptr<RelationalDBResultSet>(resultSet);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("EQ2A1B2C3D4E",
                                                       std::string("executeQuery failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("EQ2A1B2C3D4F",
                                                       "executeQuery failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<RelationalDBResultSet> SQLiteDBConnection::executeQuery(const std::string &sql)
        {
            auto result = executeQuery(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<uint64_t, DBException> SQLiteDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("3M4N5O6P7Q8R", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                char *errmsg = nullptr;
                int result = sqlite3_exec(m_db.get(), sql.c_str(), nullptr, nullptr, &errmsg);

                if (result != SQLITE_OK)
                {
                    std::string error = errmsg ? errmsg : "Unknown error";
                    sqlite3_free(errmsg);
                    return cpp_dbc::unexpected(DBException("9S0T1U2V3W4X", "Failed to execute update: " + error,
                                                           system_utils::captureCallStack()));
                }

                return static_cast<uint64_t>(sqlite3_changes(m_db.get()));
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("EU2A1B2C3D4E",
                                                       std::string("executeUpdate failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("EU2A1B2C3D4F",
                                                       "executeUpdate failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        uint64_t SQLiteDBConnection::executeUpdate(const std::string &sql)
        {
            auto result = executeUpdate(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<void, DBException> SQLiteDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("5Y6Z7A8B9C0D", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                if (this->m_autoCommit != autoCommit)
                {
                    this->m_autoCommit = autoCommit;
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SAC1A2B3C4D5",
                                                       std::string("setAutoCommit failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SAC1A2B3C4D6",
                                                       "setAutoCommit failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBConnection::setAutoCommit(bool autoCommit)
        {
            auto result = setAutoCommit(std::nothrow, autoCommit);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBConnection::getAutoCommit(std::nothrow_t) noexcept
        {
            try
            {
                return m_autoCommit;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GAC1A2B3C4D5",
                                                       std::string("getAutoCommit failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GAC1A2B3C4D6",
                                                       "getAutoCommit failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBConnection::getAutoCommit()
        {
            auto result = getAutoCommit(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBConnection::beginTransaction(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("FD82C45A3E09", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                if (m_transactionActive)
                {
                    return false;
                }

                if (m_autoCommit)
                {
                    m_autoCommit = false;
                }

                auto updateResult = executeUpdate(std::nothrow, "BEGIN TRANSACTION");
                if (!updateResult)
                {
                    return cpp_dbc::unexpected(updateResult.error());
                }

                m_transactionActive = true;
                return true;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("BT1A1B2C3D4E",
                                                       std::string("beginTransaction failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("BT1A1B2C3D4F",
                                                       "beginTransaction failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBConnection::beginTransaction()
        {
            auto result = beginTransaction(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<bool, DBException> SQLiteDBConnection::transactionActive(std::nothrow_t) noexcept
        {
            try
            {
                return m_transactionActive;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("TA1A1B2C3D4E",
                                                       std::string("transactionActive failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("TA1A1B2C3D4F",
                                                       "transactionActive failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        bool SQLiteDBConnection::transactionActive()
        {
            auto result = transactionActive(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<void, DBException> SQLiteDBConnection::commit(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("1E2F3G4H5I6J", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                auto updateResult = executeUpdate(std::nothrow, "COMMIT");
                if (!updateResult)
                {
                    return cpp_dbc::unexpected(updateResult.error());
                }

                m_transactionActive = false;
                m_autoCommit = true;
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("CO1A1B2C3D4E",
                                                       std::string("commit failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("CO1A1B2C3D4F",
                                                       "commit failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBConnection::commit()
        {
            auto result = commit(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBConnection::rollback(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("7K8L9M0N1O2P", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                auto updateResult = executeUpdate(std::nothrow, "ROLLBACK");
                if (!updateResult)
                {
                    return cpp_dbc::unexpected(updateResult.error());
                }

                m_transactionActive = false;
                m_autoCommit = true;
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RB1A1B2C3D4E",
                                                       std::string("rollback failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RB1A1B2C3D4F",
                                                       "rollback failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBConnection::rollback()
        {
            auto result = rollback(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_db)
                {
                    return cpp_dbc::unexpected(DBException("3Q4R5S6T7U8V", "Connection is closed",
                                                           system_utils::captureCallStack()));
                }

                // SQLite only supports SERIALIZABLE isolation level
                if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    return cpp_dbc::unexpected(DBException("9W0X1Y2Z3A4B", "SQLite only supports SERIALIZABLE isolation level",
                                                           system_utils::captureCallStack()));
                }

                this->m_isolationLevel = level;
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("STI1A2B3C4D5",
                                                       std::string("setTransactionIsolation failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("STI1A2B3C4D6",
                                                       "setTransactionIsolation failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            auto result = setTransactionIsolation(std::nothrow, level);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<TransactionIsolationLevel, DBException> SQLiteDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
        {
            try
            {
                return m_isolationLevel;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GTI1A2B3C4D5",
                                                       std::string("getTransactionIsolation failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GTI1A2B3C4D6",
                                                       "getTransactionIsolation failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        TransactionIsolationLevel SQLiteDBConnection::getTransactionIsolation()
        {
            auto result = getTransactionIsolation(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string SQLiteDBConnection::getURL() const
        {
            return m_url;
        }

        void SQLiteDBConnection::registerStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            m_activeStatements.insert(stmt);
        }

        void SQLiteDBConnection::unregisterStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // Remove expired weak_ptrs and the specified one
            for (auto it = m_activeStatements.begin(); it != m_activeStatements.end();)
            {
                auto locked = it->lock();
                auto stmtLocked = stmt.lock();
                if (!locked || (stmtLocked && locked.get() == stmtLocked.get()))
                {
                    it = m_activeStatements.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // SQLiteDBDriver implementation
        // Static member variables to ensure SQLite is configured once
        std::atomic<bool> SQLiteDBDriver::s_initialized{false};
        std::mutex SQLiteDBDriver::s_initMutex;

        SQLiteDBDriver::SQLiteDBDriver()
        {
            // Thread-safe single initialization pattern
            bool alreadyInitialized = s_initialized.load(std::memory_order_acquire);
            if (!alreadyInitialized)
            {
                // Use a mutex to ensure only one thread performs initialization
                std::lock_guard<std::mutex> lock(s_initMutex);

                // Double-check that initialization hasn't happened
                // while we were waiting for the lock
                if (!s_initialized.load(std::memory_order_relaxed))
                {
                    // Configure SQLite for thread safety before initialization
                    int configResult = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
                    if (configResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("9E0F1G2H3I4J: Error configuring SQLite for thread safety: "
                                     << sqlite3_errstr(configResult));
                    }

                    // Initialize SQLite
                    int initResult = sqlite3_initialize();
                    if (initResult != SQLITE_OK)
                    {
                        SQLITE_DEBUG("5K6L7M8N9O0P: Error initializing SQLite: "
                                     << sqlite3_errstr(initResult));
                    }

                    // Mark as initialized
                    s_initialized.store(true, std::memory_order_release);
                }
            }

            // Set up memory management (per instance)
            sqlite3_soft_heap_limit64(8 * 1024 * 1024); // 8MB soft limit
        }

        SQLiteDBDriver::~SQLiteDBDriver()
        {
            try
            {
                // Release as much memory as possible
                [[maybe_unused]]
                int releasedMemory = sqlite3_release_memory(INT_MAX);
                SQLITE_DEBUG("Released " << releasedMemory << " bytes of SQLite memory during driver shutdown");

                // Call sqlite3_shutdown to release all resources
                int shutdownResult = sqlite3_shutdown();
                if (shutdownResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("1Q2R3S4T5U6V: Error shutting down SQLite: "
                                 << sqlite3_errstr(shutdownResult));
                }

                // Sleep a bit to ensure all resources are properly released
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (const std::exception &e)
            {
                SQLITE_DEBUG("7W8X9Y0Z1A2B: Exception during SQLite driver shutdown: " << e.what());
            }
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> SQLiteDBDriver::connectRelational(
            std::nothrow_t,
            const std::string &url,
            [[maybe_unused]] const std::string &,
            [[maybe_unused]] const std::string &,
            const std::map<std::string, std::string> &options) noexcept
        {
            try
            {
                std::string database;

                if (acceptsURL(url))
                {
                    if (!parseURL(url, database))
                    {
                        return cpp_dbc::unexpected(DBException("5C6D7E8F9G0H", "Invalid SQLite connection URL: " + url,
                                                               system_utils::captureCallStack()));
                    }
                }
                else
                {
                    size_t dbStart = url.find("://");
                    if (dbStart != std::string::npos)
                    {
                        database = url.substr(dbStart + 3);
                    }
                    else
                    {
                        return cpp_dbc::unexpected(DBException("1I2J3K4L5M6N", "Invalid SQLite connection URL: " + url,
                                                               system_utils::captureCallStack()));
                    }
                }

                auto connection = std::make_shared<SQLiteDBConnection>(database, options);
                return std::shared_ptr<RelationalDBConnection>(connection);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("CR1A1B2C3D4E",
                                                       std::string("connectRelational failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("CR1A1B2C3D4F",
                                                       "connectRelational failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<RelationalDBConnection> SQLiteDBDriver::connectRelational(const std::string &url,
                                                                                  [[maybe_unused]] const std::string &user,
                                                                                  [[maybe_unused]] const std::string &password,
                                                                                  const std::map<std::string, std::string> &options)
        {
            auto result = connectRelational(std::nothrow, url, user, password, options);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool SQLiteDBDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 18) == "cpp_dbc:sqlite://";
        }

        bool SQLiteDBDriver::parseURL(const std::string &url, std::string &database)
        {
            // Parse URL of format: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract database path
            database = url.substr(18);
            return true;
        }

        std::string SQLiteDBDriver::getName() const noexcept
        {
            return "sqlite";
        }

        // BLOB support methods for SQLiteDBResultSet
        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("B1C2D3E4F5G6", "Invalid column index or row position for getBlob",
                                                           system_utils::captureCallStack()));
                }

                // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(std::shared_ptr<sqlite3>()));
                }

                // Check if the column is a BLOB type
                if (sqlite3_column_type(stmt, idx) != SQLITE_BLOB)
                {
                    return cpp_dbc::unexpected(DBException("H7I8J9K0L1M2", "Column is not a BLOB type",
                                                           system_utils::captureCallStack()));
                }

                // Get the binary data
                const void *blobData = sqlite3_column_blob(stmt, idx);
                int blobSize = sqlite3_column_bytes(stmt, idx);

                // Create a vector with the data
                std::vector<uint8_t> data;
                if (blobData && blobSize > 0)
                {
                    data.resize(blobSize);
                    std::memcpy(data.data(), blobData, blobSize);
                }

                // Get a shared_ptr to the connection
                auto conn = m_connection.lock();
                if (!conn)
                {
                    return cpp_dbc::unexpected(DBException("N3O4P5Q6R7S8", "Connection is no longer valid",
                                                           system_utils::captureCallStack()));
                }

                // Create a new BLOB object with the data (pass shared_ptr for safe reference)
                return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(conn->m_db, data));
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBLO1A2B3C4D",
                                                       std::string("getBlob failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBLO1A2B3C4E",
                                                       "getBlob failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<Blob> SQLiteDBResultSet::getBlob(size_t columnIndex)
        {
            auto result = getBlob(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("T9U0V1W2X3Y4", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBLN1A2B3C4D",
                                                       std::string("getBlob failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBLN1A2B3C4E",
                                                       "getBlob failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<Blob> SQLiteDBResultSet::getBlob(const std::string &columnName)
        {
            auto result = getBlob(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("CEE30385E0BB", "Invalid column index or row position for getBinaryStream",
                                                           system_utils::captureCallStack()));
                }

                // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    // Return an empty stream
                    return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(nullptr, 0));
                }

                // Get the binary data
                const void *blobData = sqlite3_column_blob(stmt, idx);
                int blobSize = sqlite3_column_bytes(stmt, idx);

                // Create a new input stream with the data
                return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(blobData, blobSize));
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBIS1A2B3C4D",
                                                       std::string("getBinaryStream failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBIS1A2B3C4E",
                                                       "getBinaryStream failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<InputStream> SQLiteDBResultSet::getBinaryStream(size_t columnIndex)
        {
            auto result = getBinaryStream(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("F1G2H3I4J5K6", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBIN1A2B3C4D",
                                                       std::string("getBinaryStream failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBIN1A2B3C4E",
                                                       "getBinaryStream failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::shared_ptr<InputStream> SQLiteDBResultSet::getBinaryStream(const std::string &columnName)
        {
            auto result = getBinaryStream(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                sqlite3_stmt *stmt = getStmt();
                if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
                {
                    return cpp_dbc::unexpected(DBException("L7M8N9O0P1Q2", "Invalid column index or row position for getBytes",
                                                           system_utils::captureCallStack()));
                }

                // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);

                if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
                {
                    return std::vector<uint8_t>{};
                }

                // Get the binary data
                const void *blobData = sqlite3_column_blob(stmt, idx);
                int blobSize = sqlite3_column_bytes(stmt, idx);

                // Create a vector with the data
                std::vector<uint8_t> data;
                if (blobData && blobSize > 0)
                {
                    data.resize(blobSize);
                    std::memcpy(data.data(), blobData, blobSize);
                }

                return data;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBYT1A2B3C4D",
                                                       std::string("getBytes failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBYT1A2B3C4E",
                                                       "getBytes failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::vector<uint8_t> SQLiteDBResultSet::getBytes(size_t columnIndex)
        {
            auto result = getBytes(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected(DBException("R3S4T5U6V7W8", "Column not found: " + columnName,
                                                           system_utils::captureCallStack()));
                }

                return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("GBYN1A2B3C4D",
                                                       std::string("getBytes failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("GBYN1A2B3C4E",
                                                       "getBytes failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        std::vector<uint8_t> SQLiteDBResultSet::getBytes(const std::string &columnName)
        {
            auto result = getBytes(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // BLOB support methods for SQLiteDBPreparedStatement
        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("X9Y0Z1A2B3C4", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                // Get the SQLite connection safely (throws if connection is closed)
                sqlite3 *dbPtr = getSQLiteConnection();

                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("D5E6F7G8H9I0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("J1K2L3M4N5O6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                m_blobObjects[parameterIndex - 1] = x;

                if (!x)
                {
                    auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                    if (!nullResult)
                    {
                        return cpp_dbc::unexpected(nullResult.error());
                    }
                    return {};
                }

                std::vector<uint8_t> data = x->getBytes(0, x->length());
                m_blobValues[parameterIndex - 1] = std::move(data);

                int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                               m_blobValues[parameterIndex - 1].data(),
                                               static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                               SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("V3W4X5Y6Z7A8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SBLO1A2B3C4D",
                                                       std::string("setBlob failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SBLO1A2B3C4E",
                                                       "setBlob failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
            auto result = setBlob(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("B9C0D1E2F3G4", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                sqlite3 *dbPtr = getSQLiteConnection();

                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("H5I6J7K8L9M0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("N1O2P3Q4R5S6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                m_streamObjects[parameterIndex - 1] = x;

                if (!x)
                {
                    auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                    if (!nullResult)
                    {
                        return cpp_dbc::unexpected(nullResult.error());
                    }
                    return {};
                }

                std::vector<uint8_t> data;
                uint8_t buffer[4096];
                int bytesRead;
                while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
                {
                    data.insert(data.end(), buffer, buffer + bytesRead);
                }

                m_blobValues[parameterIndex - 1] = std::move(data);

                int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                               m_blobValues[parameterIndex - 1].data(),
                                               static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                               SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("Z3A4B5C6D7E8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SBIS1A2B3C4D",
                                                       std::string("setBinaryStream failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SBIS1A2B3C4E",
                                                       "setBinaryStream failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
            auto result = setBinaryStream(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("F9G0H1I2J3K4", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                sqlite3 *dbPtr = getSQLiteConnection();

                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("L5M6N7O8P9Q0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("R1S2T3U4V5W6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                m_streamObjects[parameterIndex - 1] = x;

                if (!x)
                {
                    auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                    if (!nullResult)
                    {
                        return cpp_dbc::unexpected(nullResult.error());
                    }
                    return {};
                }

                std::vector<uint8_t> data;
                data.reserve(length);
                uint8_t buffer[4096];
                size_t totalBytesRead = 0;
                int bytesRead;
                while (totalBytesRead < length && (bytesRead = x->read(buffer, static_cast<int>(std::min(sizeof(buffer), length - totalBytesRead)))) > 0)
                {
                    data.insert(data.end(), buffer, buffer + bytesRead);
                    totalBytesRead += bytesRead;
                }

                m_blobValues[parameterIndex - 1] = std::move(data);

                int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                               m_blobValues[parameterIndex - 1].data(),
                                               static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                               SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("D3E4F5G6H7I8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SBISL1A2B3C4",
                                                       std::string("setBinaryStream failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SBISL1A2B3C5",
                                                       "setBinaryStream failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
            auto result = setBinaryStream(std::nothrow, parameterIndex, x, length);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
        {
            return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
        }

        void SQLiteDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
            auto result = setBytes(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (m_closed || !m_stmt)
                {
                    return cpp_dbc::unexpected(DBException("H3I4J5K6L7M8", "Statement is closed",
                                                           system_utils::captureCallStack()));
                }

                sqlite3 *dbPtr = getSQLiteConnection();

                if (parameterIndex <= 0)
                {
                    return cpp_dbc::unexpected(DBException("N9O0P1Q2R3S4", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                           system_utils::captureCallStack()));
                }

                int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
                if (parameterIndex > paramCount)
                {
                    return cpp_dbc::unexpected(DBException("T5U6V7W8X9Y0", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                           system_utils::captureCallStack()));
                }

                if (!x)
                {
                    auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                    if (!nullResult)
                    {
                        return cpp_dbc::unexpected(nullResult.error());
                    }
                    return {};
                }

                m_blobValues[parameterIndex - 1].resize(length);
                std::memcpy(m_blobValues[parameterIndex - 1].data(), x, length);

                int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                               m_blobValues[parameterIndex - 1].data(),
                                               static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                               SQLITE_STATIC);
                if (result != SQLITE_OK)
                {
                    return cpp_dbc::unexpected(DBException("F7G8H9I0J1K2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                           system_utils::captureCallStack()));
                }
                return {};
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("SBYT1A2B3C4D",
                                                       std::string("setBytes failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("SBYT1A2B3C4E",
                                                       "setBytes failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        void SQLiteDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
            auto result = setBytes(std::nothrow, parameterIndex, x, length);
            if (!result)
            {
                throw result.error();
            }
        }

    } // namespace cpp_dbc

}
#endif // USE_SQLITE
