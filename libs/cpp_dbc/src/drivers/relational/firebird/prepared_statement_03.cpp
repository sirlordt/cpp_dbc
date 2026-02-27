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

 @file prepared_statement_03.cpp
 @brief Firebird database driver implementation - FirebirdDBPreparedStatement nothrow methods (part 2)

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

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("6RG1MNZK18B1", "Connection lost");

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBLB04", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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

            m_blobObjects.push_back(x);
            std::vector<uint8_t> data = x->getBytes(0, x->length());
            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C6D7E8F9A0B1", std::string("Exception in setBlob: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D7E8F9A0B1C2", "Unknown exception in setBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("BX3YJGJ3LMQO", "Connection lost");

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBNS05", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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

            m_streamObjects.push_back(x);

            // Read all data from stream
            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
            }

            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E8F9A0B1C2D3", std::string("Exception in setBinaryStream: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F9A0B1C2D3E4", "Unknown exception in setBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("W53CBC9BYKCO", "Connection lost");

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBNL06", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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

            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A0B1C2D3E4F5", std::string("Exception in setBinaryStream(length): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B1C2D3E4F5A6", "Unknown exception in setBinaryStream(length)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        try
        {
            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBYT07", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C2D3E4F5A6B7", std::string("Exception in setBytes(vector): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D3E4F5A6B7C8", "Unknown exception in setBytes(vector)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("0Q4LMQ0ANYC8", "Connection lost");

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBYT08", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                return cpp_dbc::unexpected(DBException("C3D9E5F1A8B4", "Parameter index out of range: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
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
                    return cpp_dbc::unexpected(DBException("C3D9E5F1A8B5", "Connection has been closed", system_utils::captureCallStack()));
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
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E4F5A6B7C8D9", std::string("Exception in setBytes(pointer): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F5A6B7C8D9E0", "Unknown exception in setBytes(pointer)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> FirebirdDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_LOCK_OR_RETURN("FHMW3W23EJ01", "Connection lost");

            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeQuery(nothrow) - Starting");
            FIREBIRD_DEBUG("  m_closed: %s", (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: %p", (void*)(uintptr_t)m_stmt);

            if (m_closed.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("  Statement is closed!");
                return cpp_dbc::unexpected(DBException("D4E0F6A2B9C5", "Statement is closed", system_utils::captureCallStack()));
            }

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("  Statement was invalidated by DDL operation!");
                return cpp_dbc::unexpected(DBException("TGXPWHHQXNUJ", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            // FIX #1: Access transaction handle safely via m_connection
            auto conn = m_connection.lock();
            if (!conn)
            {
                FIREBIRD_DEBUG("  Connection destroyed!");
                return cpp_dbc::unexpected(DBException("FB9D2E3F4A5C", "Connection destroyed while executing query", system_utils::captureCallStack()));
            }

            ISC_STATUS_ARRAY status;

            // Execute the statement - now using conn->m_tr instead of m_trPtr
            FIREBIRD_DEBUG("  Executing statement with isc_dsql_execute...");
            FIREBIRD_DEBUG("    conn->m_tr=%ld", (long)conn->m_tr);
            FIREBIRD_DEBUG("    &m_stmt=%p, m_stmt=%p", (void*)&m_stmt, (void*)(uintptr_t)m_stmt);
            if (isc_dsql_execute(status, &(conn->m_tr), &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                std::string errorMsg = interpretStatusVector(status);
                FIREBIRD_DEBUG("  Execute failed: %s", errorMsg.c_str());

                // If autocommit is enabled, rollback the failed transaction and start a new one
                // Note: conn already locked earlier, just check if autocommit is enabled
                if (conn->getAutoCommit())
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

                return cpp_dbc::unexpected(DBException("E5F1A7B3C0D6", "Failed to execute query: " + errorMsg,
                                                       system_utils::captureCallStack()));
            }
            FIREBIRD_DEBUG("  Execute succeeded, m_stmt after execute=%p", (void*)(uintptr_t)m_stmt);

            // Allocate output SQLDA for results - need to copy and set up buffers
            int numCols = m_outputSqlda->sqld;
            FIREBIRD_DEBUG("  Output columns: %d", numCols);
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
                FIREBIRD_DEBUG("    Column %d: raw_sqltype=%d, type=%d, nullable=%d, len=%d",
                    i, (int)m_outputSqlda->sqlvar[i].sqltype, (int)(resultSqlda->sqlvar[i].sqltype & ~1),
                    (int)(m_outputSqlda->sqlvar[i].sqltype & 1), (int)resultSqlda->sqlvar[i].sqllen);
            }

            // IMPORTANT: Transfer ownership of the statement handle to the ResultSet
            // The ResultSet will own the statement and free it when closed
            // We set m_stmt to 0 so that PreparedStatement::close() won't try to free it
            FIREBIRD_DEBUG("  Transferring statement ownership to ResultSet");
            FIREBIRD_DEBUG("    m_stmt value: %u", (unsigned)m_stmt);
            FIREBIRD_DEBUG("    &m_stmt address: %p", (void*)&m_stmt);

            // Create a new handle with the statement value
            isc_stmt_handle *stmtPtr = new isc_stmt_handle(m_stmt);
            FIREBIRD_DEBUG("    stmtPtr: %p, *stmtPtr: %p", (void*)stmtPtr, (void*)(uintptr_t)*stmtPtr);

            // Transfer ownership - set our m_stmt to 0 so we don't free it in close()
            m_stmt = 0;
            FIREBIRD_DEBUG("    m_stmt after transfer: %p", (void*)(uintptr_t)m_stmt);

            FirebirdStmtHandle stmtHandle(stmtPtr);
            XSQLDAHandle sqldaHandle(resultSqlda);

            // ownStatement = true means the ResultSet will free the statement when closed
            FIREBIRD_DEBUG("  Creating FirebirdResultSet with ownStatement=true");
            // ResultSet no longer receives m_connMutex as parameter
            // It will access the mutex through m_connection when needed
            auto resultSet = std::make_shared<FirebirdDBResultSet>(std::move(stmtHandle), std::move(sqldaHandle), true, conn);

            // Register the ResultSet with the connection for automatic cleanup
            if (conn)
            {
                conn->registerResultSet(std::weak_ptr<FirebirdDBResultSet>(resultSet));
            }

            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeQuery(nothrow) - Done");
            return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>(std::static_pointer_cast<RelationalDBResultSet>(resultSet));
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A6B7C8D9E0F1", std::string("Exception in executeQuery: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B7C8D9E0F1A2", "Unknown exception in executeQuery", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
