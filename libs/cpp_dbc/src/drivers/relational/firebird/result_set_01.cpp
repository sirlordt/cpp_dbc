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
 @brief Firebird database driver - FirebirdDBResultSet (private nothrow constructor, helpers, destructor, create factories)

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
    //
    // IMPORTANT: Firebird uses a CURSOR-BASED model where isc_dsql_fetch()
    // communicates with the database connection handle on every call. This is different
    // from MySQL/PostgreSQL which load all results into client memory (MYSQL_RES*/PGresult*).
    //
    // Because of this, FirebirdDBResultSet MUST share the same mutex as FirebirdDBConnection
    // to prevent race conditions when multiple threads access the same connection
    // (e.g., one thread iterating results while another does pool validation).
    // ============================================================================

    // ── Private nothrow constructor ───────────────────────────────────────────

    FirebirdDBResultSet::FirebirdDBResultSet(PrivateCtorTag,
                                             std::nothrow_t,
                                             FirebirdStmtHandle stmt,
                                             XSQLDAHandle sqlda,
                                             bool ownStatement,
                                             std::shared_ptr<FirebirdDBConnection> conn) noexcept
        : m_stmt(std::move(stmt)), m_sqlda(std::move(sqlda)), m_ownStatement(ownStatement), m_connection(conn)
    {
        FIREBIRD_DEBUG("FirebirdResultSet::constructor - Creating ResultSet");
        FIREBIRD_DEBUG("  ownStatement: %s", (ownStatement ? "true" : "false"));
        FIREBIRD_DEBUG("  m_stmt valid: %s", (m_stmt ? "yes" : "no"));
        if (m_stmt)
        {
            FIREBIRD_DEBUG("  m_stmt handle value: %p", (void*)(uintptr_t)*m_stmt);
        }
        FIREBIRD_DEBUG("  m_sqlda valid: %s", (m_sqlda ? "yes" : "no"));

        if (m_sqlda)
        {
            m_fieldCount = static_cast<size_t>(m_sqlda->sqld);
            FIREBIRD_DEBUG("  Field count: %zu", m_fieldCount);
            auto initResult = initializeColumns(std::nothrow);
            if (!initResult.has_value())
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>(std::move(initResult.error()));
                FIREBIRD_DEBUG("  initializeColumns failed: %s", m_initError->what_s().data());
                return;
            }
        }
        m_closed.store(false, std::memory_order_seq_cst);
        FIREBIRD_DEBUG("FirebirdResultSet::constructor - Done");
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> FirebirdDBResultSet::initializeColumns(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - Starting");
        if (!m_sqlda)
        {
            FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - m_sqlda is null, returning");
            return {};
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
            FIREBIRD_DEBUG("  Column %zu: %s (raw_sqltype=%d, type=%d, nullable=%d, len=%d, scale=%d)",
                i, colName.c_str(), (int)var->sqltype, (int)(var->sqltype & ~1),
                (int)(var->sqltype & 1), (int)var->sqllen, (int)var->sqlscale);

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
            FIREBIRD_DEBUG("    Buffer %zu: size=%zu, sqldata=%p, sqlind=%p, *sqlind=%d",
                i, bufferSize, (void*)var->sqldata, (void*)var->sqlind, (int)*var->sqlind);
        }
        FIREBIRD_DEBUG("FirebirdResultSet::initializeColumns - Done");
        return {};
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getColumnValue(std::nothrow_t, size_t columnIndex) const noexcept
    {
        FIREBIRD_DEBUG("getColumnValue: columnIndex=%zu, m_fieldCount=%zu", columnIndex, m_fieldCount);
        if (columnIndex >= m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("A7B3C9D2E5F1", "Column index out of range: " + std::to_string(columnIndex),
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  nullIndicator=%d", (int)m_nullIndicators[columnIndex]);
        if (m_nullIndicators[columnIndex] < 0)
        {
            FIREBIRD_DEBUG("  returning empty (NULL)");
            return std::string{};
        }

        XSQLVAR *var = &m_sqlda->sqlvar[columnIndex];
        short sqlType = var->sqltype & ~1;
        FIREBIRD_DEBUG("  sqlType=%d, sqllen=%d, sqlscale=%d", (int)sqlType, (int)var->sqllen, (int)var->sqlscale);
        FIREBIRD_DEBUG("  sqldata=%p", (void*)var->sqldata);

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
            FIREBIRD_DEBUG("getColumnValue SQL_INT64: columnIndex=%zu, sqldata=%p, sqllen=%d, sqlscale=%d, raw_value=%lld",
                columnIndex, (void*)var->sqldata, (int)var->sqllen, (int)var->sqlscale, (long long)value);
            if (var->sqlscale < 0)
            {
                double scaled = static_cast<double>(value) / std::pow(10.0, -var->sqlscale);
                FIREBIRD_DEBUG("  scaled_value=%f", scaled);
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
                return std::string{"[BLOB]"}; // Can't read without connection
            }

            ISC_QUAD *blobId = reinterpret_cast<ISC_QUAD *>(var->sqldata);
            auto blobResult = FirebirdBlob::create(std::nothrow, m_connection, *blobId);
            if (!blobResult.has_value())
            {
                FIREBIRD_DEBUG("  getColumnValue BLOB create failed: %s", blobResult.error().what_s().data());
                return std::string{"[BLOB]"};
            }
            auto blob = blobResult.value();
            auto lenResult = blob->length(std::nothrow);
            if (!lenResult.has_value())
            {
                FIREBIRD_DEBUG("  getColumnValue BLOB length failed: %s", lenResult.error().what_s().data());
                return std::string{"[BLOB]"};
            }
            auto dataResult = blob->getBytes(std::nothrow, 0, lenResult.value());
            if (!dataResult.has_value())
            {
                FIREBIRD_DEBUG("  getColumnValue BLOB read failed: %s", dataResult.error().what_s().data());
                return std::string{"[BLOB]"};
            }
            return std::string(dataResult.value().begin(), dataResult.value().end());
        }
        default:
            return std::string{};
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBResultSet::notifyConnClosing(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdResultSet::notifyConnClosing - Marking as closed due to connection closing");
        // Don't actually free the statement since the connection is closing.
        // Just mark as closed to prevent further operations.
        // No lock needed — m_closed is atomic.
        m_closed.store(true, std::memory_order_seq_cst);
        return {};
    }

    cpp_dbc::expected<void, DBException> FirebirdDBResultSet::initialize(std::nothrow_t) noexcept
    {
        // Register with Connection — mandatory; every ResultSet must be tracked
        // so closeAllResultSets()/notifyConnClosing() can reach it.
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("60PP1LI60QF3",
                "Connection expired before result set could be registered",
                system_utils::captureCallStack()));
        }
        auto regResult = conn->registerResultSet(std::nothrow, std::weak_ptr<FirebirdDBResultSet>(shared_from_this()));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }
        return {};
    }

    // ── Destructor ────────────────────────────────────────────────────────────

    FirebirdDBResultSet::~FirebirdDBResultSet()
    {
        FIREBIRD_DEBUG("FirebirdResultSet::destructor - Destroying ResultSet");

        // Note: We don't need to unregister here. The weak_ptr in m_activeResultSets
        // will automatically expire when this object is destroyed, and
        // closeAllResultSets() checks if weak_ptrs can be locked before using them.

        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        auto closeResult = close(std::nothrow);
        if (!closeResult.has_value())
        {
            FIREBIRD_DEBUG("FirebirdResultSet::destructor - close() failed: %s", closeResult.error().what_s().data());
        }
        FIREBIRD_DEBUG("FirebirdResultSet::destructor - Done");
    }

    // ── Create factories ──────────────────────────────────────────────────────

#ifdef __cpp_exceptions
    std::shared_ptr<FirebirdDBResultSet>
    FirebirdDBResultSet::create(FirebirdStmtHandle stmt,
                                XSQLDAHandle sqlda,
                                bool ownStatement,
                                std::shared_ptr<FirebirdDBConnection> conn)
    {
        auto r = create(std::nothrow, std::move(stmt), std::move(sqlda), ownStatement, conn);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }
#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<FirebirdDBResultSet>, DBException>
    FirebirdDBResultSet::create(std::nothrow_t,
                                FirebirdStmtHandle stmt,
                                XSQLDAHandle sqlda,
                                bool ownStatement,
                                std::shared_ptr<FirebirdDBConnection> conn) noexcept
    {
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // Constructor errors are captured in m_initFailed / m_initError.
        auto ptr = std::make_shared<FirebirdDBResultSet>(
            PrivateCtorTag{}, std::nothrow, std::move(stmt), std::move(sqlda), ownStatement, conn);
        if (ptr->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*ptr->m_initError));
        }
        // Must be called after construction (requires shared_ptr to exist for shared_from_this())
        auto initResult = ptr->initialize(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return ptr;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
