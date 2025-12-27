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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file driver_firebird.cpp
 @brief Firebird database driver implementation

 Required system package: firebird-dev (Debian/Ubuntu) or firebird-devel (RHEL/CentOS/Fedora)
 Install with: sudo apt-get install firebird-dev libfbclient2

*/

#include "cpp_dbc/drivers/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <iostream>

// Debug output is controlled by -DDEBUG_FIREBIRD=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_FIREBIRD) && DEBUG_FIREBIRD) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define FIREBIRD_DEBUG(x) std::cout << "[Firebird] " << x << std::endl
#else
#define FIREBIRD_DEBUG(x)
#endif

namespace cpp_dbc
{
    namespace Firebird
    {
        // Static member initialization
        std::atomic<bool> FirebirdDBDriver::s_initialized{false};
        std::mutex FirebirdDBDriver::s_initMutex;

        // ============================================================================
        // FirebirdDBResultSet Implementation
        // ============================================================================

        FirebirdDBResultSet::FirebirdDBResultSet(FirebirdStmtHandle stmt, XSQLDAHandle sqlda, bool ownStatement,
                                                 std::shared_ptr<FirebirdDBConnection> conn)
            : m_stmt(std::move(stmt)), m_sqlda(std::move(sqlda)), m_ownStatement(ownStatement), m_connection(conn)
        {
            FIREBIRD_DEBUG("FirebirdResultSet::constructor - Creating ResultSet");
            FIREBIRD_DEBUG("  ownStatement: " << (ownStatement ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt valid: " << (m_stmt ? "yes" : "no"));
            if (m_stmt)
            {
                FIREBIRD_DEBUG("  m_stmt handle value: " << *m_stmt);
            }
            FIREBIRD_DEBUG("  m_sqlda valid: " << (m_sqlda ? "yes" : "no"));

            if (m_sqlda)
            {
                m_fieldCount = static_cast<size_t>(m_sqlda->sqld);
                FIREBIRD_DEBUG("  Field count: " << m_fieldCount);
                initializeColumns();
            }
            m_closed = false;
            FIREBIRD_DEBUG("FirebirdResultSet::constructor - Done");
        }

        FirebirdDBResultSet::~FirebirdDBResultSet()
        {
            FIREBIRD_DEBUG("FirebirdResultSet::destructor - Destroying ResultSet");
            close();
            FIREBIRD_DEBUG("FirebirdResultSet::destructor - Done");
        }

        void FirebirdDBResultSet::initializeColumns()
        {
            FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - Starting");
            if (!m_sqlda)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - m_sqlda is null, returning");
                return;
            }

            m_columnNames.clear();
            m_columnMap.clear();
            m_dataBuffers.resize(m_fieldCount);
            m_nullIndicators.resize(m_fieldCount);

            for (size_t i = 0; i < m_fieldCount; ++i)
            {
                XSQLVAR *var = &m_sqlda->sqlvar[i];

                // Get column name - prefer alias name if available, otherwise use column name
                // Firebird stores the alias in aliasname field when AS is used in the query
                std::string colName;
                if (var->aliasname_length > 0)
                {
                    colName = std::string(var->aliasname, static_cast<size_t>(var->aliasname_length));
                }
                else
                {
                    colName = std::string(var->sqlname, static_cast<size_t>(var->sqlname_length));
                }
                m_columnNames.push_back(colName);
                m_columnMap[colName] = i;
                FIREBIRD_DEBUG("  Column " << i << ": " << colName << " (raw_sqltype=" << var->sqltype << ", type=" << (var->sqltype & ~1) << ", nullable=" << (var->sqltype & 1) << ", len=" << var->sqllen << ", scale=" << var->sqlscale << ")");

                // Allocate buffer for data
                size_t bufferSize = static_cast<size_t>(var->sqllen);
                if ((var->sqltype & ~1) == SQL_VARYING)
                {
                    bufferSize += sizeof(short); // For length prefix
                }
                else if ((var->sqltype & ~1) == SQL_BLOB)
                {
                    bufferSize = sizeof(ISC_QUAD);
                }

                m_dataBuffers[i].resize(bufferSize + 1, 0);
                var->sqldata = m_dataBuffers[i].data();
                var->sqlind = &m_nullIndicators[i];
                FIREBIRD_DEBUG("    Buffer " << i << ": size=" << bufferSize << ", sqldata=" << static_cast<void *>(var->sqldata) << ", sqlind=" << static_cast<void *>(var->sqlind) << ", *sqlind=" << *var->sqlind);
            }
            FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - Done");
        }

        std::string FirebirdDBResultSet::getColumnValue(size_t columnIndex) const
        {
            FIREBIRD_DEBUG("getColumnValue: columnIndex=" << columnIndex << ", m_fieldCount=" << m_fieldCount);
            if (columnIndex >= m_fieldCount)
            {
                throw DBException("A7B3C9D2E5F1", "Column index out of range: " + std::to_string(columnIndex),
                                  system_utils::captureCallStack());
            }

            FIREBIRD_DEBUG("  nullIndicator=" << m_nullIndicators[columnIndex]);
            if (m_nullIndicators[columnIndex] < 0)
            {
                FIREBIRD_DEBUG("  returning empty (NULL)");
                return "";
            }

            XSQLVAR *var = &m_sqlda->sqlvar[columnIndex];
            short sqlType = var->sqltype & ~1;
            FIREBIRD_DEBUG("  sqlType=" << sqlType << ", sqllen=" << var->sqllen << ", sqlscale=" << var->sqlscale);
            FIREBIRD_DEBUG("  sqldata=" << static_cast<void *>(var->sqldata));

            switch (sqlType)
            {
            case SQL_TEXT:
            {
                return std::string(var->sqldata, static_cast<size_t>(var->sqllen));
            }
            case SQL_VARYING:
            {
                short len = *reinterpret_cast<short *>(var->sqldata);
                return std::string(var->sqldata + sizeof(short), static_cast<size_t>(len));
            }
            case SQL_SHORT:
            {
                short value = *reinterpret_cast<short *>(var->sqldata);
                if (var->sqlscale < 0)
                {
                    double scaled = value / std::pow(10.0, -var->sqlscale);
                    return std::to_string(scaled);
                }
                return std::to_string(value);
            }
            case SQL_LONG:
            {
                ISC_LONG value = *reinterpret_cast<ISC_LONG *>(var->sqldata);
                if (var->sqlscale < 0)
                {
                    double scaled = value / std::pow(10.0, -var->sqlscale);
                    return std::to_string(scaled);
                }
                return std::to_string(value);
            }
            case SQL_INT64:
            {
                ISC_INT64 value = *reinterpret_cast<ISC_INT64 *>(var->sqldata);
                FIREBIRD_DEBUG("getColumnValue SQL_INT64: columnIndex=" << columnIndex
                                                                        << ", sqldata=" << static_cast<void *>(var->sqldata)
                                                                        << ", sqllen=" << var->sqllen
                                                                        << ", sqlscale=" << var->sqlscale
                                                                        << ", raw_value=" << value);
                if (var->sqlscale < 0)
                {
                    double scaled = static_cast<double>(value) / std::pow(10.0, -var->sqlscale);
                    FIREBIRD_DEBUG("  scaled_value=" << scaled);
                    return std::to_string(scaled);
                }
                return std::to_string(value);
            }
            case SQL_FLOAT:
            {
                float value = *reinterpret_cast<float *>(var->sqldata);
                return std::to_string(value);
            }
            case SQL_DOUBLE:
            case SQL_D_FLOAT:
            {
                double value = *reinterpret_cast<double *>(var->sqldata);
                return std::to_string(value);
            }
            case SQL_TIMESTAMP:
            {
                ISC_TIMESTAMP *ts = reinterpret_cast<ISC_TIMESTAMP *>(var->sqldata);
                struct tm time;
                isc_decode_timestamp(ts, &time);
                char buffer[64];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &time);
                return std::string(buffer);
            }
            case SQL_TYPE_DATE:
            {
                ISC_DATE *date = reinterpret_cast<ISC_DATE *>(var->sqldata);
                struct tm time;
                isc_decode_sql_date(date, &time);
                char buffer[32];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time);
                return std::string(buffer);
            }
            case SQL_TYPE_TIME:
            {
                ISC_TIME *time_val = reinterpret_cast<ISC_TIME *>(var->sqldata);
                struct tm time;
                isc_decode_sql_time(time_val, &time);
                char buffer[32];
                std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &time);
                return std::string(buffer);
            }
            case SQL_BLOB:
            {
                // For BLOB columns, read the content and return as string
                // This is useful for BLOB SUB_TYPE TEXT columns storing JSON or other text
                auto conn = m_connection.lock();
                if (!conn)
                {
                    return "[BLOB]"; // Can't read without connection
                }

                ISC_QUAD *blobId = reinterpret_cast<ISC_QUAD *>(var->sqldata);
                try
                {
                    auto blob = std::make_shared<FirebirdBlob>(conn, *blobId);
                    std::vector<uint8_t> data = blob->getBytes(0, blob->length());
                    return std::string(data.begin(), data.end());
                }
                catch (...)
                {
                    return "[BLOB]"; // Return placeholder on error
                }
            }
            default:
                return "";
            }
        }

        bool FirebirdDBResultSet::next()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("FirebirdResultSet::next - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt valid: " << (m_stmt ? "yes" : "no"));

            if (m_closed)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - ResultSet is closed, returning false");
                return false;
            }

            if (!m_stmt)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - m_stmt is null, returning false");
                return false;
            }

            if (!*m_stmt)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::next - *m_stmt is 0 (invalid handle), returning false");
                return false;
            }

            FIREBIRD_DEBUG("  m_stmt handle value: " << *m_stmt);
            FIREBIRD_DEBUG("  m_sqlda valid: " << (m_sqlda ? "yes" : "no"));
            if (m_sqlda)
            {
                FIREBIRD_DEBUG("  m_sqlda->sqld: " << m_sqlda->sqld);
            }

            ISC_STATUS_ARRAY status;
            isc_stmt_handle *stmtPtr = m_stmt.get();
            FIREBIRD_DEBUG("  Calling isc_dsql_fetch with stmtPtr=" << stmtPtr << ", *stmtPtr=" << *stmtPtr);

            ISC_STATUS fetchStatus = isc_dsql_fetch(status, stmtPtr, SQL_DIALECT_V6, m_sqlda.get());
            FIREBIRD_DEBUG("  isc_dsql_fetch returned: " << fetchStatus);

            if (fetchStatus == 0)
            {
                m_rowPosition++;
                m_hasData = true;
                FIREBIRD_DEBUG("FirebirdResultSet::next - Got row " << m_rowPosition);
                // Debug: print null indicators after fetch
                for (size_t i = 0; i < m_fieldCount; ++i)
                {
                    FIREBIRD_DEBUG("  After fetch - Column " << i << ": nullInd=" << m_nullIndicators[i] << ", sqlind=" << static_cast<void *>(m_sqlda->sqlvar[i].sqlind) << ", *sqlind=" << (m_sqlda->sqlvar[i].sqlind ? *m_sqlda->sqlvar[i].sqlind : -999));
                }
                return true;
            }
            else if (fetchStatus == 100)
            {
                m_hasData = false;
                FIREBIRD_DEBUG("FirebirdResultSet::next - No more rows (status 100)");
                return false;
            }
            else
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("FirebirdResultSet::next - Error: " << errorMsg);
                throw DBException("B8C4D0E6F2A3", "Error fetching row: " + errorMsg,
                                  system_utils::captureCallStack());
            }
        }

        bool FirebirdDBResultSet::isBeforeFirst()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return m_rowPosition == 0 && !m_hasData;
        }

        bool FirebirdDBResultSet::isAfterLast()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return !m_hasData && m_rowPosition > 0;
        }

        uint64_t FirebirdDBResultSet::getRow()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return m_rowPosition;
        }

        int FirebirdDBResultSet::getInt(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? 0 : std::stoi(value);
        }

        int FirebirdDBResultSet::getInt(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("C9D5E1F7A4B0", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getInt(it->second);
        }

        long FirebirdDBResultSet::getLong(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? 0L : std::stol(value);
        }

        long FirebirdDBResultSet::getLong(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("D0E6F2A8B5C1", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getLong(it->second);
        }

        double FirebirdDBResultSet::getDouble(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("getDouble(columnIndex=" << columnIndex << ")");
            std::string value = getColumnValue(columnIndex);
            FIREBIRD_DEBUG("  getColumnValue returned: '" << value << "'");
            return value.empty() ? 0.0 : std::stod(value);
        }

        double FirebirdDBResultSet::getDouble(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("E1F7A3B9C6D2", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getDouble(it->second);
        }

        std::string FirebirdDBResultSet::getString(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return getColumnValue(columnIndex);
        }

        std::string FirebirdDBResultSet::getString(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("F2A8B4C0D7E3", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getString(it->second);
        }

        bool FirebirdDBResultSet::getBoolean(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            std::string value = getColumnValue(columnIndex);
            if (value.empty())
                return false;

            if (value == "1" || value == "true" || value == "TRUE" || value == "T" || value == "t" || value == "Y" || value == "y")
                return true;
            return false;
        }

        bool FirebirdDBResultSet::getBoolean(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("A3B9C5D1E8F4", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getBoolean(it->second);
        }

        bool FirebirdDBResultSet::isNull(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (columnIndex >= m_fieldCount)
            {
                throw DBException("B4C0D6E2F9A5", "Column index out of range: " + std::to_string(columnIndex),
                                  system_utils::captureCallStack());
            }
            return m_nullIndicators[columnIndex] < 0;
        }

        bool FirebirdDBResultSet::isNull(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("C5D1E7F3A0B6", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return isNull(it->second);
        }

        std::vector<std::string> FirebirdDBResultSet::getColumnNames()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return m_columnNames;
        }

        size_t FirebirdDBResultSet::getColumnCount()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return m_fieldCount;
        }

        void FirebirdDBResultSet::close()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("FirebirdResultSet::close - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_ownStatement: " << (m_ownStatement ? "true" : "false"));

            if (m_closed)
            {
                FIREBIRD_DEBUG("FirebirdResultSet::close - Already closed, returning");
                return;
            }

            if (m_ownStatement && m_stmt && *m_stmt)
            {
                FIREBIRD_DEBUG("  Freeing statement handle with DSQL_drop: " << *m_stmt);
                ISC_STATUS_ARRAY status;
                isc_stmt_handle *stmtPtr = m_stmt.get();
                // Use DSQL_drop to fully release the statement since we own it
                isc_dsql_free_statement(status, stmtPtr, DSQL_drop);
                *stmtPtr = 0; // Mark as freed
                FIREBIRD_DEBUG("  Statement freed");
            }

            // Smart pointers will handle cleanup automatically
            FIREBIRD_DEBUG("  Resetting smart pointers");
            m_sqlda.reset();
            m_stmt.reset();

            m_closed = true;
            FIREBIRD_DEBUG("FirebirdResultSet::close - Done");
        }

        bool FirebirdDBResultSet::isEmpty()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return !m_hasData && m_rowPosition == 0;
        }

        std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (columnIndex >= m_fieldCount)
            {
                throw DBException("D6E2F8A4B1C7", "Column index out of range: " + std::to_string(columnIndex),
                                  system_utils::captureCallStack());
            }

            if (m_nullIndicators[columnIndex] < 0)
            {
                return nullptr;
            }

            XSQLVAR *var = &m_sqlda->sqlvar[columnIndex];
            if ((var->sqltype & ~1) != SQL_BLOB)
            {
                throw DBException("E7F3A9B5C2D8", "Column is not a BLOB type", system_utils::captureCallStack());
            }

            auto conn = m_connection.lock();
            if (!conn)
            {
                throw DBException("F8A4B0C6D3E9", "Connection has been closed", system_utils::captureCallStack());
            }

            ISC_QUAD *blobId = reinterpret_cast<ISC_QUAD *>(var->sqldata);
            // Use the connection-based constructor for memory safety
            return std::make_shared<FirebirdBlob>(conn, *blobId);
        }

        std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("A9B5C1D7E4F0", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getBlob(it->second);
        }

        std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(size_t columnIndex)
        {
            auto blob = getBlob(columnIndex);
            if (!blob)
                return nullptr;
            return blob->getBinaryStream();
        }

        std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("B0C6D2E8F5A1", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getBinaryStream(it->second);
        }

        std::vector<uint8_t> FirebirdDBResultSet::getBytes(size_t columnIndex)
        {
            auto blob = getBlob(columnIndex);
            if (!blob)
                return std::vector<uint8_t>();
            return blob->getBytes(0, blob->length());
        }

        std::vector<uint8_t> FirebirdDBResultSet::getBytes(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("C1D7E3F9A6B2", "Column not found: " + columnName, system_utils::captureCallStack());
            }
            return getBytes(it->second);
        }

        // ============================================================================
        // FirebirdDBPreparedStatement Implementation
        // ============================================================================

        FirebirdDBPreparedStatement::FirebirdDBPreparedStatement(std::weak_ptr<isc_db_handle> db,
                                                                 isc_tr_handle *trPtr,
                                                                 const std::string &sql,
                                                                 std::weak_ptr<FirebirdDBConnection> conn)
            : m_dbHandle(db), m_connection(conn), m_trPtr(trPtr), m_stmt(0), m_sql(sql),
              m_inputSqlda(nullptr), m_outputSqlda(nullptr)
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::constructor - Creating statement");
            FIREBIRD_DEBUG("  SQL: " << sql);
            FIREBIRD_DEBUG("  trPtr: " << trPtr << ", *trPtr: " << (trPtr ? *trPtr : 0));
            prepareStatement();
            m_closed = false;
            FIREBIRD_DEBUG("FirebirdPreparedStatement::constructor - Done, m_stmt=" << m_stmt);
        }

        FirebirdDBPreparedStatement::~FirebirdDBPreparedStatement()
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::destructor - Destroying statement, m_stmt=" << m_stmt);
            close();
            FIREBIRD_DEBUG("FirebirdPreparedStatement::destructor - Done");
        }

        isc_db_handle *FirebirdDBPreparedStatement::getFirebirdConnection() const
        {
            auto db = m_dbHandle.lock();
            if (!db)
            {
                throw DBException("D2E8F4A0B7C3", "Connection has been closed", system_utils::captureCallStack());
            }
            return db.get();
        }

        void FirebirdDBPreparedStatement::prepareStatement()
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::prepareStatement - Starting");
            ISC_STATUS_ARRAY status;
            isc_db_handle *db = getFirebirdConnection();
            FIREBIRD_DEBUG("  db handle: " << db << ", *db: " << (db ? *db : 0));

            // Allocate statement handle
            FIREBIRD_DEBUG("  Allocating statement handle...");
            if (isc_dsql_allocate_statement(status, db, &m_stmt))
            {
                FIREBIRD_DEBUG("  Failed to allocate statement: " << interpretStatusVector(status));
                throw DBException("E3F9A5B1C8D4", "Failed to allocate statement: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  Statement allocated, m_stmt=" << m_stmt);

            // Allocate output SQLDA
            FIREBIRD_DEBUG("  Allocating output SQLDA...");
            XSQLDA *outputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(20)));
            outputSqlda->sqln = 20;
            outputSqlda->version = SQLDA_VERSION1;
            m_outputSqlda.reset(outputSqlda);

            // Prepare the statement
            FIREBIRD_DEBUG("  Preparing statement with SQL: " << m_sql);
            FIREBIRD_DEBUG("  m_trPtr=" << m_trPtr << ", *m_trPtr=" << (m_trPtr ? *m_trPtr : 0));
            if (isc_dsql_prepare(status, m_trPtr, &m_stmt, 0, m_sql.c_str(), SQL_DIALECT_V6, m_outputSqlda.get()))
            {
                // Save the error message BEFORE calling any other Firebird API functions
                // because they will overwrite the status vector
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to prepare statement: " << errorMsg);
                m_outputSqlda.reset();
                ISC_STATUS_ARRAY freeStatus; // Use separate status array for cleanup
                isc_dsql_free_statement(freeStatus, &m_stmt, DSQL_drop);
                throw DBException("F4A0B6C2D9E5", "Failed to prepare statement: " + errorMsg,
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  Statement prepared, m_stmt=" << m_stmt << ", output columns=" << m_outputSqlda->sqld);

            // Reallocate output SQLDA if needed
            if (m_outputSqlda->sqld > m_outputSqlda->sqln)
            {
                int n = m_outputSqlda->sqld;
                FIREBIRD_DEBUG("  Reallocating output SQLDA for " << n << " columns");
                XSQLDA *newOutputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(n)));
                newOutputSqlda->sqln = static_cast<short>(n);
                newOutputSqlda->version = SQLDA_VERSION1;
                m_outputSqlda.reset(newOutputSqlda);

                if (isc_dsql_describe(status, &m_stmt, SQL_DIALECT_V6, m_outputSqlda.get()))
                {
                    FIREBIRD_DEBUG("  Failed to describe statement: " << interpretStatusVector(status));
                    throw DBException("A5B1C7D3E0F6", "Failed to describe statement: " + interpretStatusVector(status),
                                      system_utils::captureCallStack());
                }
            }

            // Allocate input SQLDA
            FIREBIRD_DEBUG("  Allocating input SQLDA...");
            allocateInputSqlda();

            m_prepared = true;
            FIREBIRD_DEBUG("FirebirdPreparedStatement::prepareStatement - Done, m_stmt=" << m_stmt);
        }

        void FirebirdDBPreparedStatement::allocateInputSqlda()
        {
            ISC_STATUS_ARRAY status;

            // Allocate input SQLDA
            XSQLDA *inputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(20)));
            inputSqlda->sqln = 20;
            inputSqlda->version = SQLDA_VERSION1;
            m_inputSqlda.reset(inputSqlda);

            // Describe input parameters
            if (isc_dsql_describe_bind(status, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                m_inputSqlda.reset();
                throw DBException("B6C2D8E4F1A7", "Failed to describe bind parameters: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }

            // Reallocate if needed
            if (m_inputSqlda->sqld > m_inputSqlda->sqln)
            {
                int n = m_inputSqlda->sqld;
                XSQLDA *newInputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(n)));
                newInputSqlda->sqln = static_cast<short>(n);
                newInputSqlda->version = SQLDA_VERSION1;
                m_inputSqlda.reset(newInputSqlda);

                if (isc_dsql_describe_bind(status, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
                {
                    throw DBException("C7D3E9F5A2B8", "Failed to describe bind parameters: " + interpretStatusVector(status),
                                      system_utils::captureCallStack());
                }
            }

            // Allocate buffers for parameters
            m_paramBuffers.resize(static_cast<size_t>(m_inputSqlda->sqld));
            m_paramNullIndicators.resize(static_cast<size_t>(m_inputSqlda->sqld), 0);

            for (int i = 0; i < m_inputSqlda->sqld; ++i)
            {
                XSQLVAR *var = &m_inputSqlda->sqlvar[i];
                size_t bufferSize = static_cast<size_t>(var->sqllen);

                if ((var->sqltype & ~1) == SQL_VARYING)
                {
                    bufferSize += sizeof(short);
                }
                else if ((var->sqltype & ~1) == SQL_BLOB)
                {
                    bufferSize = sizeof(ISC_QUAD);
                }

                m_paramBuffers[static_cast<size_t>(i)].resize(bufferSize + 1, 0);
                var->sqldata = m_paramBuffers[static_cast<size_t>(i)].data();
                var->sqlind = &m_paramNullIndicators[static_cast<size_t>(i)];
            }
        }

        void FirebirdDBPreparedStatement::setParameter(int parameterIndex, const void *data, size_t length, [[maybe_unused]] short sqlType)
        {
            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                throw DBException("D8E4F0A6B3C9", "Parameter index out of range: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];

            // Resize buffer if needed
            if (length > m_paramBuffers[idx].size())
            {
                m_paramBuffers[idx].resize(length + 1, 0);
                var->sqldata = m_paramBuffers[idx].data();
            }

            std::memcpy(var->sqldata, data, length);
            var->sqllen = static_cast<short>(length);
            m_paramNullIndicators[idx] = 0;
        }

        void FirebirdDBPreparedStatement::notifyConnClosing()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            m_closed = true;
        }

        void FirebirdDBPreparedStatement::setInt(int parameterIndex, int value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            ISC_LONG val = static_cast<ISC_LONG>(value);
            setParameter(parameterIndex, &val, sizeof(ISC_LONG), SQL_LONG);
        }

        void FirebirdDBPreparedStatement::setLong(int parameterIndex, long value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            ISC_INT64 val = static_cast<ISC_INT64>(value);
            setParameter(parameterIndex, &val, sizeof(ISC_INT64), SQL_INT64);
        }

        void FirebirdDBPreparedStatement::setDouble(int parameterIndex, double value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                throw DBException("D8E4F0A6B3C8", "Parameter index out of range: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];
            short sqlType = var->sqltype & ~1;

            FIREBIRD_DEBUG("setDouble: parameterIndex=" << parameterIndex << ", value=" << value);
            FIREBIRD_DEBUG("  sqlType=" << sqlType << ", sqlscale=" << var->sqlscale << ", sqllen=" << var->sqllen);

            // Handle DECIMAL/NUMERIC types which are stored as scaled integers
            if (var->sqlscale < 0)
            {
                // Convert double to scaled integer
                double scaleFactor = std::pow(10.0, -var->sqlscale);
                FIREBIRD_DEBUG("  DECIMAL type detected, scaleFactor=" << scaleFactor);

                if (sqlType == SQL_SHORT)
                {
                    short scaledValue = static_cast<short>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_SHORT: scaledValue=" << scaledValue);
                    // Don't use setParameter, set directly to preserve sqllen
                    if (m_paramBuffers[idx].size() < sizeof(short))
                    {
                        m_paramBuffers[idx].resize(sizeof(short), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(short));
                    m_paramNullIndicators[idx] = 0;
                }
                else if (sqlType == SQL_LONG)
                {
                    ISC_LONG scaledValue = static_cast<ISC_LONG>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_LONG: scaledValue=" << scaledValue);
                    // Don't use setParameter, set directly to preserve sqllen
                    if (m_paramBuffers[idx].size() < sizeof(ISC_LONG))
                    {
                        m_paramBuffers[idx].resize(sizeof(ISC_LONG), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(ISC_LONG));
                    m_paramNullIndicators[idx] = 0;
                }
                else if (sqlType == SQL_INT64)
                {
                    ISC_INT64 scaledValue = static_cast<ISC_INT64>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_INT64: scaledValue=" << scaledValue);
                    // Don't use setParameter, set directly to preserve sqllen
                    if (m_paramBuffers[idx].size() < sizeof(ISC_INT64))
                    {
                        m_paramBuffers[idx].resize(sizeof(ISC_INT64), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(ISC_INT64));
                    m_paramNullIndicators[idx] = 0;
                }
                else
                {
                    FIREBIRD_DEBUG("  Unknown scaled type, falling back to double");
                    // Fallback to double
                    setParameter(parameterIndex, &value, sizeof(double), SQL_DOUBLE);
                }
            }
            else if (sqlType == SQL_FLOAT)
            {
                float floatValue = static_cast<float>(value);
                FIREBIRD_DEBUG("  SQL_FLOAT: floatValue=" << floatValue);
                setParameter(parameterIndex, &floatValue, sizeof(float), SQL_FLOAT);
            }
            else
            {
                FIREBIRD_DEBUG("  SQL_DOUBLE: value=" << value);
                // SQL_DOUBLE or SQL_D_FLOAT
                setParameter(parameterIndex, &value, sizeof(double), SQL_DOUBLE);
            }
        }

        void FirebirdDBPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                throw DBException("E9F5A1B7C4D0", "Parameter index out of range: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];
            short sqlType = var->sqltype & ~1;

            // Handle BLOB type - convert string to BLOB
            if (sqlType == SQL_BLOB)
            {
                FIREBIRD_DEBUG("setString: parameterIndex=" << parameterIndex << " is BLOB type, converting to BLOB");
                // Convert string to bytes and use setBytes
                std::vector<uint8_t> data(value.begin(), value.end());

                auto conn = m_connection.lock();
                if (!conn)
                {
                    throw DBException("E9F5A1B7C4D1", "Connection has been closed", system_utils::captureCallStack());
                }

                // Create a FirebirdBlob with the data using the connection-based constructor
                auto blob = std::make_shared<FirebirdBlob>(conn, data);

                // Save the blob to the database and get its ID
                ISC_QUAD blobId = blob->save();

                // Store the blob data to keep it alive
                m_blobValues.push_back(data);

                // Ensure buffer is large enough for ISC_QUAD
                if (m_paramBuffers[idx].size() < sizeof(ISC_QUAD))
                {
                    m_paramBuffers[idx].resize(sizeof(ISC_QUAD), 0);
                    var->sqldata = m_paramBuffers[idx].data();
                }

                // Copy the blob ID to the parameter buffer
                std::memcpy(var->sqldata, &blobId, sizeof(ISC_QUAD));
                var->sqllen = sizeof(ISC_QUAD);
                m_paramNullIndicators[idx] = 0;
                return;
            }

            // Handle VARCHAR type
            size_t totalLen = sizeof(short) + value.length();
            if (totalLen > m_paramBuffers[idx].size())
            {
                m_paramBuffers[idx].resize(totalLen + 1, 0);
                var->sqldata = m_paramBuffers[idx].data();
            }

            short len = static_cast<short>(value.length());
            std::memcpy(var->sqldata, &len, sizeof(short));
            std::memcpy(var->sqldata + sizeof(short), value.c_str(), value.length());
            var->sqllen = static_cast<short>(totalLen);
            m_paramNullIndicators[idx] = 0;
        }

        void FirebirdDBPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            short val = value ? 1 : 0;
            setParameter(parameterIndex, &val, sizeof(short), SQL_SHORT);
        }

        void FirebirdDBPreparedStatement::setNull(int parameterIndex, [[maybe_unused]] Types type)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                throw DBException("F0A6B2C8D5E1", "Parameter index out of range: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            m_paramNullIndicators[idx] = -1;
        }

        void FirebirdDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            // Parse date string (expected format: YYYY-MM-DD)
            struct tm time = {};
            if (sscanf(value.c_str(), "%d-%d-%d", &time.tm_year, &time.tm_mon, &time.tm_mday) != 3)
            {
                throw DBException("A1B7C3D9E6F2", "Invalid date format: " + value, system_utils::captureCallStack());
            }
            time.tm_year -= 1900;
            time.tm_mon -= 1;

            ISC_DATE date;
            isc_encode_sql_date(&time, &date);
            setParameter(parameterIndex, &date, sizeof(ISC_DATE), SQL_TYPE_DATE);
        }

        void FirebirdDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            // Parse timestamp string (expected format: YYYY-MM-DD HH:MM:SS)
            struct tm time = {};
            if (sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                       &time.tm_year, &time.tm_mon, &time.tm_mday,
                       &time.tm_hour, &time.tm_min, &time.tm_sec) != 6)
            {
                throw DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack());
            }
            time.tm_year -= 1900;
            time.tm_mon -= 1;

            ISC_TIMESTAMP ts;
            isc_encode_timestamp(&time, &ts);
            setParameter(parameterIndex, &ts, sizeof(ISC_TIMESTAMP), SQL_TIMESTAMP);
        }

        void FirebirdDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (!x)
            {
                setNull(parameterIndex, Types::BLOB);
                return;
            }

            m_blobObjects.push_back(x);
            std::vector<uint8_t> data = x->getBytes(0, x->length());
            setBytes(parameterIndex, data);
        }

        void FirebirdDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (!x)
            {
                setNull(parameterIndex, Types::BLOB);
                return;
            }

            m_streamObjects.push_back(x);

            // Read all data from stream
            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
            }

            setBytes(parameterIndex, data);
        }

        void FirebirdDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (!x)
            {
                setNull(parameterIndex, Types::BLOB);
                return;
            }

            m_streamObjects.push_back(x);

            // Read specified length from stream
            std::vector<uint8_t> data(length);
            size_t totalRead = 0;
            while (totalRead < length)
            {
                int bytesRead = x->read(data.data() + totalRead, length - totalRead);
                if (bytesRead <= 0)
                    break;
                totalRead += static_cast<size_t>(bytesRead);
            }
            data.resize(totalRead);

            setBytes(parameterIndex, data);
        }

        void FirebirdDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
            setBytes(parameterIndex, x.data(), x.size());
        }

        void FirebirdDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                throw DBException("C3D9E5F1A8B4", "Parameter index out of range: " + std::to_string(parameterIndex),
                                  system_utils::captureCallStack());
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];

            if ((var->sqltype & ~1) == SQL_BLOB)
            {
                // For Firebird BLOB parameters, we need to create a BLOB in the database
                // and store its ID (ISC_QUAD) in the parameter buffer
                auto conn = m_connection.lock();
                if (!conn)
                {
                    throw DBException("C3D9E5F1A8B5", "Connection has been closed", system_utils::captureCallStack());
                }

                // Create a FirebirdBlob with the data using the connection-based constructor
                std::vector<uint8_t> blobData(x, x + length);
                auto blob = std::make_shared<FirebirdBlob>(conn, blobData);

                // Save the blob to the database and get its ID
                ISC_QUAD blobId = blob->save();

                // Store the blob object to keep it alive
                m_blobValues.push_back(blobData);

                // Ensure buffer is large enough for ISC_QUAD
                if (m_paramBuffers[idx].size() < sizeof(ISC_QUAD))
                {
                    m_paramBuffers[idx].resize(sizeof(ISC_QUAD), 0);
                    var->sqldata = m_paramBuffers[idx].data();
                }

                // Copy the blob ID to the parameter buffer
                std::memcpy(var->sqldata, &blobId, sizeof(ISC_QUAD));
                var->sqllen = sizeof(ISC_QUAD);
                m_paramNullIndicators[idx] = 0;
            }
            else
            {
                // For non-BLOB types, store the raw bytes
                m_blobValues.push_back(std::vector<uint8_t>(x, x + length));
                setParameter(parameterIndex, x, length, SQL_BLOB);
            }
        }

        std::shared_ptr<RelationalDBResultSet> FirebirdDBPreparedStatement::executeQuery()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeQuery - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);
            FIREBIRD_DEBUG("  m_trPtr: " << m_trPtr << ", *m_trPtr: " << (m_trPtr ? *m_trPtr : 0));

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Statement is closed!");
                throw DBException("D4E0F6A2B9C5", "Statement is closed", system_utils::captureCallStack());
            }

            ISC_STATUS_ARRAY status;

            // Execute the statement
            FIREBIRD_DEBUG("  Executing statement with isc_dsql_execute...");
            FIREBIRD_DEBUG("    m_trPtr=" << m_trPtr << ", *m_trPtr=" << (m_trPtr ? *m_trPtr : 0));
            FIREBIRD_DEBUG("    &m_stmt=" << &m_stmt << ", m_stmt=" << m_stmt);
            if (isc_dsql_execute(status, m_trPtr, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Execute failed: " << errorMsg);

                // If autocommit is enabled, rollback the failed transaction and start a new one
                auto conn = m_connection.lock();
                if (conn && conn->getAutoCommit())
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, rolling back failed transaction");
                    try
                    {
                        conn->rollback();
                    }
                    catch (...)
                    {
                        FIREBIRD_DEBUG("  Rollback failed, ignoring");
                    }
                }

                throw DBException("E5F1A7B3C0D6", "Failed to execute query: " + errorMsg,
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  Execute succeeded, m_stmt after execute=" << m_stmt);

            // Allocate output SQLDA for results - need to copy and set up buffers
            int numCols = m_outputSqlda->sqld;
            FIREBIRD_DEBUG("  Output columns: " << numCols);
            if (numCols == 0)
            {
                numCols = 1; // At least allocate space for 1 column
            }

            XSQLDA *resultSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(numCols)));
            resultSqlda->sqln = static_cast<short>(numCols);
            resultSqlda->sqld = m_outputSqlda->sqld;
            resultSqlda->version = SQLDA_VERSION1;

            // Copy column descriptors
            for (int i = 0; i < m_outputSqlda->sqld; ++i)
            {
                resultSqlda->sqlvar[i] = m_outputSqlda->sqlvar[i];
                FIREBIRD_DEBUG("    Column " << i << ": raw_sqltype=" << m_outputSqlda->sqlvar[i].sqltype << ", type=" << (resultSqlda->sqlvar[i].sqltype & ~1) << ", nullable=" << (m_outputSqlda->sqlvar[i].sqltype & 1) << ", len=" << resultSqlda->sqlvar[i].sqllen);
            }

            // IMPORTANT: Transfer ownership of the statement handle to the ResultSet
            // The ResultSet will own the statement and free it when closed
            // We set m_stmt to 0 so that PreparedStatement::close() won't try to free it
            FIREBIRD_DEBUG("  Transferring statement ownership to ResultSet");
            FIREBIRD_DEBUG("    m_stmt value: " << m_stmt);
            FIREBIRD_DEBUG("    &m_stmt address: " << &m_stmt);

            // Create a new handle with the statement value
            isc_stmt_handle *stmtPtr = new isc_stmt_handle(m_stmt);
            FIREBIRD_DEBUG("    stmtPtr: " << stmtPtr << ", *stmtPtr: " << *stmtPtr);

            // Transfer ownership - set our m_stmt to 0 so we don't free it in close()
            m_stmt = 0;
            FIREBIRD_DEBUG("    m_stmt after transfer: " << m_stmt);

            FirebirdStmtHandle stmtHandle(stmtPtr);
            XSQLDAHandle sqldaHandle(resultSqlda);

            // ownStatement = true means the ResultSet will free the statement when closed
            FIREBIRD_DEBUG("  Creating FirebirdResultSet with ownStatement=true");
            // Pass the connection to the ResultSet so it can read BLOBs
            auto conn = m_connection.lock();
            auto resultSet = std::make_shared<FirebirdDBResultSet>(std::move(stmtHandle), std::move(sqldaHandle), true, conn);
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeQuery - Done");
            return resultSet;
        }

        uint64_t FirebirdDBPreparedStatement::executeUpdate()
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate - Starting");
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);
            if (m_closed)
            {
                throw DBException("F6A2B8C4D1E7", "Statement is closed", system_utils::captureCallStack());
            }

            ISC_STATUS_ARRAY status;

            // Execute the statement
            FIREBIRD_DEBUG("  Calling isc_dsql_execute...");
            FIREBIRD_DEBUG("    m_trPtr=" << m_trPtr << ", *m_trPtr=" << (m_trPtr ? *m_trPtr : 0));
            FIREBIRD_DEBUG("    &m_stmt=" << &m_stmt << ", m_stmt=" << m_stmt);
            if (isc_dsql_execute(status, m_trPtr, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                // Save the error message BEFORE calling any other Firebird API functions
                // because they will overwrite the status vector
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  isc_dsql_execute failed: " << errorMsg);

                // If autocommit is enabled, rollback the failed transaction and start a new one
                // This ensures the connection is in a clean state for the next operation
                auto conn = m_connection.lock();
                if (conn && conn->getAutoCommit())
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, rolling back failed transaction");
                    try
                    {
                        conn->rollback();
                    }
                    catch (...)
                    {
                        FIREBIRD_DEBUG("  Rollback failed, ignoring");
                    }
                }

                throw DBException("A7B3C9D5E2F8", "Failed to execute update: " + errorMsg,
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  isc_dsql_execute succeeded");

            // Get affected rows count
            char infoBuffer[64] = {0}; // Initialize to zero to avoid valgrind warnings
            char resultBuffer[64] = {0};
            infoBuffer[0] = isc_info_sql_records;
            infoBuffer[1] = isc_info_end;

            if (isc_dsql_sql_info(status, &m_stmt, sizeof(infoBuffer), infoBuffer, sizeof(resultBuffer), resultBuffer))
            {
                FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate - Failed to get sql_info, checking autocommit");
                // If autocommit is enabled, commit the transaction even if we can't get the count
                auto conn = m_connection.lock();
                if (conn && conn->getAutoCommit())
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, calling commit()");
                    conn->commit();
                    FIREBIRD_DEBUG("  Commit completed");
                }
                FIREBIRD_DEBUG("  Returning 0 (unable to get count)");
                return 0; // Unable to get count, return 0
            }

            // Parse the result buffer to get the count
            uint64_t count = 0;
            char *p = resultBuffer;
            while (*p != isc_info_end)
            {
                char item = *p++;
                short len = static_cast<short>(isc_vax_integer(p, 2));
                p += 2;

                if (item == isc_info_sql_records)
                {
                    while (*p != isc_info_end)
                    {
                        char subItem = *p++;
                        short subLen = static_cast<short>(isc_vax_integer(p, 2));
                        p += 2;

                        if (subItem == isc_info_req_update_count ||
                            subItem == isc_info_req_delete_count ||
                            subItem == isc_info_req_insert_count)
                        {
                            count += static_cast<uint64_t>(isc_vax_integer(p, subLen));
                        }
                        p += subLen;
                    }
                }
                else
                {
                    p += len;
                }
            }

            // If autocommit is enabled, commit the transaction after the update
            // Note: commit() already calls startTransaction() internally when autoCommit is enabled
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate - Checking autocommit");
            auto conn = m_connection.lock();
            if (conn)
            {
                FIREBIRD_DEBUG("  Connection is valid");
                if (conn->getAutoCommit())
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, calling commit()");
                    conn->commit();
                    FIREBIRD_DEBUG("  Commit completed");
                }
                else
                {
                    FIREBIRD_DEBUG("  AutoCommit is disabled, skipping commit");
                }
            }
            else
            {
                FIREBIRD_DEBUG("  Connection is null (weak_ptr expired)");
            }

            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate - Done, returning count=" << count);
            return count;
        }

        bool FirebirdDBPreparedStatement::execute()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            if (m_closed)
            {
                throw DBException("B8C4D0E6F3A9", "Statement is closed", system_utils::captureCallStack());
            }

            ISC_STATUS_ARRAY status;

            if (isc_dsql_execute(status, m_trPtr, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                throw DBException("C9D5E1F7A4B0", "Failed to execute statement: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }

            return m_outputSqlda->sqld > 0; // Returns true if there are result columns
        }

        void FirebirdDBPreparedStatement::close()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            FIREBIRD_DEBUG("FirebirdPreparedStatement::close - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Already closed, returning");
                return;
            }

            ISC_STATUS_ARRAY status;

            if (m_stmt)
            {
                FIREBIRD_DEBUG("  Freeing statement with DSQL_drop...");
                isc_dsql_free_statement(status, &m_stmt, DSQL_drop);
                FIREBIRD_DEBUG("  Statement freed");
                m_stmt = 0;
            }

            // Smart pointers will handle cleanup automatically
            FIREBIRD_DEBUG("  Resetting smart pointers");
            m_inputSqlda.reset();
            m_outputSqlda.reset();

            m_closed = true;
            FIREBIRD_DEBUG("FirebirdPreparedStatement::close - Done");
        }

        // ============================================================================
        // FirebirdDBConnection Implementation
        // ============================================================================

        FirebirdDBConnection::FirebirdDBConnection(const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password,
                                                   const std::map<std::string, std::string> &options)
            : m_tr(0), m_isolationLevel(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
        {
            FIREBIRD_DEBUG("FirebirdConnection::constructor - Starting");
            FIREBIRD_DEBUG("  host: " << host);
            FIREBIRD_DEBUG("  port: " << port);
            FIREBIRD_DEBUG("  database: " << database);
            FIREBIRD_DEBUG("  user: " << user);

            ISC_STATUS_ARRAY status;

            // Build connection string
            std::string connStr;
            if (!host.empty() && host != "localhost" && host != "127.0.0.1")
            {
                connStr = host;
                if (port != 3050 && port != 0)
                {
                    connStr += "/" + std::to_string(port);
                }
                connStr += ":";
            }
            connStr += database;
            FIREBIRD_DEBUG("  Connection string: " << connStr);

            // Build DPB (Database Parameter Block)
            std::vector<char> dpb;
            dpb.push_back(isc_dpb_version1);

            // Add user name
            dpb.push_back(isc_dpb_user_name);
            dpb.push_back(static_cast<char>(user.length()));
            dpb.insert(dpb.end(), user.begin(), user.end());

            // Add password
            dpb.push_back(isc_dpb_password);
            dpb.push_back(static_cast<char>(password.length()));
            dpb.insert(dpb.end(), password.begin(), password.end());

            // Add character set (default to UTF8)
            std::string charset = "UTF8";
            auto it = options.find("charset");
            if (it != options.end())
            {
                charset = it->second;
            }
            dpb.push_back(isc_dpb_lc_ctype);
            dpb.push_back(static_cast<char>(charset.length()));
            dpb.insert(dpb.end(), charset.begin(), charset.end());

            // Allocate database handle
            isc_db_handle *dbHandle = new isc_db_handle(0);
            FIREBIRD_DEBUG("  Attaching to database...");

            // Attach to database
            if (isc_attach_database(status, 0, connStr.c_str(), dbHandle,
                                    static_cast<short>(dpb.size()), dpb.data()))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to attach: " << errorMsg);
                delete dbHandle;
                throw DBException("D0E6F2A8B5C1", "Failed to connect to database: " + errorMsg,
                                  system_utils::captureCallStack());
            }
            FIREBIRD_DEBUG("  Attached successfully, dbHandle=" << dbHandle << ", *dbHandle=" << *dbHandle);

            // Create shared_ptr with custom deleter
            m_db = std::shared_ptr<isc_db_handle>(dbHandle, [](isc_db_handle *db)
                                                  {
                FIREBIRD_DEBUG("FirebirdConnection deleter - Detaching database");
                if (db && *db)
                {
                    ISC_STATUS_ARRAY detachStatus;
                    isc_detach_database(detachStatus, db);
                }
                delete db;
                FIREBIRD_DEBUG("FirebirdConnection deleter - Done"); });

            // Cache URL
            m_url = "cpp_dbc:firebird://" + host + ":" + std::to_string(port) + "/" + database;

            m_closed = false;

            // Start initial transaction if autocommit is enabled
            FIREBIRD_DEBUG("  m_autoCommit: " << (m_autoCommit ? "true" : "false"));
            if (m_autoCommit)
            {
                FIREBIRD_DEBUG("  Starting initial transaction...");
                startTransaction();
            }
            FIREBIRD_DEBUG("FirebirdConnection::constructor - Done");
        }

        FirebirdDBConnection::~FirebirdDBConnection()
        {
            close();
        }

        void FirebirdDBConnection::startTransaction()
        {
            FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Starting");
            FIREBIRD_DEBUG("  m_tr: " << m_tr);

            if (m_tr)
            {
                FIREBIRD_DEBUG("  Transaction already active, returning");
                return; // Transaction already active
            }

            ISC_STATUS_ARRAY status;

            // Build TPB (Transaction Parameter Block) based on isolation level
            std::vector<char> tpb;
            tpb.push_back(isc_tpb_version3);

            switch (m_isolationLevel)
            {
            case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
                tpb.push_back(isc_tpb_read_committed);
                tpb.push_back(isc_tpb_rec_version);
                break;
            case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
                tpb.push_back(isc_tpb_read_committed);
                tpb.push_back(isc_tpb_no_rec_version);
                break;
            case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
                tpb.push_back(isc_tpb_concurrency);
                break;
            case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
                tpb.push_back(isc_tpb_consistency);
                break;
            default:
                tpb.push_back(isc_tpb_read_committed);
                tpb.push_back(isc_tpb_no_rec_version);
                break;
            }

            tpb.push_back(isc_tpb_write);
            tpb.push_back(isc_tpb_wait);

            FIREBIRD_DEBUG("  Calling isc_start_transaction...");
            FIREBIRD_DEBUG("    m_db.get()=" << m_db.get() << ", *m_db.get()=" << (m_db.get() ? *m_db.get() : 0));
            if (isc_start_transaction(status, &m_tr, 1, m_db.get(),
                                      static_cast<unsigned short>(tpb.size()), tpb.data()))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to start transaction: " << errorMsg);
                throw DBException("E1F7A3B9C6D2", "Failed to start transaction: " + errorMsg,
                                  system_utils::captureCallStack());
            }

            m_transactionActive = true;
            FIREBIRD_DEBUG("FirebirdConnection::startTransaction - Done, m_tr=" << m_tr);
        }

        void FirebirdDBConnection::endTransaction(bool commit)
        {
            FIREBIRD_DEBUG("FirebirdConnection::endTransaction - Starting, commit=" << (commit ? "true" : "false"));
            if (!m_tr)
            {
                FIREBIRD_DEBUG("  No active transaction (m_tr=0), returning");
                return;
            }

            ISC_STATUS_ARRAY status;

            if (commit)
            {
                FIREBIRD_DEBUG("  Calling isc_commit_transaction, m_tr=" << m_tr);
                if (isc_commit_transaction(status, &m_tr))
                {
                    FIREBIRD_DEBUG("  isc_commit_transaction failed: " << interpretStatusVector(status));
                    throw DBException("F2A8B4C0D7E3", "Failed to commit transaction: " + interpretStatusVector(status),
                                      system_utils::captureCallStack());
                }
                FIREBIRD_DEBUG("  isc_commit_transaction succeeded");
            }
            else
            {
                FIREBIRD_DEBUG("  Calling isc_rollback_transaction, m_tr=" << m_tr);
                if (isc_rollback_transaction(status, &m_tr))
                {
                    FIREBIRD_DEBUG("  isc_rollback_transaction failed: " << interpretStatusVector(status));
                    throw DBException("A3B9C5D1E8F4", "Failed to rollback transaction: " + interpretStatusVector(status),
                                      system_utils::captureCallStack());
                }
                FIREBIRD_DEBUG("  isc_rollback_transaction succeeded");
            }

            m_tr = 0;
            m_transactionActive = false;
            FIREBIRD_DEBUG("FirebirdConnection::endTransaction - Done");
        }

        void FirebirdDBConnection::registerStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            m_activeStatements.insert(stmt);
        }

        void FirebirdDBConnection::unregisterStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            m_activeStatements.erase(stmt);
        }

        void FirebirdDBConnection::close()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            if (m_closed)
                return;

            // Notify all active statements
            {
                std::lock_guard<std::mutex> stmtLock(m_statementsMutex);
                for (auto &weakStmt : m_activeStatements)
                {
                    if (auto stmt = weakStmt.lock())
                    {
                        stmt->notifyConnClosing();
                    }
                }
                m_activeStatements.clear();
            }

            // End any active transaction
            if (m_tr)
            {
                ISC_STATUS_ARRAY status;
                isc_rollback_transaction(status, &m_tr);
                m_tr = 0;
            }

            // The database handle will be closed by the shared_ptr deleter
            m_db.reset();

            m_closed = true;

            // Small delay to ensure cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        bool FirebirdDBConnection::isClosed()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            return m_closed;
        }

        void FirebirdDBConnection::returnToPool()
        {
            FIREBIRD_DEBUG("FirebirdConnection::returnToPool - Starting");
            FIREBIRD_DEBUG("  m_transactionActive: " << (m_transactionActive ? "true" : "false"));
            FIREBIRD_DEBUG("  m_autoCommit: " << (m_autoCommit ? "true" : "false"));
            FIREBIRD_DEBUG("  m_tr: " << m_tr);

            // Reset connection state for pool reuse
            // Always ensure we have a clean transaction state
            if (m_tr)
            {
                FIREBIRD_DEBUG("  Transaction handle exists, committing/rolling back");
                try
                {
                    if (m_autoCommit)
                    {
                        // In autocommit mode, commit any pending changes
                        commit();
                    }
                    else if (m_transactionActive)
                    {
                        // In manual mode with active transaction, rollback
                        rollback();
                    }
                }
                catch (...)
                {
                    FIREBIRD_DEBUG("  Commit/rollback failed, forcing rollback");
                    // If commit/rollback fails, force a rollback to clean up
                    try
                    {
                        ISC_STATUS_ARRAY status;
                        if (m_tr)
                        {
                            isc_rollback_transaction(status, &m_tr);
                            m_tr = 0;
                        }
                    }
                    catch (...)
                    {
                        FIREBIRD_DEBUG("  Force rollback also failed");
                    }
                }
            }

            // Ensure autocommit is enabled for pool reuse (default state)
            m_autoCommit = true;
            m_transactionActive = false;

            // Start a fresh transaction for the next use
            if (!m_tr && !m_closed)
            {
                FIREBIRD_DEBUG("  Starting fresh transaction for pool reuse");
                try
                {
                    startTransaction();
                }
                catch (...)
                {
                    FIREBIRD_DEBUG("  Failed to start fresh transaction");
                }
            }

            FIREBIRD_DEBUG("FirebirdConnection::returnToPool - Done, m_tr=" << m_tr);
        }

        bool FirebirdDBConnection::isPooled()
        {
            return false; // Not pooled by default
        }

        std::shared_ptr<RelationalDBPreparedStatement> FirebirdDBConnection::prepareStatement(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement - Starting");
            FIREBIRD_DEBUG("  SQL: " << sql);
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_tr: " << m_tr);

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Connection is closed!");
                throw DBException("B4C0D6E2F9A5", "Connection is closed", system_utils::captureCallStack());
            }

            if (!m_tr)
            {
                FIREBIRD_DEBUG("  No active transaction, starting one...");
                startTransaction();
            }

            FIREBIRD_DEBUG("  Creating FirebirdDBPreparedStatement...");
            FIREBIRD_DEBUG("    m_db.get()=" << m_db.get() << ", *m_db.get()=" << (m_db.get() ? *m_db.get() : 0));
            FIREBIRD_DEBUG("    &m_tr=" << &m_tr << ", m_tr=" << m_tr);
            auto stmt = std::make_shared<FirebirdDBPreparedStatement>(
                std::weak_ptr<isc_db_handle>(m_db), &m_tr, sql, shared_from_this());

            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement - Done");
            return stmt;
        }

        std::shared_ptr<RelationalDBResultSet> FirebirdDBConnection::executeQuery(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            auto stmt = prepareStatement(sql);
            return stmt->executeQuery();
        }

        uint64_t FirebirdDBConnection::executeUpdate(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            // Check if this is a CREATE DATABASE statement
            // CREATE DATABASE requires special handling with isc_dsql_execute_immediate
            std::string upperSql = sql;
            std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

            // Remove leading whitespace for comparison
            size_t start = upperSql.find_first_not_of(" \t\n\r");
            if (start != std::string::npos)
            {
                upperSql = upperSql.substr(start);
            }

            if (upperSql.find("CREATE DATABASE") == 0 || upperSql.find("CREATE SCHEMA") == 0)
            {
                FIREBIRD_DEBUG("FirebirdConnection::executeUpdate - Detected CREATE DATABASE statement");
                return executeCreateDatabase(sql);
            }

            auto stmt = prepareStatement(sql);
            // Note: PreparedStatement::executeUpdate now handles autocommit internally
            return stmt->executeUpdate();
        }

        void FirebirdDBConnection::setAutoCommit(bool autoCommit)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            if (m_autoCommit == autoCommit)
                return;

            if (m_autoCommit && !autoCommit)
            {
                // Switching from auto-commit to manual
                // Commit any pending transaction
                if (m_tr)
                {
                    commit();
                }
            }

            m_autoCommit = autoCommit;

            if (m_autoCommit && !m_tr)
            {
                startTransaction();
            }
        }

        bool FirebirdDBConnection::getAutoCommit()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            return m_autoCommit;
        }

        bool FirebirdDBConnection::beginTransaction()
        {
            FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Starting");
            FIREBIRD_DEBUG("  m_autoCommit before: " << (m_autoCommit ? "true" : "false"));
            FIREBIRD_DEBUG("  m_transactionActive: " << (m_transactionActive ? "true" : "false"));
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            // Disable autocommit when beginning a manual transaction
            // This prevents executeUpdate from auto-committing
            // IMPORTANT: Must be done BEFORE checking m_transactionActive
            // because in Firebird, a transaction is always active (started in constructor)
            m_autoCommit = false;
            FIREBIRD_DEBUG("  m_autoCommit after: " << (m_autoCommit ? "true" : "false"));

            // If transaction is already active, just return true (like MySQL)
            if (m_transactionActive)
            {
                FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Transaction already active, returning true");
                return true;
            }

            startTransaction();
            FIREBIRD_DEBUG("FirebirdConnection::beginTransaction - Done");
            return true;
        }

        bool FirebirdDBConnection::transactionActive()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            return m_transactionActive;
        }

        void FirebirdDBConnection::commit()
        {
            FIREBIRD_DEBUG("FirebirdConnection::commit - Starting");
#if DB_DRIVER_THREAD_SAFE
            FIREBIRD_DEBUG("  Acquiring lock...");
            DB_DRIVER_LOCK_GUARD(m_connMutex);
            FIREBIRD_DEBUG("  Lock acquired");
#endif
            FIREBIRD_DEBUG("  Calling endTransaction(true)...");
            endTransaction(true);
            FIREBIRD_DEBUG("  endTransaction completed");

            if (m_autoCommit)
            {
                FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
                startTransaction();
                FIREBIRD_DEBUG("  startTransaction completed");
            }
            FIREBIRD_DEBUG("FirebirdConnection::commit - Done");
        }

        void FirebirdDBConnection::rollback()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            endTransaction(false);

            if (m_autoCommit)
            {
                startTransaction();
            }
        }

        void FirebirdDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            // If the isolation level is already set to the requested level, do nothing
            if (m_isolationLevel == level)
            {
                return;
            }

            // If a transaction is active, we need to end it first, change the isolation level,
            // and restart the transaction with the new isolation level
            bool hadActiveTransaction = m_transactionActive;
            if (m_transactionActive)
            {
                // Commit the current transaction (or rollback if autocommit is off)
                if (m_autoCommit)
                {
                    endTransaction(true); // Commit
                }
                else
                {
                    endTransaction(false); // Rollback
                }
            }

            m_isolationLevel = level;

            // Restart the transaction if we had one active and autocommit is on
            if (hadActiveTransaction && m_autoCommit)
            {
                startTransaction();
            }
        }

        TransactionIsolationLevel FirebirdDBConnection::getTransactionIsolation()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_connMutex);
#endif
            return m_isolationLevel;
        }

        std::string FirebirdDBConnection::getURL() const
        {
            return m_url;
        }

        uint64_t FirebirdDBConnection::executeCreateDatabase(const std::string &sql)
        {
            FIREBIRD_DEBUG("FirebirdConnection::executeCreateDatabase - Starting");
            FIREBIRD_DEBUG("  SQL: " << sql);

            ISC_STATUS_ARRAY status;
            isc_db_handle db = 0;
            isc_tr_handle tr = 0;

            // Execute CREATE DATABASE using isc_dsql_execute_immediate
            // Note: For CREATE DATABASE, we pass null handles and the SQL creates the database
            if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql.c_str(), SQL_DIALECT_V6, nullptr))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to create database or schema: " << errorMsg);
                throw DBException("G8H4I0J6K2L8", "Failed to create database/schema: " + errorMsg,
                                  system_utils::captureCallStack());
            }

            FIREBIRD_DEBUG("  Database created successfully!");

            // Detach from the newly created database
            if (db)
            {
                isc_detach_database(status, &db);
            }

            return 0; // CREATE DATABASE doesn't return affected rows
        }

        // ============================================================================
        // FirebirdDBDriver Implementation
        // ============================================================================

        FirebirdDBDriver::FirebirdDBDriver()
        {
            std::lock_guard<std::mutex> lock(s_initMutex);
            if (!s_initialized)
            {
                // Firebird doesn't require explicit initialization
                s_initialized = true;
            }
        }

        FirebirdDBDriver::~FirebirdDBDriver()
        {
            // Firebird doesn't require explicit cleanup
        }

        std::shared_ptr<RelationalDBConnection> FirebirdDBDriver::connectRelational(const std::string &url,
                                                                                    const std::string &user,
                                                                                    const std::string &password,
                                                                                    const std::map<std::string, std::string> &options)
        {
            std::string host;
            int port;
            std::string database;

            if (!parseURL(url, host, port, database))
            {
                throw DBException("D6E2F8A4B1C7", "Invalid Firebird URL: " + url, system_utils::captureCallStack());
            }

            return std::make_shared<FirebirdDBConnection>(host, port, database, user, password, options);
        }

        bool FirebirdDBDriver::acceptsURL(const std::string &url)
        {
            return url.find("cpp_dbc:firebird:") == 0;
        }

        int FirebirdDBDriver::command(const std::map<std::string, std::any> &params)
        {
            FIREBIRD_DEBUG("FirebirdDriver::command - Starting");

            // Get the command name
            auto cmdIt = params.find("command");
            if (cmdIt == params.end())
            {
                throw DBException("J1K7L3M9N5O1", "Missing 'command' parameter", system_utils::captureCallStack());
            }

            std::string cmd;
            try
            {
                cmd = std::any_cast<std::string>(cmdIt->second);
            }
            catch (const std::bad_any_cast &)
            {
                throw DBException("K2L8M4N0O6P2", "Invalid 'command' parameter type (expected string)",
                                  system_utils::captureCallStack());
            }

            FIREBIRD_DEBUG("  Command: " << cmd);

            if (cmd == "create_database")
            {
                // Extract required parameters
                std::string url, user, password;
                std::map<std::string, std::string> options;

                auto urlIt = params.find("url");
                if (urlIt == params.end())
                {
                    throw DBException("L3M9N5O1P7Q3", "Missing 'url' parameter for create_database",
                                      system_utils::captureCallStack());
                }
                try
                {
                    url = std::any_cast<std::string>(urlIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    throw DBException("M4N0O6P2Q8R4", "Invalid 'url' parameter type", system_utils::captureCallStack());
                }

                auto userIt = params.find("user");
                if (userIt == params.end())
                {
                    throw DBException("N5O1P7Q3R9S5", "Missing 'user' parameter for create_database",
                                      system_utils::captureCallStack());
                }
                try
                {
                    user = std::any_cast<std::string>(userIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    throw DBException("O6P2Q8R4S0T6", "Invalid 'user' parameter type", system_utils::captureCallStack());
                }

                auto passIt = params.find("password");
                if (passIt == params.end())
                {
                    throw DBException("P7Q3R9S5T1U7", "Missing 'password' parameter for create_database",
                                      system_utils::captureCallStack());
                }
                try
                {
                    password = std::any_cast<std::string>(passIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    throw DBException("Q8R4S0T6U2V8", "Invalid 'password' parameter type", system_utils::captureCallStack());
                }

                // Extract optional parameters
                auto pageSizeIt = params.find("page_size");
                if (pageSizeIt != params.end())
                {
                    try
                    {
                        options["page_size"] = std::any_cast<std::string>(pageSizeIt->second);
                    }
                    catch (const std::bad_any_cast &)
                    { /* ignore invalid type */
                    }
                }

                auto charsetIt = params.find("charset");
                if (charsetIt != params.end())
                {
                    try
                    {
                        options["charset"] = std::any_cast<std::string>(charsetIt->second);
                    }
                    catch (const std::bad_any_cast &)
                    { /* ignore invalid type */
                    }
                }

                // Call createDatabase
                createDatabase(url, user, password, options);
                return 0;
            }
            else
            {
                throw DBException("R9S5T1U7V3W9", "Unknown command: " + cmd, system_utils::captureCallStack());
            }
        }

        bool FirebirdDBDriver::createDatabase(const std::string &url,
                                              const std::string &user,
                                              const std::string &password,
                                              const std::map<std::string, std::string> &options)
        {
            std::string host;
            int port;
            std::string database;

            if (!parseURL(url, host, port, database))
            {
                throw DBException("H9I5J1K7L3M9", "Invalid Firebird URL: " + url, system_utils::captureCallStack());
            }

            // Build the Firebird connection string for CREATE DATABASE
            std::string fbConnStr;
            if (!host.empty() && host != "localhost" && host != "127.0.0.1")
            {
                fbConnStr = host;
                if (port != 3050 && port != 0)
                {
                    fbConnStr += "/" + std::to_string(port);
                }
                fbConnStr += ":";
            }
            fbConnStr += database;

            // Get page size from options
            std::string pageSize = "4096";
            auto it = options.find("page_size");
            if (it != options.end())
            {
                pageSize = it->second;
            }

            // Get charset from options
            std::string charset = "UTF8";
            it = options.find("charset");
            if (it != options.end())
            {
                charset = it->second;
            }

            // Build CREATE DATABASE SQL command
            std::string createDbSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                        "USER '" +
                                      user + "' "
                                             "PASSWORD '" +
                                      password + "' "
                                                 "PAGE_SIZE " +
                                      pageSize + " "
                                                 "DEFAULT CHARACTER SET " +
                                      charset;

            FIREBIRD_DEBUG("FirebirdDriver::createDatabase - Executing: " << createDbSql);

            ISC_STATUS_ARRAY status;
            isc_db_handle db = 0;
            isc_tr_handle tr = 0;

            // Execute CREATE DATABASE using isc_dsql_execute_immediate
            if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Failed to create database: " << errorMsg);
                throw DBException("I0J6K2L8M4N0", "Failed to create database: " + errorMsg,
                                  system_utils::captureCallStack());
            }

            FIREBIRD_DEBUG("  Database created successfully!");

            // Detach from the newly created database
            if (db)
            {
                isc_detach_database(status, &db);
            }

            return true;
        }

        bool FirebirdDBDriver::parseURL(const std::string &url, std::string &host, int &port, std::string &database)
        {
            // Expected formats:
            // cpp_dbc:firebird://host:port/path/to/database.fdb
            // cpp_dbc:firebird://host/path/to/database.fdb
            // cpp_dbc:firebird:///path/to/database.fdb (local)

            std::string workUrl = url;

            // Remove prefix - only accept cpp_dbc:firebird:// prefix
            if (workUrl.find("cpp_dbc:firebird://") == 0)
            {
                workUrl = workUrl.substr(19);
            }
            else
            {
                return false;
            }

            // Default values
            host = "localhost";
            port = 3050; // Default Firebird port

            // Check for local connection (starts with /)
            if (workUrl[0] == '/')
            {
                database = workUrl;
                return true;
            }

            // Find host:port/database
            size_t slashPos = workUrl.find('/');
            if (slashPos == std::string::npos)
            {
                return false;
            }

            std::string hostPort = workUrl.substr(0, slashPos);
            database = workUrl.substr(slashPos);

            // Parse host:port
            size_t colonPos = hostPort.find(':');
            if (colonPos != std::string::npos)
            {
                host = hostPort.substr(0, colonPos);
                try
                {
                    port = std::stoi(hostPort.substr(colonPos + 1));
                }
                catch (...)
                {
                    port = 3050;
                }
            }
            else
            {
                host = hostPort;
            }

            if (host.empty())
            {
                host = "localhost";
            }

            return !database.empty();
        }

    } // namespace Firebird
} // namespace cpp_dbc

#endif // USE_FIREBIRD