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

 @file connection_03.cpp
 @brief MySQL database driver implementation - MySQLDBConnection nothrow methods (part 2 - transactions)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    cpp_dbc::expected<void, DBException> MySQLDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("47FCEE77D4F3", "Connection is closed", system_utils::captureCallStack()));
            }

            using enum TransactionIsolationLevel;
            std::string query;
            switch (level)
            {
            case TRANSACTION_READ_UNCOMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                break;
            case TRANSACTION_READ_COMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED";
                break;
            case TRANSACTION_REPEATABLE_READ:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                break;
            case TRANSACTION_SERIALIZABLE:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                break;
            default:
                return cpp_dbc::unexpected(DBException("7Q8R9S0T1U2V", "Unsupported transaction isolation level", system_utils::captureCallStack()));
            }

            if (mysql_query(m_mysql.get(), query.c_str()) != 0)
            {
                return cpp_dbc::unexpected(DBException("3W4X5Y6Z7A8B", std::string("Failed to set transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            // Verify that the isolation level was actually set
            auto actualLevelResult = getTransactionIsolation(std::nothrow);
            if (!actualLevelResult)
            {
                return cpp_dbc::unexpected(actualLevelResult.error());
            }
            TransactionIsolationLevel actualLevel = *actualLevelResult;

            if (actualLevel != level)
            {
                // Some MySQL configurations might not allow certain isolation levels
                // In this case, we'll update our internal state to match what MySQL actually set
                this->m_isolationLevel = actualLevel;
            }
            else
            {
                this->m_isolationLevel = level;
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E3F9A5B1C7DB",
                                                   std::string("setTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E3F9A5B1C7DC",
                                                   "setTransactionIsolation failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> MySQLDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("9C0D1E2F3G4H", "Connection is closed", system_utils::captureCallStack()));
            }

            // If we're being called from setTransactionIsolation, return the cached value
            // to avoid potential infinite recursion
            static bool inGetTransactionIsolation = false;
            if (inGetTransactionIsolation)
            {
                return this->m_isolationLevel;
            }

            inGetTransactionIsolation = true;

            try // NOSONAR - nested try is intentional for separate cleanup logic
            {
                // Query the current isolation level (try newer syntax first, then fall back to older MySQL versions)
                if (mysql_query(m_mysql.get(), "SELECT @@transaction_isolation") != 0 &&
                    mysql_query(m_mysql.get(), "SELECT @@tx_isolation") != 0)
                {
                    inGetTransactionIsolation = false;
                    return cpp_dbc::unexpected(DBException("5I6J7K8L9M0N", std::string("Failed to get transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
                }

                MYSQL_RES *result = mysql_store_result(m_mysql.get());
                if (!result)
                {
                    inGetTransactionIsolation = false;
                    return cpp_dbc::unexpected(DBException("1O2P3Q4R5S6T", std::string("Failed to get result set: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
                }

                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    inGetTransactionIsolation = false;
                    return cpp_dbc::unexpected(DBException("7U8V9W0X1Y2Z", "Failed to fetch transaction isolation level", system_utils::captureCallStack()));
                }

                std::string level = row[0];
                mysql_free_result(result);

                // Convert the string value to the enum
                TransactionIsolationLevel isolationResult;
                if (level == "READ-UNCOMMITTED" || level == "READ_UNCOMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
                else if (level == "READ-COMMITTED" || level == "READ_COMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
                else if (level == "REPEATABLE-READ" || level == "REPEATABLE_READ")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
                else if (level == "SERIALIZABLE")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
                else
                    isolationResult = TransactionIsolationLevel::TRANSACTION_NONE;

                // Update our cached value
                this->m_isolationLevel = isolationResult;
                inGetTransactionIsolation = false;
                return isolationResult;
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                inGetTransactionIsolation = false;
                MYSQL_DEBUG("MySQLDBConnection::getTransactionIsolation - Exception during query: " << ex.what());
                throw;
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F4A0B6C2D8EB",
                                                   std::string("getTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
