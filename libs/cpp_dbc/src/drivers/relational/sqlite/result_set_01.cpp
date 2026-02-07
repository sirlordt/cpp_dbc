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
    // Because of this, SQLiteDBResultSet MUST share the same mutex as SQLiteDBConnection
    // to prevent race conditions when multiple threads access the same connection
    // (e.g., one thread iterating results while another does pool validation).
    //
#if DB_DRIVER_THREAD_SAFE
    SQLiteDBResultSet::SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteDBConnection> conn, SharedConnMutex connMutex)
        : m_stmt(stmt), m_ownStatement(ownStatement), m_rowPosition(0), m_rowCount(0), m_fieldCount(0),
          m_columnNames(), m_columnMap(), m_hasData(false), m_closed(false), m_connection(conn),
          m_connMutex(std::move(connMutex))
    {
#else
    SQLiteDBResultSet::SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteDBConnection> conn)
        : m_stmt(stmt), m_ownStatement(ownStatement), m_rowPosition(0), m_rowCount(0), m_fieldCount(0),
          m_columnNames(), m_columnMap(), m_hasData(false), m_closed(false), m_connection(conn)
    {
#endif
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

    bool SQLiteDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    bool SQLiteDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    int SQLiteDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    int64_t SQLiteDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    int64_t SQLiteDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    double SQLiteDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    std::string SQLiteDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    bool SQLiteDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    bool SQLiteDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getDate(const std::string &columnName)
    {
        auto result = getDate(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = getTimestamp(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getTime(size_t columnIndex)
    {
        auto result = getTime(std::nothrow, columnIndex);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBResultSet::getTime(const std::string &columnName)
    {
        auto result = getTime(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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
        // CRITICAL: Must use shared connection mutex because sqlite3_reset() and
        // sqlite3_finalize() access the sqlite3* connection handle internally.
        // See class documentation for why SQLite needs SharedConnMutex but MySQL/PostgreSQL don't.
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        return m_rowPosition == 0 && !m_hasData;
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

    std::shared_ptr<Blob> SQLiteDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = getBlob(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    std::shared_ptr<InputStream> SQLiteDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    std::vector<uint8_t> SQLiteDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
