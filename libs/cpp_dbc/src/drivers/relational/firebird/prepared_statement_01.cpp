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

 @file prepared_statement_01.cpp
 @brief Firebird database driver implementation - FirebirdDBPreparedStatement throwing methods

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
    // FirebirdDBPreparedStatement Implementation - Private Methods
    // ============================================================================

    void FirebirdDBPreparedStatement::notifyConnClosing()
    {
        // No lock needed - m_closed is atomic
        m_closed.store(true, std::memory_order_release);
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
        FIREBIRD_DEBUG("  db handle: %p, *db: %ld", (void*)db, (db ? (long)*db : 0L));

        // Allocate statement handle
        FIREBIRD_DEBUG("  Allocating statement handle...");
        if (isc_dsql_allocate_statement(status, db, &m_stmt))
        {
            FIREBIRD_DEBUG("  Failed to allocate statement: %s", interpretStatusVector(status).c_str());
            throw DBException("E3F9A5B1C8D4", "Failed to allocate statement: " + interpretStatusVector(status),
                              system_utils::captureCallStack());
        }
        FIREBIRD_DEBUG("  Statement allocated, m_stmt=%p", (void*)(uintptr_t)m_stmt);

        // Allocate output SQLDA
        FIREBIRD_DEBUG("  Allocating output SQLDA...");
        XSQLDA *outputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(20)));
        outputSqlda->sqln = 20;
        outputSqlda->version = SQLDA_VERSION1;
        m_outputSqlda.reset(outputSqlda);

        // Prepare the statement
        FIREBIRD_DEBUG("  Preparing statement with SQL: %s", m_sql.c_str());

        // FIX #1: Access transaction handle safely via m_connection
        auto conn = m_connection.lock();
        if (!conn)
        {
            std::string errorMsg = "Connection destroyed while preparing statement";
            FIREBIRD_DEBUG("  Failed to prepare statement: %s", errorMsg.c_str());
            m_outputSqlda.reset();
            ISC_STATUS_ARRAY freeStatus;
            isc_dsql_free_statement(freeStatus, &m_stmt, DSQL_drop);
            throw DBException("FB9D2E3F4A5B", errorMsg, system_utils::captureCallStack());
        }

        FIREBIRD_DEBUG("  conn->m_tr=%ld", (long)conn->m_tr);
        if (isc_dsql_prepare(status, &(conn->m_tr), &m_stmt, 0, m_sql.c_str(), SQL_DIALECT_V6, m_outputSqlda.get()))
        {
            // Save the error message BEFORE calling any other Firebird API functions
            // because they will overwrite the status vector
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to prepare statement: %s", errorMsg.c_str());
            m_outputSqlda.reset();
            ISC_STATUS_ARRAY freeStatus; // Use separate status array for cleanup
            isc_dsql_free_statement(freeStatus, &m_stmt, DSQL_drop);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            throw DBException("F4A0B6C2D9E5", "Failed to prepare statement: " + errorMsg,
                              system_utils::captureCallStack());
        }
        FIREBIRD_DEBUG("  Statement prepared, m_stmt=%p, output columns=%d", (void*)(uintptr_t)m_stmt, (int)m_outputSqlda->sqld);

        // Reallocate output SQLDA if needed
        if (m_outputSqlda->sqld > m_outputSqlda->sqln)
        {
            int n = m_outputSqlda->sqld;
            FIREBIRD_DEBUG("  Reallocating output SQLDA for %d columns", n);
            XSQLDA *newOutputSqlda = reinterpret_cast<XSQLDA *>(malloc(XSQLDA_LENGTH(n)));
            newOutputSqlda->sqln = static_cast<short>(n);
            newOutputSqlda->version = SQLDA_VERSION1;
            m_outputSqlda.reset(newOutputSqlda);

            if (isc_dsql_describe(status, &m_stmt, SQL_DIALECT_V6, m_outputSqlda.get()))
            {
                FIREBIRD_DEBUG("  Failed to describe statement: %s", interpretStatusVector(status).c_str());
                throw DBException("A5B1C7D3E0F6", "Failed to describe statement: " + interpretStatusVector(status),
                                  system_utils::captureCallStack());
            }
        }

        // Allocate input SQLDA
        FIREBIRD_DEBUG("  Allocating input SQLDA...");
        allocateInputSqlda();

        m_prepared = true;
        FIREBIRD_DEBUG("FirebirdPreparedStatement::prepareStatement - Done, m_stmt=%p", (void*)(uintptr_t)m_stmt);
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

    // ============================================================================
    // FirebirdDBPreparedStatement Implementation - Public Methods
    // ============================================================================

    FirebirdDBPreparedStatement::FirebirdDBPreparedStatement(std::weak_ptr<isc_db_handle> db,
                                                             const std::string &sql,
                                                             std::weak_ptr<FirebirdDBConnection> conn)
        : m_dbHandle(db), m_connection(conn), m_stmt(0), m_sql(sql),
          m_inputSqlda(nullptr), m_outputSqlda(nullptr)
    {
        FIREBIRD_DEBUG("FirebirdPreparedStatement::constructor - Creating statement");
        FIREBIRD_DEBUG("  SQL: %s", sql.c_str());
        // No longer receives m_connMutex - accesses it through m_connection when needed
        prepareStatement();
        m_closed.store(false, std::memory_order_release);
        FIREBIRD_DEBUG("FirebirdPreparedStatement::constructor - Done, m_stmt=%p", (void*)(uintptr_t)m_stmt);
    }

    FirebirdDBPreparedStatement::~FirebirdDBPreparedStatement()
    {
        FIREBIRD_DEBUG("FirebirdPreparedStatement::destructor - Destroying statement, m_stmt=%p", (void*)(uintptr_t)m_stmt);
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
        FIREBIRD_DEBUG("FirebirdPreparedStatement::destructor - Done");
    }

    void FirebirdDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = setInt(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setLong(int parameterIndex, int64_t value)
    {
        auto result = setLong(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setDouble(int parameterIndex, double value)
    {
        auto result = setDouble(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = setString(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBoolean(int parameterIndex, bool value)
    {
        auto result = setBoolean(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setNull(int parameterIndex, Types type)
    {
        auto result = setNull(std::nothrow, parameterIndex, type);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = setDate(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = setTimestamp(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setTime(int parameterIndex, const std::string &value)
    {
        auto result = setTime(std::nothrow, parameterIndex, value);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
    {
        auto result = setBlob(std::nothrow, parameterIndex, x);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x, length);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x, length);
        if (!result)
        {
            throw result.error();
        }
    }

    std::shared_ptr<RelationalDBResultSet> FirebirdDBPreparedStatement::executeQuery()
    {
        auto result = executeQuery(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBPreparedStatement::executeUpdate()
    {
        auto result = executeUpdate(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBPreparedStatement::execute()
    {
        auto result = execute(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBPreparedStatement::close()
    {
        auto result = close(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
