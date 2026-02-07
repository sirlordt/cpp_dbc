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

 @file prepared_statement_01.cpp
 @brief SQLite database driver implementation - SQLiteDBPreparedStatement (constructor, destructor, throwing methods)

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

    // SQLiteDBPreparedStatement implementation
    // Private methods
    void SQLiteDBPreparedStatement::notifyConnClosing()
    {
        // Connection is closing, release the statement pointer without calling sqlite3_finalize
        // since the connection is already being destroyed and will clean up all statements
        m_stmt.release(); // Release without calling deleter
        m_closed = true;
    }

    sqlite3 *SQLiteDBPreparedStatement::getSQLiteConnection() const
    {
        auto conn = m_db.lock();
        if (!conn)
        {
            throw DBException("471F2E35F962", "SQLite connection has been closed", system_utils::captureCallStack());
        }
        return conn.get();
    }

    // Public methods - Constructor and destructor
#if DB_DRIVER_THREAD_SAFE
    SQLiteDBPreparedStatement::SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, SharedConnMutex connMutex, const std::string &sql)
        : m_db(db), m_sql(sql), m_stmt(nullptr), m_closed(false), m_blobValues(), m_blobObjects(), m_streamObjects(),
          m_connMutex(std::move(connMutex))
    {
#else
    SQLiteDBPreparedStatement::SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, const std::string &sql)
        : m_db(db), m_sql(sql), m_stmt(nullptr), m_closed(false), m_blobValues(), m_blobObjects(), m_streamObjects()
    {
#endif
        sqlite3 *dbPtr = getSQLiteConnection();

        sqlite3_stmt *rawStmt = nullptr;
        int result = sqlite3_prepare_v2(dbPtr, m_sql.c_str(), -1, &rawStmt, nullptr);
        if (result != SQLITE_OK)
        {
            throw DBException("U0A1B2C3D4E5", "Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(dbPtr)),
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

    // Throwing API methods
    void SQLiteDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = setInt(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setLong(int parameterIndex, int64_t value)
    {
        auto result = setLong(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
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

    void SQLiteDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = setString(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
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

    void SQLiteDBPreparedStatement::setNull(int parameterIndex, [[maybe_unused]] Types type)
    {
        auto result = setNull(std::nothrow, parameterIndex, type);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = setDate(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = setTimestamp(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBPreparedStatement::setTime(int parameterIndex, const std::string &value)
    {
        auto result = setTime(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
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

    void SQLiteDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result)
        {
            throw result.error();
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

    void SQLiteDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x);
        if (!result)
        {
            throw result.error();
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

    std::shared_ptr<RelationalDBResultSet> SQLiteDBPreparedStatement::executeQuery()
    {
        auto result = executeQuery(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

    bool SQLiteDBPreparedStatement::execute()
    {
        auto result = execute(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
