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

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

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
            DB_DRIVER_LOCK_GUARD(m_mutex);

            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeQuery(nothrow) - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);
            FIREBIRD_DEBUG("  m_trPtr: " << m_trPtr << ", *m_trPtr: " << (m_trPtr ? *m_trPtr : 0));

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Statement is closed!");
                return cpp_dbc::unexpected(DBException("D4E0F6A2B9C5", "Statement is closed", system_utils::captureCallStack()));
            }

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("  Statement was invalidated by DDL operation!");
                return cpp_dbc::unexpected(DBException("FB1NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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

                return cpp_dbc::unexpected(DBException("E5F1A7B3C0D6", "Failed to execute query: " + errorMsg,
                                                       system_utils::captureCallStack()));
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

    cpp_dbc::expected<uint64_t, DBException> FirebirdDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Starting");

            DB_DRIVER_LOCK_GUARD(m_mutex);

            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("F6A2B8C4D1E7", "Statement is closed", system_utils::captureCallStack()));
            }

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("  Statement was invalidated by DDL operation!");
                return cpp_dbc::unexpected(DBException("FB2NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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

                return cpp_dbc::unexpected(DBException("A7B3C9D5E2F8", "Failed to execute update: " + errorMsg,
                                                       system_utils::captureCallStack()));
            }
            FIREBIRD_DEBUG("  isc_dsql_execute succeeded");

            // Get affected rows count
            char infoBuffer[64] = {0}; // Initialize to zero to avoid valgrind warnings
            char resultBuffer[64] = {0};
            infoBuffer[0] = isc_info_sql_records;
            infoBuffer[1] = isc_info_end;

            if (isc_dsql_sql_info(status, &m_stmt, sizeof(infoBuffer), infoBuffer, sizeof(resultBuffer), resultBuffer))
            {
                FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Failed to get sql_info, checking autocommit");
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
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Checking autocommit");
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

            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Done, returning count=" << count);
            return count;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C8D9E0F1A2B3", std::string("Exception in executeUpdate: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D9E0F1A2B3C4", "Unknown exception in executeUpdate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("B8C4D0E6F3A9", "Statement is closed", system_utils::captureCallStack()));
            }

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FB3NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            ISC_STATUS_ARRAY status;

            if (isc_dsql_execute(status, m_trPtr, &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
            {
                std::string errorMsg = interpretStatusVector(status);
                return cpp_dbc::unexpected(DBException("FBCX4Y5Z6A7B", "Failed to execute statement: " + errorMsg,
                                                       system_utils::captureCallStack()));
            }

            return m_outputSqlda->sqld > 0; // Returns true if there are result columns
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E0F1A2B3C4D5", std::string("Exception in execute: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F1A2B3C4D5E6", "Unknown exception in execute", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            FIREBIRD_DEBUG("FirebirdPreparedStatement::close(nothrow) - Starting");
            FIREBIRD_DEBUG("  m_closed: " << (m_closed ? "true" : "false"));
            FIREBIRD_DEBUG("  m_stmt: " << m_stmt);

            if (m_closed)
            {
                FIREBIRD_DEBUG("  Already closed, returning");
                return {};
            }

            ISC_STATUS_ARRAY status;

            if (m_stmt)
            {
                FIREBIRD_DEBUG("  Freeing statement with DSQL_drop...");
                // Only try to free if the handle is valid
                if (m_stmt != 0)
                {
                    // Create a local copy of the statement handle
                    isc_stmt_handle localStmt = m_stmt;

                    // Free the statement using the local copy
                    isc_dsql_free_statement(status, &localStmt, DSQL_drop);
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                    FIREBIRD_DEBUG("  Statement freed");
                }
                m_stmt = 0;
            }

            // Smart pointers will handle cleanup automatically
            FIREBIRD_DEBUG("  Resetting smart pointers");
            m_inputSqlda.reset();
            m_outputSqlda.reset();

            m_closed = true;
            FIREBIRD_DEBUG("FirebirdPreparedStatement::close(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C1B8225A38F6", std::string("Exception in close: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("9C4BC01FFDD7", "Unknown exception in close", system_utils::captureCallStack()));
        }
    }

    void FirebirdDBPreparedStatement::invalidate()
    {
        FIREBIRD_DEBUG("FirebirdPreparedStatement::invalidate - Starting");

        // Set the invalidated flag first (atomic operation)
        m_invalidated.store(true, std::memory_order_release);

        // Close the statement to release metadata locks
        auto closeResult = close(std::nothrow);
        if (!closeResult)
        {
            FIREBIRD_DEBUG("  close() failed during invalidation: " << closeResult.error().what());
        }

        FIREBIRD_DEBUG("FirebirdPreparedStatement::invalidate - Done");
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
