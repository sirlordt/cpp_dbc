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

    // ── Public nothrow constructor (PrivateCtorTag) ───────────────────────────────
    // sqlite3_open_v2, sqlite3_exec (for PRAGMAs) are C APIs that never throw C++
    // exceptions. The only possible C++ throws come from std::string/std::ifstream
    // (death-sentence: std::bad_alloc) and std::make_shared<std::recursive_mutex>
    // (death-sentence). No recoverable exceptions → no try/catch needed.
    SQLiteDBConnection::SQLiteDBConnection(SQLiteDBConnection::PrivateCtorTag,
                                           std::nothrow_t,
                                           const std::string &database,
                                           const std::map<std::string, std::string> &options) noexcept
        : m_uri("cpp_dbc:sqlite://" + database)
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
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("SLGP6Q7R8S9T",
                "Failed to connect to SQLite database: " + error,
                system_utils::captureCallStack());
            return;
        }

        // Create shared_ptr with custom deleter for sqlite3*
        m_conn = makeSQLiteDbHandle(rawDb);
        m_closed.store(false, std::memory_order_seq_cst);

        SQLITE_DEBUG("Database opened successfully");

        // Initialize mutex for synchronization
        // SPECIAL CASE: :memory: databases are independent per connection
        // Each :memory: creates a SEPARATE in-memory database, so they need LOCAL mutexes
        if (database == ":memory:")
        {
            m_connPath = ":memory:";
            m_globalFileMutex = std::make_shared<std::recursive_mutex>();
            SQLITE_DEBUG("Created LOCAL mutex for :memory: database");
        }
        else
        {
            m_connPath = FileMutexRegistry::normalizePath(database);
            m_globalFileMutex = FileMutexRegistry::getInstance().getMutexForFile(m_connPath);
            SQLITE_DEBUG("FileMutexRegistry initialized for: %s", m_connPath.c_str());
        }

        // Apply configuration options via sqlite3_exec (C API, never throws)
        SQLITE_DEBUG("Applying configuration options");
        auto applyPragma = [&](const char *pragma)
        {
            char *errmsg = nullptr;
            int rc = sqlite3_exec(m_conn.get(), pragma, nullptr, nullptr, &errmsg);
            if (rc != SQLITE_OK)
            {
                SQLITE_DEBUG("PRAGMA failed (%s): %s", pragma, errmsg ? errmsg : "unknown");
                sqlite3_free(errmsg);
            }
        };

        for (const auto &option : options)
        {
            SQLITE_DEBUG("Processing option: %s=%s", option.first.c_str(), option.second.c_str());
            if (option.first == "foreign_keys" && option.second == "true")
            {
                applyPragma("PRAGMA foreign_keys = ON");
            }
            else if (option.first == "journal_mode" && option.second == "WAL")
            {
                applyPragma("PRAGMA journal_mode = WAL");
            }
            else if (option.first == "synchronous" && option.second == "FULL")
            {
                applyPragma("PRAGMA synchronous = FULL");
            }
            else if (option.first == "synchronous" && option.second == "NORMAL")
            {
                applyPragma("PRAGMA synchronous = NORMAL");
            }
            else if (option.first == "synchronous" && option.second == "OFF")
            {
                applyPragma("PRAGMA synchronous = OFF");
            }
        }

        // Enable foreign keys by default if not specified in options
        if (!options.contains("foreign_keys"))
        {
            SQLITE_DEBUG("Enabling foreign keys by default");
            applyPragma("PRAGMA foreign_keys = ON");
        }

        SQLITE_DEBUG("Connection created successfully");
    }

    // ── Private helpers ───────────────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::registerStatement(std::nothrow_t, std::weak_ptr<SQLiteDBPreparedStatement> stmt) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: closeAllStatements() used m_globalFileMutex while registerStatement()/
        // unregisterStatement() used m_statementsMutex, creating a data race (same
        // data protected by different mutexes in different contexts).
        // Solution: Changed from m_statementsMutex to m_globalFileMutex so ALL access
        // to m_activeStatements is consistently protected by the file-level lock.
        SQLITE_CONNECTION_LOCK_OR_RETURN("3N95KEM5ON6S", "Cannot register statement");
        if (m_activeStatements.size() > 50)
        {
            std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
        }
        m_activeStatements.insert(stmt);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::unregisterStatement(std::nothrow_t, std::weak_ptr<SQLiteDBPreparedStatement> stmt) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: unregisterStatement() used m_statementsMutex while closeAllStatements()
        // used m_globalFileMutex, creating a data race on m_activeStatements.
        // Solution: Changed to m_globalFileMutex for consistency with all other
        // m_activeStatements access paths.
        SQLITE_CONNECTION_LOCK_OR_RETURN("610ERYQ21GDY", "Cannot unregister statement");
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
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::closeAllStatements(std::nothrow_t) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: This method acquired both m_globalFileMutex and then m_statementsMutex
        // (nested lock), causing Helgrind LockOrder violations. Other code paths
        // (e.g., pool::returnConnection) acquire locks in a different order:
        //   pool mutex -> m_globalFileMutex -> m_statementsMutex
        // creating potential deadlock if threads acquire locks in different orders.
        // Solution: Removed m_statementsMutex entirely. m_globalFileMutex already
        // protects ALL access to the sqlite3* connection state, including
        // m_activeStatements, making the second mutex redundant.
        SQLITE_CONNECTION_LOCK_OR_RETURN("SC4B8YEQV0UV", "Cannot close statements");

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
        while ((stmt = sqlite3_next_stmt(m_conn.get(), nullptr)) != nullptr)
        {
            int result = sqlite3_finalize(stmt);
            if (result != SQLITE_OK)
            {
                SQLITE_DEBUG("1M2N3O4P5Q6R: Error finalizing leaked SQLite statement: %s",
                             sqlite3_errstr(result));
            }
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::registerResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: closeAllResultSets() used m_globalFileMutex while registerResultSet()/
        // unregisterResultSet() used m_resultSetsMutex, creating a data race (same
        // data protected by different mutexes in different contexts).
        // Solution: Changed from m_resultSetsMutex to m_globalFileMutex so ALL access
        // to m_activeResultSets is consistently protected by the file-level lock.
        SQLITE_CONNECTION_LOCK_OR_RETURN("4YYPPGK3MX58", "Cannot register result set");
        if (m_activeResultSets.size() > 50)
        {
            std::erase_if(m_activeResultSets, [](const auto &w) { return w.expired(); });
        }
        m_activeResultSets.insert(rs);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::unregisterResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: unregisterResultSet() used m_resultSetsMutex while closeAllResultSets()
        // used m_globalFileMutex, creating a data race on m_activeResultSets.
        // Solution: Changed to m_globalFileMutex for consistency with all other
        // m_activeResultSets access paths.
        SQLITE_CONNECTION_LOCK_OR_RETURN("238KFPCDSBRF", "Cannot unregister result set");
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
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::closeAllResultSets(std::nothrow_t) noexcept
    {
        // 2026-02-15T00:00:00Z
        // Bug: This method acquired both m_globalFileMutex and then m_resultSetsMutex
        // (nested lock), causing Helgrind LockOrder violations. Other code paths
        // (e.g., pool::returnConnection) acquire locks in a different order:
        //   pool mutex -> m_globalFileMutex -> m_resultSetsMutex
        // PreparedStatement destructors also call closeAllResultSets() in a different
        // lock context, creating potential deadlock scenarios.
        // Solution: Removed m_resultSetsMutex entirely. m_globalFileMutex already
        // protects ALL access to the sqlite3* connection state, including
        // m_activeResultSets, making the second mutex redundant.
        SQLITE_CONNECTION_LOCK_OR_RETURN("227RAX807NHS", "Cannot close result sets");

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
                // Use nothrow close — we are in a noexcept method
                [[maybe_unused]] auto closeResult = rs->close(std::nothrow);
            }
        }
        return {};
    }

    // ── Destructor ────────────────────────────────────────────────────────────────
    SQLiteDBConnection::~SQLiteDBConnection()
    {
        // CRITICAL: Use nothrow version - destructors must NEVER throw exceptions
        [[maybe_unused]] auto closeResult = close(std::nothrow);
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    std::shared_ptr<SQLiteDBConnection> SQLiteDBConnection::create(
        const std::string &database,
        const std::map<std::string, std::string> &options)
    {
        auto r = create(std::nothrow, database, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void SQLiteDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBPreparedStatement> SQLiteDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> SQLiteDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t SQLiteDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool SQLiteDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool SQLiteDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void SQLiteDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void SQLiteDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel SQLiteDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string SQLiteDBConnection::getURI() const
    {
        auto result = getURI(std::nothrow);
        if (!result.has_value())
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
        return result.value();
    }

    std::string SQLiteDBConnection::getServerVersion()
    {
        auto result = getServerVersion(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::map<std::string, std::string> SQLiteDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
