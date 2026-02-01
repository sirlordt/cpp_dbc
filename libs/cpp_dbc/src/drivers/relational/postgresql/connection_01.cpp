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

 @file connection_01.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBConnection (constructor, destructor, throwing methods)

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

    // PostgreSQLDBConnection implementation

    // Private methods (in order of declaration in .hpp)
    void PostgreSQLDBConnection::registerStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        m_activeStatements.insert(stmt);
    }

    void PostgreSQLDBConnection::unregisterStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        m_activeStatements.erase(stmt);
    }

    // Constructor
    PostgreSQLDBConnection::PostgreSQLDBConnection(const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password,
                                                   const std::map<std::string, std::string> &options)
        : m_closed(false)
    {
        // Build connection string
        std::stringstream conninfo;
        conninfo << "host=" << host << " ";
        conninfo << "port=" << port << " ";
        conninfo << "dbname=" << database << " ";
        conninfo << "user=" << user << " ";
        conninfo << "password=" << password << " ";

        for (const auto &[key, value] : options)
        {
            if (!key.starts_with("query__"))
            {
                conninfo << key << "=" << value << " ";
            }
        }

        if (!options.contains("gssencmode"))
        {
            conninfo << "gssencmode=disable";
        }

        // Connect to the database - create shared_ptr with custom deleter
        PGconn *rawConn = PQconnectdb(conninfo.str().c_str());
        if (PQstatus(rawConn) != CONNECTION_OK)
        {
            std::string error = PQerrorMessage(rawConn);
            PQfinish(rawConn);
            throw DBException("1Q2R3S4T5U6V", "Failed to connect to PostgreSQL: " + error, system_utils::captureCallStack());
        }

        // Wrap in shared_ptr with custom deleter
        m_conn = std::shared_ptr<PGconn>(rawConn, PGconnDeleter());

        // Set up a notice processor to suppress NOTICE messages
        PQsetNoticeProcessor(m_conn.get(), []([[maybe_unused]] void *arg, [[maybe_unused]] const char *message) // NOSONAR - void* signature required by PostgreSQL libpq API (PQnoticeProcessor typedef)
                             {
                                 // Do nothing with the notice message
                             },
                             nullptr);

        // Set auto-commit mode
        setAutoCommit(true);

        // Initialize URL string once
        std::stringstream urlBuilder;
        urlBuilder << "cpp_dbc:postgresql://" << host << ":" << port;
        if (!database.empty())
        {
            urlBuilder << "/" << database;
        }
        m_url = urlBuilder.str();
    }

    // Destructor
    PostgreSQLDBConnection::~PostgreSQLDBConnection()
    {
        PostgreSQLDBConnection::close();
    }

    // DBConnection interface
    void PostgreSQLDBConnection::close()
    {
        if (!m_closed && m_conn)
        {

            // Notify all active statements that connection is closing
            {
                std::scoped_lock lock(m_statementsMutex);
                for (auto &stmt : m_activeStatements)
                {
                    if (stmt)
                    {
                        stmt->notifyConnClosing();
                    }
                }
                m_activeStatements.clear();
            }

            // Sleep for 25ms to avoid problems with concurrency
            std::this_thread::sleep_for(std::chrono::milliseconds(25));

            // shared_ptr will automatically call PQfinish via PGconnDeleter
            m_conn.reset();
            m_closed = true;
        }
    }

    bool PostgreSQLDBConnection::isClosed() const
    {
        return m_closed;
    }

    void PostgreSQLDBConnection::returnToPool()
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

            // We don't set m_closed = true because we want to keep the connection open
            // Just mark it as available for reuse
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            PG_DEBUG("PostgreSQLDBConnection::returnToPool - Exception during cleanup: " << ex.what());
        }
        catch (...) // NOSONAR - Intentional catch-all for non-std::exception types during cleanup
        {
            PG_DEBUG("PostgreSQLDBConnection::returnToPool - Unknown exception during cleanup");
        }
    }

    bool PostgreSQLDBConnection::isPooled()
    {
        return false;
    }

    std::string PostgreSQLDBConnection::getURL() const
    {
        return m_url;
    }

    // RelationalDBConnection interface
    std::shared_ptr<RelationalDBPreparedStatement> PostgreSQLDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> PostgreSQLDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t PostgreSQLDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBConnection::setAutoCommit(bool autoCommitFlag)
    {
        auto result = setAutoCommit(std::nothrow, autoCommitFlag);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool PostgreSQLDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBConnection::beginTransaction()
    {
        auto result = this->beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBConnection::transactionActive()
    {
        auto result = this->transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBConnection::commit()
    {
        auto result = this->commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBConnection::rollback()
    {
        auto result = this->rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = this->setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel PostgreSQLDBConnection::getTransactionIsolation()
    {
        auto result = this->getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBConnection::generateStatementName()
    {
        int counter = m_statementCounter;
        m_statementCounter += 1;
        std::stringstream ss;
        ss << "stmt_" << counter;
        return ss.str();
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
