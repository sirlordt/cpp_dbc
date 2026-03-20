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

 @file connection_02.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBConnection nothrow methods (part 1 - query and autocommit)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // Nothrow API implementation for PostgreSQLDBConnection
    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> PostgreSQLDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("7W8X9Y0Z1A2B", "Connection is closed");

        // Generate a unique statement name and pass weak_ptr to the connection
        // so the statement can safely detect when connection is closed
        std::string stmtName = generateStatementName(std::nothrow);
        auto self = weak_from_this();
        if (self.expired())
        {
            return cpp_dbc::unexpected<DBException>(DBException("7W8X9Y0Z1A2C", "Connection is not owned by shared_ptr", system_utils::captureCallStack()));
        }
        auto stmtResult = PostgreSQLDBPreparedStatement::create(std::nothrow, self, sql, stmtName);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(stmtResult.error());
        }
        auto stmt = stmtResult.value();

        // Register the statement as weak_ptr - allows natural destruction when user releases reference.
        // Statements will be explicitly closed in returnToPool() or close() before connection reuse.
        // If registration fails the statement is NOT returned — destructor closes it.
        auto regResult = registerStatement(std::nothrow, std::weak_ptr<PostgreSQLDBPreparedStatement>(stmt));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }

        return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("3C4D5E6F7G8H", "Connection is closed");

        PGresultHandle result(PQexec(m_conn.get(), sql.c_str()));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("6NSV59A3RDC7", "Query failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_TUPLES_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("9I0J1K2L3M4N", "Query failed: " + error, system_utils::captureCallStack()));
        }

        auto selfWeak = weak_from_this();
        if (selfWeak.expired())
        {
            return cpp_dbc::unexpected<DBException>(DBException("9I0J1K2L3M4O", "Connection is not owned by shared_ptr", system_utils::captureCallStack()));
        }
        auto rsResult = PostgreSQLDBResultSet::create(std::nothrow, selfWeak, result.release());
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }
        auto rs = rsResult.value();

        // Registration with the connection is done inside create() → initialize()
        return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>(std::static_pointer_cast<RelationalDBResultSet>(rs));
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("5O6P7Q8R9S0T", "Connection is closed");

        PGresultHandle result(PQexec(m_conn.get(), sql.c_str()));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("BZXECC4BPVXM", "Update failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("1U2V3W4X5Y6Z", "Update failed: " + error, system_utils::captureCallStack()));
        }

        // Get the number of affected rows
        const char *affectedRows = PQcmdTuples(result.get());
        uint64_t rowCount = 0;
        if (affectedRows && affectedRows[0] != '\0')
        {
            std::string_view affectedRowsView(affectedRows);
            auto [ptr, ec] = std::from_chars(affectedRowsView.data(), affectedRowsView.data() + affectedRowsView.size(), rowCount);
            if (ec != std::errc{})
            {
                return cpp_dbc::unexpected<DBException>(DBException("JH2IMLZPHQKD", "Failed to parse affected row count", system_utils::captureCallStack()));
            }
        }

        return rowCount;
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("R4S5T6U7V8W9", "Connection is closed");

        // Only take action if the flag is actually changing
        if (this->m_autoCommit != autoCommitFlag)
        {
            if (!autoCommitFlag)
            {
                // Disabling autoCommit — start an explicit transaction first.
                // PostgreSQL is always in autocommit mode until a transaction is started.
                // Only flip the flag after BEGIN succeeds to avoid inconsistent state.
                auto result = beginTransaction(std::nothrow);
                if (!result.has_value())
                {
                    return cpp_dbc::unexpected<DBException>(result.error());
                }

                this->m_autoCommit = false;
            }
            else
            {
                // If we're enabling autocommit, we must ensure any active transaction is ended
                if (m_transactionActive)
                {
                    auto result = commit(std::nothrow);
                    if (!result.has_value())
                    {
                        return cpp_dbc::unexpected<DBException>(result.error());
                    }
                }

                this->m_autoCommit = true;
            }
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("9D3F5A7C2E8B", "Connection is closed");

        return m_autoCommit;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("S5T6U7V8W9X0", "Connection is closed");

        // If transaction is already active, just return true
        if (m_transactionActive)
        {
            return true;
        }

        // Start a transaction
        std::string beginCmd = "BEGIN";

        // For SERIALIZABLE isolation, we need special handling
        if (m_isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
        {
            beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";

            // Execute the BEGIN command
            PGresultHandle dummyResult(PQexec(m_conn.get(), beginCmd.c_str()));
            if (!dummyResult.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("M5UCTHN8L2QR", "Failed to start SERIALIZABLE transaction (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(dummyResult.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(dummyResult.get());
                return cpp_dbc::unexpected<DBException>(DBException("3G4H5I6J7K8L", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
            }

            // Force snapshot acquisition with a dummy query
            PGresultHandle snapshotResult(PQexec(m_conn.get(), "SELECT 1"));
            if (!snapshotResult.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                return cpp_dbc::unexpected<DBException>(DBException("VQ8V8AP5Y77Z", "Failed to acquire snapshot (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(snapshotResult.get()) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(snapshotResult.get());
                // BEGIN succeeded above, so the server is inside an open transaction.
                // Roll it back before returning to keep the server state consistent
                // with the client object (m_transactionActive is still false here).
                [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                return cpp_dbc::unexpected<DBException>(DBException("9M0N1O2P3Q4R", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
            }
        }
        else
        {
            // Standard BEGIN for other isolation levels
            PGresultHandle result(PQexec(m_conn.get(), beginCmd.c_str()));
            if (!result.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("H6D90ADC9XML", "Failed to start transaction (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result.get());
                return cpp_dbc::unexpected<DBException>(DBException("5S6T7U8V9W0X", "Failed to start transaction: " + error, system_utils::captureCallStack()));
            }
        }

        m_autoCommit = false;
        m_transactionActive = true;
        return true;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("5F8B2E9A7D3C", "Connection is closed");

        return m_transactionActive;
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::commit(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("7E8F9G0H1I2J", "Connection is closed");

        // If no transaction is active, nothing to commit
        if (!m_transactionActive)
        {
            return {};
        }

        PGresultHandle result(PQexec(m_conn.get(), "COMMIT"));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("KRBTT2PHNR0Z", "Commit failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("3K4L5M6N7O8P", "Commit failed: " + error, system_utils::captureCallStack()));
        }

        m_transactionActive = false;

        // If autoCommit is still false, start a new transaction automatically.
        // PostgreSQL (unlike MySQL) does not implicitly start a new transaction
        // after COMMIT — an explicit BEGIN is required.
        if (!m_autoCommit)
        {
            PGresultHandle beginRes(PQexec(m_conn.get(), "BEGIN"));
            if (!beginRes.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("X50CV1WLJCV3",
                    "Failed to restart transaction after commit (OOM): " + error,
                    system_utils::captureCallStack()));
            }
            if (PQresultStatus(beginRes.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(beginRes.get());
                return cpp_dbc::unexpected<DBException>(DBException("PQ6YETSTZ1VH",
                    "Failed to restart transaction after commit: " + error,
                    system_utils::captureCallStack()));
            }
            m_transactionActive = true;
        }

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::rollback(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("5W6X7Y8Z9A0B", "Connection is closed");

        // If no transaction is active, nothing to rollback
        if (!m_transactionActive)
        {
            return {};
        }

        PGresultHandle result(PQexec(m_conn.get(), "ROLLBACK"));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("5B5Q7WTALLL6", "Rollback failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("1C2D3E4F5G6H", "Rollback failed: " + error, system_utils::captureCallStack()));
        }

        m_transactionActive = false;

        // If autoCommit is still false, start a new transaction automatically.
        // PostgreSQL (unlike MySQL) does not implicitly start a new transaction
        // after ROLLBACK — an explicit BEGIN is required.
        if (!m_autoCommit)
        {
            PGresultHandle beginRes(PQexec(m_conn.get(), "BEGIN"));
            if (!beginRes.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("7X6KAFKVI9VC",
                    "Failed to restart transaction after rollback (OOM): " + error,
                    system_utils::captureCallStack()));
            }
            if (PQresultStatus(beginRes.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(beginRes.get());
                return cpp_dbc::unexpected<DBException>(DBException("C0JFJM05WL73",
                    "Failed to restart transaction after rollback: " + error,
                    system_utils::captureCallStack()));
            }
            m_transactionActive = true;
        }

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("3O4P5Q6R7S8T", "Connection is closed");

        using enum TransactionIsolationLevel;
        std::string query;
        switch (level)
        {
        case TRANSACTION_READ_UNCOMMITTED:
        {
            // PostgreSQL treats READ UNCOMMITTED the same as READ COMMITTED
            query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
            break;
        }
        case TRANSACTION_READ_COMMITTED:
        {
            query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED";
            break;
        }
        case TRANSACTION_REPEATABLE_READ:
        {
            query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ";
            break;
        }
        case TRANSACTION_SERIALIZABLE:
        {
            query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE";
            break;
        }
        default:
        {
            return cpp_dbc::unexpected<DBException>(DBException("9U0V1W2X3Y4Z", "Unsupported transaction isolation level", system_utils::captureCallStack()));
        }
        }

        // Cannot change isolation level while a transaction is active — the caller
        // must commit or rollback first. Silently committing would destroy in-flight work.
        if (m_transactionActive)
        {
            return cpp_dbc::unexpected<DBException>(DBException("USA7EDKRMV5D",
                "Cannot change transaction isolation level while a transaction is active",
                system_utils::captureCallStack()));
        }

        // SET SESSION CHARACTERISTICS applies to future transactions, not the current one.
        // Safe to execute outside a transaction.
        {
            PGresultHandle result(PQexec(m_conn.get(), query.c_str()));
            if (!result.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("NDRIRZ2EHLVU", "Failed to set transaction isolation level (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result.get());
                return cpp_dbc::unexpected<DBException>(DBException("3Q37JJHOWQJE", "Failed to set transaction isolation level: " + error, system_utils::captureCallStack()));
            }
        }

        // If autoCommit is false, start a new transaction with the new isolation level.
        // m_transactionActive is false here (checked above), so we need a fresh BEGIN.
        if (!m_autoCommit)
        {
            if (level == TRANSACTION_SERIALIZABLE)
            {
                std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                PGresultHandle beginResult(PQexec(m_conn.get(), beginCmd.c_str()));
                if (!beginResult.get())
                {
                    std::string error = PQerrorMessage(m_conn.get());
                    return cpp_dbc::unexpected<DBException>(DBException("M5A3J5W4WDCS", "Failed to start SERIALIZABLE transaction (OOM): " + error, system_utils::captureCallStack()));
                }
                if (PQresultStatus(beginResult.get()) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(beginResult.get());
                    return cpp_dbc::unexpected<DBException>(DBException("V8W9X0Y1Z2A3", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
                }

                // Force snapshot acquisition with a dummy query
                PGresultHandle snapshotResult(PQexec(m_conn.get(), "SELECT 1"));
                if (!snapshotResult.get())
                {
                    std::string error = PQerrorMessage(m_conn.get());
                    [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                    return cpp_dbc::unexpected<DBException>(DBException("7TC8S1ZMFYZ3", "Failed to acquire snapshot (OOM): " + error, system_utils::captureCallStack()));
                }
                if (PQresultStatus(snapshotResult.get()) != PGRES_TUPLES_OK)
                {
                    std::string error = PQresultErrorMessage(snapshotResult.get());
                    // BEGIN succeeded above, so the server is inside an open transaction.
                    // Roll it back before returning to keep the server state consistent
                    // with the client object (m_transactionActive is still false here).
                    [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                    return cpp_dbc::unexpected<DBException>(DBException("3S4T5U6V7W8X", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
                }
            }
            else
            {
                PGresultHandle beginResult(PQexec(m_conn.get(), "BEGIN"));
                if (!beginResult.get())
                {
                    std::string error = PQerrorMessage(m_conn.get());
                    return cpp_dbc::unexpected<DBException>(DBException("5HLZWRY3M33J", "Failed to start transaction (OOM): " + error, system_utils::captureCallStack()));
                }
                if (PQresultStatus(beginResult.get()) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(beginResult.get());
                    return cpp_dbc::unexpected<DBException>(DBException("9Y0Z1A2B3C4D", "Failed to start transaction: " + error, system_utils::captureCallStack()));
                }
            }

            m_transactionActive = true;
        }

        // Only update m_isolationLevel after the full sequence succeeds
        this->m_isolationLevel = level;

        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> PostgreSQLDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("5E6F7G8H9I0J", "Connection is closed");

        // Query the current isolation level
        PGresultHandle result(PQexec(m_conn.get(), "SHOW transaction_isolation"));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("389O3M7IEVO1", "Failed to get transaction isolation level (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_TUPLES_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("3W4X5Y6Z7A8B", "Failed to get transaction isolation level: " + error, system_utils::captureCallStack()));
        }

        if (PQntuples(result.get()) == 0)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9C0D1E2F3G4H", "Failed to fetch transaction isolation level", system_utils::captureCallStack()));
        }

        std::string level = PQgetvalue(result.get(), 0, 0);

        // Convert the string value to the enum - handle both formats
        std::string levelLower = level;
        // Convert to lowercase for case-insensitive comparison
        std::ranges::transform(levelLower, levelLower.begin(),
                               [](unsigned char c)
                               {
                                   return std::tolower(c);
                               });

        if (levelLower == "read uncommitted" || levelLower == "read_uncommitted")
        {
            return TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
        }
        else if (levelLower == "read committed" || levelLower == "read_committed")
        {
            return TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
        }
        else if (levelLower == "repeatable read" || levelLower == "repeatable_read")
        {
            return TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
        }
        else if (levelLower == "serializable")
        {
            return TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
        }
        else
        {
            return TransactionIsolationLevel::TRANSACTION_NONE;
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("36M3T260UN4J", "Connection is closed");

        return formatServerVersion(std::nothrow, PQserverVersion(m_conn.get()));
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> PostgreSQLDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN("IB9HNOSWLXD7", "Connection is closed");

        std::map<std::string, std::string> info;

        int version = PQserverVersion(m_conn.get());
        info["ServerVersion"] = formatServerVersion(std::nothrow, version);
        info["ServerVersionNumeric"] = std::to_string(version);

        int protocolVersion = PQprotocolVersion(m_conn.get());
        info["ProtocolVersion"] = std::to_string(protocolVersion);

        const char *serverEncoding = PQparameterStatus(m_conn.get(), "server_encoding");
        if (serverEncoding)
        {
            info["ServerEncoding"] = serverEncoding;
        }

        const char *clientEncoding = PQparameterStatus(m_conn.get(), "client_encoding");
        if (clientEncoding)
        {
            info["ClientEncoding"] = clientEncoding;
        }

        const char *timeZone = PQparameterStatus(m_conn.get(), "TimeZone");
        if (timeZone)
        {
            info["TimeZone"] = timeZone;
        }

        const char *intDateTimes = PQparameterStatus(m_conn.get(), "integer_datetimes");
        if (intDateTimes)
        {
            info["IntegerDatetimes"] = intDateTimes;
        }

        const char *stdConformingStrings = PQparameterStatus(m_conn.get(), "standard_conforming_strings");
        if (stdConformingStrings)
        {
            info["StandardConformingStrings"] = stdConformingStrings;
        }

        return info;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
