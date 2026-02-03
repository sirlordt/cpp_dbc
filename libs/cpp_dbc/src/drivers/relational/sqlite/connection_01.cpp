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
 @brief SQLite database driver implementation - SQLiteDBConnection (constructor, destructor, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include "cpp_dbc/drivers/relational/sqlite_blob.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // SQLiteDBConnection implementation - Private methods
    void SQLiteDBConnection::registerStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt)
    {
        std::lock_guard<std::mutex> lock(m_statementsMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
    }

    void SQLiteDBConnection::unregisterStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt)
    {
        std::lock_guard<std::mutex> lock(m_statementsMutex);
        // Remove expired weak_ptrs and the specified one
        for (auto it = m_activeStatements.begin(); it != m_activeStatements.end();)
        {
            auto locked = it->lock();
            auto stmtLocked = stmt.lock();
            if (!locked || (stmtLocked && locked.get() == stmtLocked.get()))
            {
                it = m_activeStatements.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // SQLiteDBConnection implementation - Throwing API

    SQLiteDBConnection::SQLiteDBConnection(const std::string &database,
                                           const std::map<std::string, std::string> &options)
        : m_db(nullptr), m_closed(false), m_autoCommit(true), m_transactionActive(false),
          m_isolationLevel(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE), // SQLite default
          m_url("cpp_dbc:sqlite://" + database)
#if DB_DRIVER_THREAD_SAFE
        , m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
    {
        try
        {
            SQLITE_DEBUG("Creating connection to: " << database);

            // Verificar si el archivo existe (para bases de datos de archivo)
            if (database != ":memory:")
            {
                std::ifstream fileCheck(database.c_str());
                if (!fileCheck)
                {
                    SQLITE_DEBUG("Database file does not exist, will be created: " << database);
                }
                else
                {
                    SQLITE_DEBUG("Database file exists: " << database);
                    fileCheck.close();
                }
            }
            else
            {
                SQLITE_DEBUG("Using in-memory database");
            }

            SQLITE_DEBUG("Calling sqlite3_open_v2");
            sqlite3 *rawDb = nullptr;
            int result = sqlite3_open_v2(database.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            if (result != SQLITE_OK)
            {
                std::string error = sqlite3_errmsg(rawDb);
                SQLITE_DEBUG("1I2J3K4L5M6N: Failed to open database: " << error);
                sqlite3_close_v2(rawDb);
                throw DBException("SLGP6Q7R8S9T", "Failed to connect to SQLite database: " + error,
                                  system_utils::captureCallStack());
            }

            // Create shared_ptr with custom deleter for sqlite3*
            m_db = makeSQLiteDbHandle(rawDb);

            SQLITE_DEBUG("Database opened successfully");

            // Aplicar opciones de configuración
            SQLITE_DEBUG("Applying configuration options");
            for (const auto &option : options)
            {
                SQLITE_DEBUG("Processing option: " << option.first << "=" << option.second);
                if (option.first == "foreign_keys" && option.second == "true")
                {
                    executeUpdate("PRAGMA foreign_keys = ON");
                }
                else if (option.first == "journal_mode" && option.second == "WAL")
                {
                    executeUpdate("PRAGMA journal_mode = WAL");
                }
                else if (option.first == "synchronous" && option.second == "FULL")
                {
                    executeUpdate("PRAGMA synchronous = FULL");
                }
                else if (option.first == "synchronous" && option.second == "NORMAL")
                {
                    executeUpdate("PRAGMA synchronous = NORMAL");
                }
                else if (option.first == "synchronous" && option.second == "OFF")
                {
                    executeUpdate("PRAGMA synchronous = OFF");
                }
            }

            // Si no se especificó foreign_keys en las opciones, habilitarlo por defecto
            if (options.find("foreign_keys") == options.end())
            {
                SQLITE_DEBUG("Enabling foreign keys by default");
                executeUpdate("PRAGMA foreign_keys = ON");
            }

            SQLITE_DEBUG("Connection created successfully");
        }
        catch (const DBException &e)
        {
            SQLITE_DEBUG("3U4V5W6X7Y8Z: DBException: " << e.what_s());
            throw;
        }
        catch (const std::exception &e)
        {
            SQLITE_DEBUG("9A0B1C2D3E4F: std::exception: " << e.what());
            throw DBException("F1262039BA12", "SQLiteConnection constructor exception: " + std::string(e.what()),
                              system_utils::captureCallStack());
        }
        catch (...)
        {
            SQLITE_DEBUG("5G6H7I8J9K0L: Unknown exception");
            throw DBException("D68199523A23", "SQLiteConnection constructor unknown exception",
                              system_utils::captureCallStack());
        }
    }

    SQLiteDBConnection::~SQLiteDBConnection()
    {
        // Make sure to close the connection and clean up resources
        try
        {
            close();
        }
        catch (...)
        {
            // Ignore exceptions during destruction
        }
    }

    void SQLiteDBConnection::close()
    {
        if (!m_closed && m_db)
        {
            try
            {
                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(m_statementsMutex);
                    for (auto &weak_stmt : m_activeStatements)
                    {
                        auto stmt = weak_stmt.lock();
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    m_activeStatements.clear();
                }

                // Explicitly finalize all prepared statements
                // This is a more aggressive approach to ensure all statements are properly cleaned up
                sqlite3_stmt *stmt;
                while ((stmt = sqlite3_next_stmt(m_db.get(), nullptr)) != nullptr)
                {
                    int result = sqlite3_finalize(stmt);
                    if (result != SQLITE_OK)
                    {
                        SQLITE_DEBUG("1M2N3O4P5Q6R: Error finalizing SQLite statement during connection close: "
                                     << sqlite3_errstr(result));
                    }
                }

                // Call sqlite3_release_memory to free up caches and unused memory
                [[maybe_unused]]
                int releasedMemory = sqlite3_release_memory(1000000); // Try to release up to 1MB of memory
                SQLITE_DEBUG("Released " << releasedMemory << " bytes of SQLite memory");

                // Smart pointer will automatically call sqlite3_close_v2 via SQLiteDbDeleter
                m_db.reset();
                m_closed = true;

                // Sleep for 10ms to avoid problems with concurrency and memory stability
                // This helps ensure all resources are properly released
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (const std::exception &e)
            {
                SQLITE_DEBUG("3Y4Z5A6B7C8D: Exception during SQLite connection close: " << e.what());
                // Asegurarse de que el db se establece a nullptr y closed a true incluso si hay una excepción
                m_db.reset();
                m_closed = true;
            }
        }
    }

    bool SQLiteDBConnection::isClosed() const
    {
        return m_closed;
    }

    void SQLiteDBConnection::returnToPool()
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

            // We don't set closed = true because we want to keep the connection open
            // Just mark it as available for reuse
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
    }

    bool SQLiteDBConnection::isPooled()
    {
        return false;
    }

    std::shared_ptr<RelationalDBPreparedStatement> SQLiteDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<RelationalDBResultSet> SQLiteDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t SQLiteDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    void SQLiteDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result)
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    bool SQLiteDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    bool SQLiteDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    void SQLiteDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result)
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel SQLiteDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string SQLiteDBConnection::getURL() const
    {
        return m_url;
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
