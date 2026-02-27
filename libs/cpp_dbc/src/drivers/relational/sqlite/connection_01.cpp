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
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_statementsMutex to
        // m_globalFileMutex to ensure ALL accesses to m_activeStatements use the
        // same mutex. Previously, closeAllStatements() used m_globalFileMutex while
        // registerStatement()/unregisterStatement() used m_statementsMutex, creating
        // a data race (same data protected by different mutexes in different contexts).
        //
        // Now ALL access to m_activeStatements is consistently protected by
        // m_globalFileMutex, which is the file-level lock that protects the entire
        // SQLite connection state.
        std::lock_guard<std::recursive_mutex> lock(*m_globalFileMutex);
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
    }

    void SQLiteDBConnection::unregisterStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt)
    {
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_statementsMutex to
        // m_globalFileMutex for consistency with closeAllStatements() and
        // registerStatement(). All modifications to m_activeStatements must use
        // the same mutex to prevent data races.
        std::lock_guard<std::recursive_mutex> lock(*m_globalFileMutex);
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

    void SQLiteDBConnection::closeAllStatements()
    {
        // CRITICAL: Must hold global file mutex to prevent other threads from using
        // the sqlite3* connection while we close statements.
        //
        // LOCK ORDER FIX (2026-02-15): Previously, this method acquired both
        // m_globalFileMutex and then m_statementsMutex (nested lock). This caused
        // Helgrind LockOrder violations because other code paths (e.g., from
        // RelationalDBConnectionPool::returnConnection) acquire locks in order:
        //   pool mutex → m_globalFileMutex → m_statementsMutex
        //
        // This creates potential for deadlock if different code paths acquire locks
        // in different relative orders.
        //
        // SOLUTION: m_globalFileMutex is a file-level lock that protects ALL access
        // to the underlying sqlite3* connection. Since m_activeStatements is part of
        // the connection state and is only accessed when working with the sqlite3*
        // connection, it is already implicitly protected by m_globalFileMutex.
        // Therefore, the additional m_statementsMutex lock is redundant and can be
        // safely removed.
        //
        // SAFETY: Access to m_activeStatements is now protected solely by
        // m_globalFileMutex, which is acquired by ALL methods that interact with
        // the SQLite connection (prepareStatement, executeQuery, executeUpdate, etc.).
        // This provides sufficient synchronization without introducing lock ordering
        // issues.
        std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

        // CRITICAL: Copy weak_ptrs to temporary vector to avoid iterator invalidation.
        // When we call stmt->close(), it may call unregisterStatement() which modifies
        // m_activeStatements, invalidating iterators if we iterate directly.
        std::vector<std::weak_ptr<SQLiteDBPreparedStatement>> statementsToClose;
        statementsToClose.reserve(m_activeStatements.size());
        for (const auto &weak_stmt : m_activeStatements)
        {
            statementsToClose.push_back(weak_stmt);
        }
        m_activeStatements.clear();

        // Now close all statements without holding the registry lock
        for (auto &weak_stmt : statementsToClose)
        {
            auto stmt = weak_stmt.lock();
            if (stmt)
            {
                // Close the statement (sqlite3_finalize) while we have exclusive access
                auto result = stmt->close(std::nothrow);
                if (!result.has_value())
                {
                    // Log the error but don't throw - connection is already closing
                    SQLITE_DEBUG("Failed to close prepared statement: %s", result.error().what_s().data());
                }
            }
        }

        // Safety net: Explicitly finalize any remaining SQLite statements
        // This catches leaked statements or those not tracked in m_activeStatements
        // NOTE: We already hold m_globalFileMutex from the beginning of this function
        sqlite3_stmt *stmt;
        while ((stmt = sqlite3_next_stmt(m_db.get(), nullptr)) != nullptr)
        {
            int result = sqlite3_finalize(stmt);
            if (result != SQLITE_OK)
            {
                SQLITE_DEBUG("1M2N3O4P5Q6R: Error finalizing leaked SQLite statement: %s",
                             sqlite3_errstr(result));
            }
        }
    }

    void SQLiteDBConnection::registerResultSet(std::weak_ptr<SQLiteDBResultSet> rs)
    {
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_resultSetsMutex to
        // m_globalFileMutex to ensure ALL accesses to m_activeResultSets use the
        // same mutex. Previously, closeAllResultSets() used m_globalFileMutex while
        // registerResultSet()/unregisterResultSet() used m_resultSetsMutex, creating
        // a data race (same data protected by different mutexes in different contexts).
        //
        // Now ALL access to m_activeResultSets is consistently protected by
        // m_globalFileMutex, which is the file-level lock that protects the entire
        // SQLite connection state.
        std::lock_guard<std::recursive_mutex> lock(*m_globalFileMutex);
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w) { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
    }

    void SQLiteDBConnection::unregisterResultSet(std::weak_ptr<SQLiteDBResultSet> rs)
    {
        // LOCK CONSISTENCY FIX (2026-02-15): Changed from m_resultSetsMutex to
        // m_globalFileMutex for consistency with closeAllResultSets() and
        // registerResultSet(). All modifications to m_activeResultSets must use
        // the same mutex to prevent data races.
        std::lock_guard<std::recursive_mutex> lock(*m_globalFileMutex);
        // Remove expired weak_ptrs and the specified one
        for (auto it = m_activeResultSets.begin(); it != m_activeResultSets.end();)
        {
            auto locked = it->lock();
            auto rsLocked = rs.lock();
            if (!locked || (rsLocked && locked.get() == rsLocked.get()))
            {
                it = m_activeResultSets.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SQLiteDBConnection::closeAllResultSets()
    {
        // CRITICAL: Must hold global file mutex to prevent other threads from using
        // the sqlite3* connection while we close result sets.
        //
        // LOCK ORDER FIX (2026-02-15): Previously, this method acquired both
        // m_globalFileMutex and then m_resultSetsMutex (nested lock). This caused
        // Helgrind LockOrder violations because other code paths (e.g., from
        // RelationalDBConnectionPool::returnConnection) acquire locks in order:
        //   pool mutex → m_globalFileMutex → m_resultSetsMutex
        //
        // But when PreparedStatement destructors call closeAllResultSets(), they
        // acquire locks in a different context, creating potential deadlock scenarios.
        //
        // SOLUTION: m_globalFileMutex is a file-level lock that protects ALL access
        // to the underlying sqlite3* connection. Since m_activeResultSets is part of
        // the connection state and is only accessed when working with the sqlite3*
        // connection, it is already implicitly protected by m_globalFileMutex.
        // Therefore, the additional m_resultSetsMutex lock is redundant and can be
        // safely removed.
        //
        // SAFETY: Access to m_activeResultSets is now protected solely by
        // m_globalFileMutex, which is acquired by ALL methods that interact with
        // the SQLite connection (executeQuery, executeUpdate, prepareStatement, etc.).
        // This provides sufficient synchronization without introducing lock ordering
        // issues.
        //
        // Reference: Helgrind error logs in logs/test/2026-02-15-18-46-52/22_RUN02_fail.log
        std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

        // CRITICAL: Copy weak_ptrs to temporary vector to avoid iterator invalidation.
        // When we call rs->close(), it calls unregisterResultSet() which modifies
        // m_activeResultSets, invalidating iterators if we iterate directly.
        std::vector<std::weak_ptr<SQLiteDBResultSet>> resultSetsToClose;
        resultSetsToClose.reserve(m_activeResultSets.size());
        for (const auto &weak_rs : m_activeResultSets)
        {
            resultSetsToClose.push_back(weak_rs);
        }
        m_activeResultSets.clear();

        // Now close all result sets without holding the registry lock
        for (auto &weak_rs : resultSetsToClose)
        {
            auto rs = weak_rs.lock();
            if (rs)
            {
                try
                {
                    rs->close();
                }
                catch (...)
                {
                    // Ignore errors during shutdown
                }
            }
        }
    }

    cpp_dbc::expected<void, cpp_dbc::DBException> SQLiteDBConnection::reset(std::nothrow_t) noexcept
    {
        try
        {
            // CRITICAL FIX (2026-02-15): Force SQLite to cancel any pending operations
            // and clear internal state BEFORE attempting cleanup. This prevents
            // "readonly database" errors when executeQuery() fails and leaves the
            // connection in an inconsistent state (e.g., WAL locks, partial transactions).
            //
            // Without sqlite3_interrupt(), the following scenario occurs:
            // 1. executeQuery() fails with invalid column → DB enters readonly state
            // 2. reset() calls rollback() → executeUpdate("ROLLBACK")
            // 3. executeUpdate() fails → "attempt to write a readonly database" (R4Z5A6B7C8D9)
            // 4. Connection permanently corrupted, cannot perform cleanup
            //
            // sqlite3_interrupt() forcibly cancels operations and clears locks,
            // allowing subsequent operations to succeed.
            if (m_db)
            {
                sqlite3_interrupt(m_db.get());
            }

            // NOTE: We don't acquire locks here because each method we call
            // already acquires m_globalFileMutex internally:
            // - closeAllResultSets() acquires m_globalFileMutex (line 141)
            // - closeAllStatements() acquires m_globalFileMutex (line 75)
            // - rollback() acquires m_globalFileMutex (line 372)
            // - setAutoCommit() acquires m_globalFileMutex (line 175)
            // - transactionActive() reads atomic m_transactionActive (thread-safe)
            //
            // Acquiring the lock here would cause deadlock when those methods
            // try to acquire it again.

            // Close all result sets first, then statements
            closeAllResultSets();
            closeAllStatements();

            // Rollback any active transaction
            auto txActive = transactionActive(std::nothrow);
            if (txActive.has_value() && txActive.value())
            {
                rollback(std::nothrow);
            }

            // Reset auto-commit to true
            setAutoCommit(std::nothrow, true);

            return {};
        }
        catch (const cpp_dbc::DBException &e)
        {
            SQLITE_DEBUG("DBException during connection reset: %s", e.what());
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            SQLITE_DEBUG("Exception during connection reset: %s", e.what());
            return cpp_dbc::unexpected(cpp_dbc::DBException("EZ8HHCVD9T2D",
                                                            std::string("Reset failed: ") + e.what(),
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
        catch (...)
        {
            SQLITE_DEBUG("Unknown exception during connection reset");
            return cpp_dbc::unexpected(cpp_dbc::DBException("GZ8XD1JMH5QP",
                                                            "Reset failed with unknown error",
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
    }

    // SQLiteDBConnection implementation - Throwing API

    SQLiteDBConnection::SQLiteDBConnection(const std::string &database,
                                           const std::map<std::string, std::string> &options)
        : m_db(nullptr), m_closed(false), m_autoCommit(true), m_transactionActive(false),
          m_isolationLevel(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE), // SQLite default
          m_url("cpp_dbc:sqlite://" + database)
    {
        try
        {
            SQLITE_DEBUG("Creating connection to: %s", database.c_str());

            // Verificar si el archivo existe (para bases de datos de archivo)
            if (database != ":memory:")
            {
                std::ifstream fileCheck(database.c_str());
                if (!fileCheck)
                {
                    SQLITE_DEBUG("Database file does not exist, will be created: %s", database.c_str());
                }
                else
                {
                    SQLITE_DEBUG("Database file exists: %s", database.c_str());
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
                SQLITE_DEBUG("1I2J3K4L5M6N: Failed to open database: %s", error.c_str());
                sqlite3_close_v2(rawDb);
                throw DBException("SLGP6Q7R8S9T", "Failed to connect to SQLite database: " + error,
                                  system_utils::captureCallStack());
            }

            // Create shared_ptr with custom deleter for sqlite3*
            m_db = makeSQLiteDbHandle(rawDb);

            SQLITE_DEBUG("Database opened successfully");

            // Initialize mutex for synchronization
            // SPECIAL CASE: :memory: databases are independent per connection
            // Each :memory: creates a SEPARATE in-memory database, so they need LOCAL mutexes
            if (database == ":memory:")
            {
                // Create a LOCAL mutex for this :memory: connection
                // :memory: databases don't share data, so they don't need a global mutex
                m_dbPath = ":memory:";
                m_globalFileMutex = std::make_shared<std::recursive_mutex>();
                SQLITE_DEBUG("Created LOCAL mutex for :memory: database");
            }
            else
            {
                // For file-based databases, use FileMutexRegistry for global file-level synchronization
                m_dbPath = FileMutexRegistry::normalizePath(database);
                m_globalFileMutex = FileMutexRegistry::getInstance().getMutexForFile(m_dbPath);
                SQLITE_DEBUG("FileMutexRegistry initialized for: %s", m_dbPath.c_str());
            }

            // Aplicar opciones de configuración
            SQLITE_DEBUG("Applying configuration options");
            for (const auto &option : options)
            {
                SQLITE_DEBUG("Processing option: %s=%s", option.first.c_str(), option.second.c_str());
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
            SQLITE_DEBUG("3U4V5W6X7Y8Z: DBException: %s", e.what_s().data());
            throw;
        }
        catch (const std::exception &e)
        {
            SQLITE_DEBUG("9A0B1C2D3E4F: std::exception: %s", e.what());
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
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    void SQLiteDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::prepareForBorrow()
    {
        auto result = prepareForBorrow(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
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
        auto result = getURL(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::ping(std::nothrow_t) noexcept
    {
        auto result = executeQuery(std::nothrow, "SELECT 1");
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        auto closeResult = result.value()->close(std::nothrow);
        if (!closeResult.has_value())
        {
            return cpp_dbc::unexpected(closeResult.error());
        }
        return true;
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
