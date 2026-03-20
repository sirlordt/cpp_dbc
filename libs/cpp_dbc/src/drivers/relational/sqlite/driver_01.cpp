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

 @file driver_01.cpp
 @brief SQLite database driver implementation - SQLiteDBDriver class (static members, constructor, destructor, all methods)

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
#include <ranges>
#include <vector>
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // ── Static member initialization ──────────────────────────────────────────
    // IMPORTANT: s_instance MUST be declared LAST — see mysql/driver_01.cpp for
    // the full explanation of the static destruction order requirement.
    std::atomic<bool> SQLiteDBDriver::s_initialized{false}; // NOSONAR(cpp:S1197) — explicit template arg for clarity in static member definition
    std::mutex SQLiteDBDriver::s_initMutex;
    std::mutex                      SQLiteDBDriver::s_instanceMutex;
    std::mutex                      SQLiteDBDriver::s_registryMutex;
    std::set<std::weak_ptr<SQLiteDBConnection>,
             std::owner_less<std::weak_ptr<SQLiteDBConnection>>> SQLiteDBDriver::s_connectionRegistry;
    std::atomic<bool> SQLiteDBDriver::s_cleanupPending{false};
    std::shared_ptr<SQLiteDBDriver> SQLiteDBDriver::s_instance;

    // ── Private static helpers ────────────────────────────────────────────────
    cpp_dbc::expected<bool, DBException> SQLiteDBDriver::initialize(std::nothrow_t) noexcept
    {
        // Thread-safe single initialization pattern.
        // Double-checked locking is required because sqlite3_config() is NOT idempotent —
        // it must be called exactly once before sqlite3_initialize().
        // std::call_once is avoided because it can throw std::system_error internally,
        // incompatible with -fno-exceptions.
        if (!s_initialized.load(std::memory_order_seq_cst))
        {
            std::scoped_lock lock(s_initMutex);
            if (!s_initialized.load(std::memory_order_seq_cst))
            {
                // Configure SQLite for thread safety before initialization
                int configResult = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
                if (configResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("9E0F1G2H3I4J: Error configuring SQLite for thread safety: %s",
                                 sqlite3_errstr(configResult));
                    return cpp_dbc::unexpected(DBException("OAUGISBIUSJ1",
                        "Failed to configure SQLite for thread safety: " + std::string(sqlite3_errstr(configResult)),
                        system_utils::captureCallStack()));
                }

                // Initialize SQLite
                int initResult = sqlite3_initialize();
                if (initResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("5K6L7M8N9O0P: Error initializing SQLite: %s",
                                 sqlite3_errstr(initResult));
                    return cpp_dbc::unexpected(DBException("KJ4YMTV96W4O",
                        "Failed to initialize SQLite: " + std::string(sqlite3_errstr(initResult)),
                        system_utils::captureCallStack()));
                }

                // Mark as initialized
                s_initialized.store(true, std::memory_order_seq_cst);
            }
        }

        return true;
    }

    void SQLiteDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<SQLiteDBConnection> conn) noexcept
    {
        size_t registrySize = 0;
        {
            std::scoped_lock lock(s_registryMutex);
            s_connectionRegistry.insert(std::move(conn));
            registrySize = s_connectionRegistry.size();
        }

        // Coalesced cleanup: only post when the registry has grown past the
        // cleanup threshold and no cleanup task is already queued.
        if (registrySize > 25 && !s_cleanupPending.exchange(true, std::memory_order_seq_cst))
        {
            SerialQueue::global().post([]()
            {
                {
                    std::scoped_lock lock(s_registryMutex);
                    std::erase_if(s_connectionRegistry,
                                  [](const auto &w)
                                  {
                                      return w.expired();
                                  });
                }
                s_cleanupPending.store(false, std::memory_order_seq_cst);
            });
        }
    }

    void SQLiteDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<SQLiteDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void SQLiteDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_seq_cst);

        // Close all open connections before releasing library resources.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<SQLiteDBConnection>> connectionsToClose;
        {
            std::scoped_lock lock(s_registryMutex);
            for (const auto &weak : s_connectionRegistry)
            {
                auto conn = weak.lock();
                if (conn)
                {
                    connectionsToClose.push_back(std::move(conn));
                }
            }
            s_connectionRegistry.clear();
        }
        for (const auto &conn : connectionsToClose)
        {
            [[maybe_unused]] auto closeResult = conn->close(std::nothrow);
        }
    }

    void SQLiteDBDriver::cleanup(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_initMutex);
        if (!s_initialized.load(std::memory_order_seq_cst))
        {
            return;
        }

        // No try/catch: all inner calls are noexcept (C APIs sqlite3_release_memory,
        // sqlite3_shutdown, and std::this_thread::sleep_for). Only death-sentence
        // exceptions possible (none from these calls).

        // Release as much memory as possible
        [[maybe_unused]]
        int releasedMemory = sqlite3_release_memory(INT_MAX);
        SQLITE_DEBUG("Released %d bytes of SQLite memory during driver shutdown", releasedMemory);

        // Call sqlite3_shutdown to release all resources
        int shutdownResult = sqlite3_shutdown();
        if (shutdownResult != SQLITE_OK)
        {
            SQLITE_DEBUG("1Q2R3S4T5U6V: Error shutting down SQLite: %s",
                         sqlite3_errstr(shutdownResult));
        }

        // Sleep a bit to ensure all resources are properly released
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        s_initialized.store(false, std::memory_order_seq_cst);
    }

    // ── Constructor + Destructor ──────────────────────────────────────────────
    SQLiteDBDriver::SQLiteDBDriver(SQLiteDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(result.error()));
            return;
        }

        // Set up memory management (per instance)
        sqlite3_soft_heap_limit64(8 * 1024 * 1024); // 8MB soft limit
    }

    SQLiteDBDriver::~SQLiteDBDriver()
    {
        SQLITE_DEBUG("SQLiteDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup(std::nothrow);
    }

#ifdef __cpp_exceptions
    std::shared_ptr<SQLiteDBDriver> SQLiteDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBConnection> SQLiteDBDriver::connectRelational(const std::string &uri,
                                                                              const std::string &user,
                                                                              const std::string &password,
                                                                              const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<SQLiteDBDriver>, DBException>
    SQLiteDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        if (s_instance)
        {
            return s_instance;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        auto inst = std::make_shared<SQLiteDBDriver>(SQLiteDBDriver::PrivateCtorTag{}, std::nothrow);
        if (inst->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*inst->m_initError));
        }
        s_instance = inst;
        return s_instance;
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> SQLiteDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        const std::string fullPrefix =
            std::string(cpp_dbc::system_constants::URI_PREFIX) + "sqlite://";

        if (!uri.starts_with(fullPrefix))
        {
            return cpp_dbc::unexpected(DBException("5RHF8WK03IZG",
                                                   "Invalid SQLite URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // SQLite has no network — host is empty, port is 0
        std::string database = uri.substr(fullPrefix.size());
        if (database.empty())
        {
            return cpp_dbc::unexpected(DBException("5V9WQBTN67OL",
                                                   "Invalid SQLite URI: database path is empty",
                                                   system_utils::captureCallStack()));
        }

        std::map<std::string, std::string> result;
        result["host"] = "";
        result["port"] = "0";
        result["database"] = std::move(database);
        return result;
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBDriver::buildURI(
        std::nothrow_t,
        const std::string & /*host*/,
        int /*port*/,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        if (database.empty())
        {
            return cpp_dbc::unexpected(DBException("HLME1TRN0JA8",
                                                   "Cannot build SQLite URI: database path is empty",
                                                   system_utils::captureCallStack()));
        }

        // SQLite ignores host and port — URI is always cpp_dbc:sqlite://path
        return std::string(cpp_dbc::system_constants::URI_PREFIX) + "sqlite://" + database;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> SQLiteDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &uri,
        const std::string & /*user*/,
        const std::string & /*password*/,
        const std::map<std::string, std::string> &options) noexcept
    {
        if (m_closed.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("N2QJBT6FXC5P",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

        auto parseResult = parseURI(std::nothrow, uri);
        if (!parseResult.has_value())
        {
            return cpp_dbc::unexpected(parseResult.error());
        }

        const auto &parsed = parseResult.value();
        auto dbIt = parsed.find("database");
        if (dbIt == parsed.end() || dbIt->second.empty())
        {
            return cpp_dbc::unexpected(DBException("SLEN4O5P6Q7R", "Invalid SQLite connection URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // SQLiteDBConnection::create(nothrow) sets m_self internally and handles
        // any construction errors — no try/catch needed here (all calls are noexcept).
        auto connResult = SQLiteDBConnection::create(std::nothrow, dbIt->second, options);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto connection = connResult.value();
        registerConnection(std::nothrow, connection);
        return std::shared_ptr<RelationalDBConnection>(connection);
    }

    std::string SQLiteDBDriver::getName() const noexcept
    {
        return "sqlite";
    }

    std::string SQLiteDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:sqlite://<path>";
    }

    std::string SQLiteDBDriver::getDriverVersion() const noexcept
    {
        return sqlite3_libversion();
    }

    size_t SQLiteDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::ranges::count_if(
            s_connectionRegistry,
            [](const auto &w)
            {
                return !w.expired();
            }));
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
