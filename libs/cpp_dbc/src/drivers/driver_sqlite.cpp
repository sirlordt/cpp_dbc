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
 @brief Tests for SQLite database operations

*/

#include "cpp_dbc/drivers/driver_sqlite.hpp"
#include "cpp_dbc/drivers/sqlite_blob.hpp"
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

        // SQLiteResultSet implementation
        SQLiteResultSet::SQLiteResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteConnection> conn)
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

        SQLiteResultSet::~SQLiteResultSet()
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

        bool SQLiteResultSet::next()
        {
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
                throw DBException("A1B2C3D4E5F6", "Error stepping through SQLite result set: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))),
                                  system_utils::captureCallStack());
            }
        }

        bool SQLiteResultSet::isBeforeFirst()
        {
            return m_rowPosition == 0;
        }

        bool SQLiteResultSet::isAfterLast()
        {
            return m_rowPosition > 0 && !m_hasData;
        }

        uint64_t SQLiteResultSet::getRow()
        {
            return m_rowPosition;
        }

        int SQLiteResultSet::getInt(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("7A8B9C0D1E2F", "Invalid column index or row position",
                                  system_utils::captureCallStack());
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return sqlite3_column_int(stmt, idx);
        }

        int SQLiteResultSet::getInt(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("3G4H5I6J7K8L", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long SQLiteResultSet::getLong(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("DDAABD02C9D3", "Invalid column index or row position",
                                  system_utils::captureCallStack());
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0;
            }

            return sqlite3_column_int64(stmt, idx);
        }

        long SQLiteResultSet::getLong(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("20C1324B8D71", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getLong(it->second + 1);
        }

        double SQLiteResultSet::getDouble(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("9M0N1O2P3Q4R", "Invalid column index or row position",
                                  system_utils::captureCallStack());
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0.0;
            }

            return sqlite3_column_double(stmt, idx);
        }

        double SQLiteResultSet::getDouble(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("5S6T7U8V9W0X", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getDouble(it->second + 1);
        }

        std::string SQLiteResultSet::getString(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("1Y2Z3A4B5C6D", "Invalid column index or row position",
                                  system_utils::captureCallStack());
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return "";
            }

            const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
            return text ? std::string(text) : "";
        }

        std::string SQLiteResultSet::getString(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("93A82C42FA7B", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getString(it->second + 1);
        }

        bool SQLiteResultSet::getBoolean(size_t columnIndex)
        {
            return getInt(columnIndex) != 0;
        }

        bool SQLiteResultSet::getBoolean(const std::string &columnName)
        {
            return getInt(columnName) != 0;
        }

        bool SQLiteResultSet::isNull(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("407EBCBBE843", "Invalid column index or row position",
                                  system_utils::captureCallStack());
            }

            int idx = static_cast<int>(columnIndex - 1);
            return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
        }

        bool SQLiteResultSet::isNull(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("8BAE4B58A947", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> SQLiteResultSet::getColumnNames()
        {
            return m_columnNames;
        }

        size_t SQLiteResultSet::getColumnCount()
        {
            return m_fieldCount;
        }

        void SQLiteResultSet::close()
        {
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

        // SQLitePreparedStatement implementation
        sqlite3 *SQLitePreparedStatement::getSQLiteConnection() const
        {
            auto conn = m_db.lock();
            if (!conn)
            {
                throw DBException("471F2E35F962", "SQLite connection has been closed", system_utils::captureCallStack());
            }
            return conn.get();
        }

        SQLitePreparedStatement::SQLitePreparedStatement(std::weak_ptr<sqlite3> db, const std::string &sql)
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

        SQLitePreparedStatement::~SQLitePreparedStatement()
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

        void SQLitePreparedStatement::close()
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

        void SQLitePreparedStatement::notifyConnClosing()
        {
            // Connection is closing, release the statement pointer without calling sqlite3_finalize
            // since the connection is already being destroyed and will clean up all statements
            m_stmt.release(); // Release without calling deleter
            m_closed = true;
        }

        void SQLitePreparedStatement::setInt(int parameterIndex, int value)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("9Q0R1S2T3U4V", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("5W6X7Y8Z9A0B", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("1C2D3E4F5G6H", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Debug output - comentado para reducir la salida de Valgrind
            // std::cout << "Binding integer parameter " << parameterIndex << " with value " << value
            //           << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("7I8J9K0L1M2N", "Failed to bind integer parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setLong(int parameterIndex, long value)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("3O4P5Q6R7S8T", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("9U0V1W2X3Y4Z", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            int result = sqlite3_bind_int64(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("5A6B7C8D9E0F", "Failed to bind long parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("1G2H3I4J5K6L", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("7M8N9O0P1Q2R", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("3S4T5U6V7W8X", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Debug output - comentado para reducir la salida de Valgrind
            // std::cout << "Binding double parameter " << parameterIndex << " with value " << value
            //           << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_double(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("9Y0Z1A2B3C4D", "Failed to bind double parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("5E6F7G8H9I0J", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("1K2L3M4N5O6P", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("7Q8R9S0T1U2V", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Debug output - comentado para reducir la salida de Valgrind
            // std::cout << "Binding string parameter " << parameterIndex << " with value '" << value
            //           << "' (statement has " << paramCount << " parameters)" << std::endl;

            // SQLITE_TRANSIENT tells SQLite to make its own copy of the data
            int result = sqlite3_bind_text(m_stmt.get(), parameterIndex, value.c_str(), -1, SQLITE_TRANSIENT);
            if (result != SQLITE_OK)
            {
                throw DBException("3W4X5Y6Z7A8B", "Failed to bind string parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value='" + value + "'" + ", result=" + std::to_string(result) + ")",
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("9C0D1E2F3G4H", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("5I6J7K8L9M0N", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("1O2P3Q4R5S6T", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Debug output - comentado para reducir la salida de Valgrind
            // std::cout << "Binding boolean parameter " << parameterIndex << " with value " << (value ? "true" : "false")
            //           << " (statement has " << paramCount << " parameters)" << std::endl;

            int intValue = value ? 1 : 0;
            int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, intValue);
            if (result != SQLITE_OK)
            {
                throw DBException("7U8V9W0X1Y2Z", "Failed to bind boolean parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + (value ? "true" : "false") + ", result=" + std::to_string(result) + ")",
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setNull(int parameterIndex, [[maybe_unused]] Types type)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("3A4B5C6D7E8F", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("9G0H1I2J3K4L", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
            if (result != SQLITE_OK)
            {
                throw DBException("5M6N7O8P9Q0R", "Failed to bind null parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            // SQLite doesn't have a specific date type, so we store it as text
            setString(parameterIndex, value);
        }

        void SQLitePreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            // SQLite doesn't have a specific timestamp type, so we store it as text
            setString(parameterIndex, value);
        }

        std::shared_ptr<ResultSet> SQLitePreparedStatement::executeQuery()
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("1S2T3U4V5W6X", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                throw DBException("7Y8Z9A0B1C2D", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                  system_utils::captureCallStack());
            }

            // Debug output - comentado para reducir la salida de Valgrind
            // Commented out to avoid unused variable warning
            // int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            // std::cout << "Executing query with " << paramCount << " parameters" << std::endl;

            // Print the SQL statement - comentado para reducir la salida de Valgrind
            // std::cout << "SQL: " << sql << std::endl;

            // Create a result set that will NOT own the statement (false = non-owning)
            // We don't execute the first step here - let the ResultSet do it when next() is called
            // Note: We pass nullptr for connection since this ResultSet doesn't own the statement
            // and doesn't need to check connection validity for finalization
            auto resultSet = std::make_shared<SQLiteResultSet>(m_stmt.get(), false, nullptr);

            // Note: We don't clear bindings here because the ResultSet needs them
            // The bindings will be cleared when the statement is reset for the next use

            return resultSet;
        }

        uint64_t SQLitePreparedStatement::executeUpdate()
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("3E4F5G6H7I8J", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                throw DBException("9K0L1M2N3O4P", "Failed to reset statement: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                  system_utils::captureCallStack());
            }

            // Execute the statement
            int result = sqlite3_step(m_stmt.get());
            if (result != SQLITE_DONE)
            {
                throw DBException("5Q6R7S8T9U0V", "Failed to execute update: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(result) + ")",
                                  system_utils::captureCallStack());
            }

            // Get the number of affected rows
            uint64_t changes = sqlite3_changes(dbPtr);

            // Reset the statement after execution to allow reuse
            resetResult = sqlite3_reset(m_stmt.get());
            if (resetResult != SQLITE_OK)
            {
                throw DBException("1W2X3Y4Z5A6B", "Failed to reset statement after execution: " + std::string(sqlite3_errmsg(dbPtr)) + " (result=" + std::to_string(resetResult) + ")",
                                  system_utils::captureCallStack());
            }

            // Note: We don't clear bindings here anymore
            // This allows reusing the statement with the same parameters

            return changes;
        }

        bool SQLitePreparedStatement::execute()
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("7C8D9E0F1G2H", "Statement is closed",
                                  system_utils::captureCallStack());
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
                throw DBException("3I4J5K6L7M8N", "Failed to execute statement: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        // SQLiteConnection implementation

        SQLiteConnection::SQLiteConnection(const std::string &database,
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

        SQLiteConnection::~SQLiteConnection()
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

        void SQLiteConnection::close()
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

        bool SQLiteConnection::isClosed()
        {
            return m_closed;
        }

        void SQLiteConnection::returnToPool()
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

        bool SQLiteConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<PreparedStatement> SQLiteConnection::prepareStatement(const std::string &sql)
        {
            if (m_closed || !m_db)
            {
                throw DBException("5U6V7W8X9Y0Z", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            // Pass weak_ptr to the PreparedStatement so it can safely detect when connection is closed
            auto stmt = std::make_shared<SQLitePreparedStatement>(std::weak_ptr<sqlite3>(m_db), sql);
            registerStatement(std::weak_ptr<SQLitePreparedStatement>(stmt));
            return stmt;
        }

        std::shared_ptr<ResultSet> SQLiteConnection::executeQuery(const std::string &sql)
        {
            try
            {
                SQLITE_DEBUG("SQLiteConnection::executeQuery - Executing query: " << sql);

                if (m_closed || !m_db)
                {
                    SQLITE_DEBUG("SQLiteConnection::executeQuery - Connection is closed");
                    throw DBException("1A2B3C4D5E6F", "Connection is closed",
                                      system_utils::captureCallStack());
                }

                SQLITE_DEBUG("SQLiteConnection::executeQuery - Preparing statement");
                sqlite3_stmt *stmt = nullptr;
                int result = sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &stmt, nullptr);
                if (result != SQLITE_OK)
                {
                    std::string errorMsg = sqlite3_errmsg(m_db.get());
                    SQLITE_DEBUG("SQLiteConnection::executeQuery - Failed to prepare query: " << errorMsg);
                    throw DBException("7G8H9I0J1K2L", "Failed to prepare query: " + errorMsg,
                                      system_utils::captureCallStack());
                }

                SQLITE_DEBUG("SQLiteConnection::executeQuery - Statement prepared successfully");

                if (!stmt)
                {
                    SQLITE_DEBUG("SQLiteConnection::executeQuery - Statement is null after successful preparation");
                    throw DBException("1DEA86F65A95", "SQLiteConnection::executeQuery - Statement is null after successful preparation",
                                      system_utils::captureCallStack());
                }

                SQLITE_DEBUG("SQLiteConnection::executeQuery - Creating SQLiteResultSet");
                // Pass a shared_ptr to this connection to the result set
                // The ResultSet will check if connection is still valid before finalizing
                auto self = std::dynamic_pointer_cast<SQLiteConnection>(shared_from_this());
                auto resultSet = std::make_shared<SQLiteResultSet>(stmt, true, self);
                SQLITE_DEBUG("SQLiteConnection::executeQuery - SQLiteResultSet created successfully");

                return resultSet;
            }
            catch (const DBException &e)
            {
                SQLITE_DEBUG("SQLiteConnection::executeQuery - DBException: " << e.what());
                throw;
            }
            catch (const std::exception &e)
            {
                SQLITE_DEBUG("SQLiteConnection::executeQuery - std::exception: " << e.what());
                throw DBException("5CD341#BCECB3", "SQLiteConnection::executeQuery exception: " + std::string(e.what()),
                                  system_utils::captureCallStack());
            }
            catch (...)
            {
                SQLITE_DEBUG("SQLiteConnection::executeQuery - Unknown exception");
                throw DBException("B7CD9911C7E1", "SQLiteConnection::executeQuery unknown exception",
                                  system_utils::captureCallStack());
            }
        }

        uint64_t SQLiteConnection::executeUpdate(const std::string &sql)
        {
            if (m_closed || !m_db)
            {
                throw DBException("3M4N5O6P7Q8R", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            char *errmsg = nullptr;
            int result = sqlite3_exec(m_db.get(), sql.c_str(), nullptr, nullptr, &errmsg);

            if (result != SQLITE_OK)
            {
                std::string error = errmsg ? errmsg : "Unknown error";
                sqlite3_free(errmsg);
                throw DBException("9S0T1U2V3W4X", "Failed to execute update: " + error,
                                  system_utils::captureCallStack());
            }

            return static_cast<uint64_t>(sqlite3_changes(m_db.get()));
        }

        void SQLiteConnection::setAutoCommit(bool autoCommit)
        {
            if (m_closed || !m_db)
            {
                throw DBException("5Y6Z7A8B9C0D", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            /*
            if (m_transactionActive && autoCommit)
            {
                throw DBException("AC79D30BE5F1", "Cannot enable autocommit while a transaction is active. Commit or rollback first.",
                                  system_utils::captureCallStack());
            }
            */

            if (this->m_autoCommit != autoCommit)
            {
                // Simplemente actualizar el flag sin iniciar transacción
                this->m_autoCommit = autoCommit;
            }
        }

        bool SQLiteConnection::beginTransaction()
        {
            if (m_closed || !m_db)
            {
                throw DBException("FD82C45A3E09", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            if (m_transactionActive)
            {
                return false;
            }

            if (m_autoCommit)
            {
                m_autoCommit = false;
            }

            try
            {
                executeUpdate("BEGIN TRANSACTION");
                m_transactionActive = true;
                return true;
            }
            catch (const DBException &e)
            {
                SQLITE_DEBUG("BC64F52A8D10: Error starting transaction: " << e.what());
                return false;
            }
        }

        bool SQLiteConnection::transactionActive()
        {
            return m_transactionActive;
        }

        bool SQLiteConnection::getAutoCommit()
        {
            return m_autoCommit;
        }

        void SQLiteConnection::commit()
        {
            if (m_closed || !m_db)
            {
                throw DBException("1E2F3G4H5I6J", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            executeUpdate("COMMIT");

            m_transactionActive = false;
            m_autoCommit = true;
        }

        void SQLiteConnection::rollback()
        {
            if (m_closed || !m_db)
            {
                throw DBException("7K8L9M0N1O2P", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            executeUpdate("ROLLBACK");

            m_transactionActive = false;
            m_autoCommit = true;
        }

        void SQLiteConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            if (m_closed || !m_db)
            {
                throw DBException("3Q4R5S6T7U8V", "Connection is closed",
                                  system_utils::captureCallStack());
            }

            // SQLite only supports SERIALIZABLE isolation level
            if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
            {
                throw DBException("9W0X1Y2Z3A4B", "SQLite only supports SERIALIZABLE isolation level",
                                  system_utils::captureCallStack());
            }

            this->m_isolationLevel = level;
        }

        TransactionIsolationLevel SQLiteConnection::getTransactionIsolation()
        {
            return m_isolationLevel;
        }

        std::string SQLiteConnection::getURL() const
        {
            return m_url;
        }

        void SQLiteConnection::registerStatement(std::weak_ptr<SQLitePreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            m_activeStatements.insert(stmt);
        }

        void SQLiteConnection::unregisterStatement(std::weak_ptr<SQLitePreparedStatement> stmt)
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

        // SQLiteDriver implementation
        // Static member variables to ensure SQLite is configured once
        std::atomic<bool> SQLiteDriver::s_initialized{false};
        std::mutex SQLiteDriver::s_initMutex;

        SQLiteDriver::SQLiteDriver()
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

        SQLiteDriver::~SQLiteDriver()
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

        std::shared_ptr<Connection> SQLiteDriver::connect(const std::string &url,
                                                          [[maybe_unused]] const std::string &,
                                                          [[maybe_unused]] const std::string &,
                                                          const std::map<std::string, std::string> &options)
        {
            try
            {
                SQLITE_DEBUG("SQLiteDriver::connect - Starting connection process for URL: " << url);
                std::string database;

                // Check if the URL is in the expected format
                if (acceptsURL(url))
                {
                    SQLITE_DEBUG("SQLiteDriver::connect - URL format accepted");
                    // URL is in the format cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
                    if (!parseURL(url, database))
                    {
                        SQLITE_DEBUG("SQLiteDriver::connect - Failed to parse URL");
                        throw DBException("5C6D7E8F9G0H", "Invalid SQLite connection URL: " + url,
                                          system_utils::captureCallStack());
                    }
                }
                else
                {
                    SQLITE_DEBUG("SQLiteDriver::connect - URL format not recognized, trying direct extraction");
                    // Try to extract database path directly
                    size_t dbStart = url.find("://");
                    if (dbStart != std::string::npos)
                    {
                        database = url.substr(dbStart + 3);
                        SQLITE_DEBUG("SQLiteDriver::connect - Extracted database path: " << database);
                    }
                    else
                    {
                        SQLITE_DEBUG("SQLiteDriver::connect - Could not extract database path from URL");
                        throw DBException("1I2J3K4L5M6N", "Invalid SQLite connection URL: " + url,
                                          system_utils::captureCallStack());
                    }
                }

                SQLITE_DEBUG("SQLiteDriver::connect - Connecting to SQLite database: " << database);

                // Verificar si la base de datos es :memory: o un archivo
                if (database == ":memory:")
                {
                    SQLITE_DEBUG("SQLiteDriver::connect - Using in-memory SQLite database");
                }
                else
                {
                    SQLITE_DEBUG("SQLiteDriver::connect - Using file-based SQLite database: " << database);

                    // Verificar si el archivo existe o si se puede crear
                    std::ifstream fileCheck(database.c_str());
                    if (!fileCheck)
                    {
                        SQLITE_DEBUG("SQLiteDriver::connect - Database file does not exist, will be created");
                    }
                    else
                    {
                        SQLITE_DEBUG("SQLiteDriver::connect - Database file exists");
                        fileCheck.close();
                    }
                }

                // It's crucial to use make_shared for shared_from_this() to work properly
                SQLITE_DEBUG("SQLiteDriver::connect - Creating SQLiteConnection object");
                auto connection = std::make_shared<SQLiteConnection>(database, options);
                SQLITE_DEBUG("SQLiteDriver::connect - SQLiteConnection object created successfully");
                return connection;
            }
            catch (const std::exception &e)
            {
                SQLITE_DEBUG("SQLiteDriver::connect - Exception caught: " << e.what());
                throw;
            }
            catch (...)
            {
                SQLITE_DEBUG("SQLiteDriver::connect - Unknown exception caught");
                throw;
            }
        }

        bool SQLiteDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 18) == "cpp_dbc:sqlite://";
        }

        bool SQLiteDriver::parseURL(const std::string &url, std::string &database)
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

        // BLOB support methods for SQLiteResultSet
        std::shared_ptr<Blob> SQLiteResultSet::getBlob(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("B1C2D3E4F5G6", "Invalid column index or row position for getBlob",
                                  system_utils::captureCallStack());
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::make_shared<SQLite::SQLiteBlob>(nullptr);
            }

            // Check if the column is a BLOB type
            if (sqlite3_column_type(stmt, idx) != SQLITE_BLOB)
            {
                throw DBException("H7I8J9K0L1M2", "Column is not a BLOB type",
                                  system_utils::captureCallStack());
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
                throw DBException("N3O4P5Q6R7S8", "Connection is no longer valid",
                                  system_utils::captureCallStack());
            }

            // Create a new BLOB object with the data
            return std::make_shared<SQLite::SQLiteBlob>(conn->m_db.get(), data);
        }

        std::shared_ptr<Blob> SQLiteResultSet::getBlob(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("T9U0V1W2X3Y4", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getBlob(it->second + 1); // +1 because getBlob(int) is 1-based
        }

        std::shared_ptr<InputStream> SQLiteResultSet::getBinaryStream(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("CEE30385E0BB", "Invalid column index or row position for getBinaryStream",
                                  system_utils::captureCallStack());
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                // Return an empty stream
                return std::make_shared<SQLite::SQLiteInputStream>(nullptr, 0);
            }

            // Get the binary data
            const void *blobData = sqlite3_column_blob(stmt, idx);
            int blobSize = sqlite3_column_bytes(stmt, idx);

            // Create a new input stream with the data
            return std::make_shared<SQLite::SQLiteInputStream>(blobData, blobSize);
        }

        std::shared_ptr<InputStream> SQLiteResultSet::getBinaryStream(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("F1G2H3I4J5K6", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getBinaryStream(it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }

        std::vector<uint8_t> SQLiteResultSet::getBytes(size_t columnIndex)
        {
            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("L7M8N9O0P1Q2", "Invalid column index or row position for getBytes",
                                  system_utils::captureCallStack());
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return {};
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

        std::vector<uint8_t> SQLiteResultSet::getBytes(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("R3S4T5U6V7W8", "Column not found: " + columnName,
                                  system_utils::captureCallStack());
            }

            return getBytes(it->second + 1); // +1 because getBytes(int) is 1-based
        }

        // BLOB support methods for SQLitePreparedStatement
        void SQLitePreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("X9Y0Z1A2B3C4", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("D5E6F7G8H9I0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("J1K2L3M4N5O6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Store the blob object to keep it alive
            m_blobObjects[parameterIndex - 1] = x;

            if (!x)
            {
                // Set to NULL
                int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
                if (result != SQLITE_OK)
                {
                    throw DBException("P7Q8R9S0T1U2", "Failed to bind null BLOB: " + std::string(sqlite3_errmsg(dbPtr)),
                                      system_utils::captureCallStack());
                }
                return;
            }

            // Get the blob data
            std::vector<uint8_t> data = x->getBytes(0, x->length());

            // Store the data in our vector to keep it alive
            m_blobValues[parameterIndex - 1] = std::move(data);

            // Bind the BLOB data
            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                throw DBException("V3W4X5Y6Z7A8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("B9C0D1E2F3G4", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("H5I6J7K8L9M0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("N1O2P3Q4R5S6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Store the stream object to keep it alive
            m_streamObjects[parameterIndex - 1] = x;

            if (!x)
            {
                // Set to NULL
                int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
                if (result != SQLITE_OK)
                {
                    throw DBException("T7U8V9W0X1Y2", "Failed to bind null BLOB: " + std::string(sqlite3_errmsg(dbPtr)),
                                      system_utils::captureCallStack());
                }
                return;
            }

            // Read all data from the stream
            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
            }

            // Store the data in our vector to keep it alive
            m_blobValues[parameterIndex - 1] = std::move(data);

            // Bind the BLOB data
            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                throw DBException("Z3A4B5C6D7E8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("F9G0H1I2J3K4", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("L5M6N7O8P9Q0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("R1S2T3U4V5W6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Store the stream object to keep it alive
            m_streamObjects[parameterIndex - 1] = x;

            if (!x)
            {
                // Set to NULL
                int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
                if (result != SQLITE_OK)
                {
                    throw DBException("X7Y8Z9A0B1C2", "Failed to bind null BLOB: " + std::string(sqlite3_errmsg(dbPtr)),
                                      system_utils::captureCallStack());
                }
                return;
            }

            // Read up to 'length' bytes from the stream
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

            // Store the data in our vector to keep it alive
            m_blobValues[parameterIndex - 1] = std::move(data);

            // Bind the BLOB data
            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                throw DBException("D3E4F5G6H7I8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("J9K0L1M2N3O4", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("P5Q6R7S8T9U0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("V1W2X3Y4Z5A6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            // Store the data in our vector to keep it alive
            m_blobValues[parameterIndex - 1] = x;

            // Bind the BLOB data
            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                throw DBException("B7C8D9E0F1G2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

        void SQLitePreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
            if (m_closed || !m_stmt)
            {
                throw DBException("H3I4J5K6L7M8", "Statement is closed",
                                  system_utils::captureCallStack());
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("N9O0P1Q2R3S4", "Invalid parameter index: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                throw DBException("T5U6V7W8X9Y0", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                  system_utils::captureCallStack());
            }

            if (!x)
            {
                // Set to NULL
                int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
                if (result != SQLITE_OK)
                {
                    throw DBException("Z1A2B3C4D5E6", "Failed to bind null BLOB: " + std::string(sqlite3_errmsg(dbPtr)),
                                      system_utils::captureCallStack());
                }
                return;
            }

            // Store the data in our vector to keep it alive
            m_blobValues[parameterIndex - 1].resize(length);
            std::memcpy(m_blobValues[parameterIndex - 1].data(), x, length);

            // Bind the BLOB data
            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                throw DBException("F7G8H9I0J1K2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                  system_utils::captureCallStack());
            }
        }

    } // namespace cpp_dbc

}
#endif // USE_SQLITE