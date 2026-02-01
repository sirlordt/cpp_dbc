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

 @file result_set_01.cpp
 @brief Firebird database driver - FirebirdDBResultSet (constructor, destructor, init, getColumnValue)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

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

        // Note: We don't need to unregister here. The weak_ptr in m_activeResultSets
        // will automatically expire when this object is destroyed, and
        // closeAllActiveResultSets() checks if weak_ptrs can be locked before using them.

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

    void FirebirdDBResultSet::notifyConnClosing()
    {

        DB_DRIVER_LOCK_GUARD(m_mutex);

        FIREBIRD_DEBUG("FirebirdResultSet::notifyConnClosing - Marking as closed due to connection closing");
        // Don't actually free the statement since the connection is closing
        // Just mark as closed to prevent further operations
        m_closed = true;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
