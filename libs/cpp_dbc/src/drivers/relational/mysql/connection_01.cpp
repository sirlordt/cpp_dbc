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
 @brief MySQL database driver implementation - MySQLDBConnection (constructor, destructor, throwing methods)

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

    // MySQLDBConnection implementation

    void MySQLDBConnection::registerStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        m_activeStatements.insert(stmt);
    }

    void MySQLDBConnection::unregisterStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt)
    {
        std::scoped_lock lock(m_statementsMutex);
        m_activeStatements.erase(stmt);
    }

    MySQLDBConnection::MySQLDBConnection(const std::string &host,
                                         int port,
                                         const std::string &database,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options)
        : m_closed(false)
    {
        // Create shared_ptr with custom deleter for MYSQL*
        m_mysql = std::shared_ptr<MYSQL>(mysql_init(nullptr), MySQLDeleter());
        if (!m_mysql)
        {
            throw DBException("N3Z4A5B6C7D8", "Failed to initialize MySQL connection", system_utils::captureCallStack());
        }

        // Force TCP/IP connection
        unsigned int protocol = MYSQL_PROTOCOL_TCP;
        mysql_options(m_mysql.get(), MYSQL_OPT_PROTOCOL, &protocol);

        // Aplicar opciones de conexión desde el mapa
        for (const auto &[key, value] : options)
        {
            if (key == "connect_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
            }
            else if (key == "read_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_READ_TIMEOUT, &timeout);
            }
            else if (key == "write_timeout")
            {
                unsigned int timeout = std::stoi(value);
                mysql_options(m_mysql.get(), MYSQL_OPT_WRITE_TIMEOUT, &timeout);
            }
            else if (key == "charset")
            {
                mysql_options(m_mysql.get(), MYSQL_SET_CHARSET_NAME, value.c_str());
            }
            else if (key == "auto_reconnect" && value == "true")
            {
                bool reconnect = true;
                mysql_options(m_mysql.get(), MYSQL_OPT_RECONNECT, &reconnect);
            }
        }

        // Connect to the database
        if (!mysql_real_connect(m_mysql.get(), host.c_str(), user.c_str(), password.c_str(),
                                nullptr, port, nullptr, 0))
        {
            std::string error = mysql_error(m_mysql.get());
            // unique_ptr will automatically call mysql_close via the deleter
            m_mysql.reset();
            throw DBException("N4Z5A6B7C8D9", "Failed to connect to MySQL: " + error, system_utils::captureCallStack());
        }

        // Select the database if provided
        if (!database.empty() && mysql_select_db(m_mysql.get(), database.c_str()) != 0)
        {
            std::string error = mysql_error(m_mysql.get());
            // unique_ptr will automatically call mysql_close via the deleter
            m_mysql.reset();
            throw DBException("N5Z6A7B8C9D0", "Failed to select database: " + error, system_utils::captureCallStack());
        }

        // Enable auto-commit by default
        setAutoCommit(true); // NOSONAR - safe: during construction, dynamic type is exactly MySQLDBConnection

        // Initialize URL string once
        std::stringstream urlBuilder;
        urlBuilder << "cpp_dbc:mysql://" << host << ":" << port;
        if (!database.empty())
        {
            urlBuilder << "/" << database;
        }
        m_url = urlBuilder.str();
    }

    MySQLDBConnection::~MySQLDBConnection()
    {
        MySQLDBConnection::close();
    }

    void MySQLDBConnection::close()
    {
        if (!m_closed && m_mysql)
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

            m_mysql.reset();
            m_closed = true;
        }
    }

    bool MySQLDBConnection::isClosed() const
    {
        return m_closed;
    }

    void MySQLDBConnection::returnToPool()
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
            // Ignore errors during cleanup
            MYSQL_DEBUG("MySQLDBConnection::returnToPool - Exception ignored during cleanup: " << ex.what());
        }
    }

    bool MySQLDBConnection::isPooled()
    {
        return false;
    }

    std::string MySQLDBConnection::getURL() const
    {
        return m_url;
    }

    std::shared_ptr<RelationalDBPreparedStatement> MySQLDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<RelationalDBResultSet> MySQLDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t MySQLDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MySQLDBConnection::setAutoCommit(bool autoCommitFlag)
    {
        auto result = setAutoCommit(std::nothrow, autoCommitFlag);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MySQLDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MySQLDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel MySQLDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
