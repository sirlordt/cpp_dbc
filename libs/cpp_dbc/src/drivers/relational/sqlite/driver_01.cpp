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
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // SQLiteDBDriver implementation
    // Static member variables to ensure SQLite is configured once
    std::atomic<bool> SQLiteDBDriver::s_initialized{false};
    std::atomic<size_t> SQLiteDBDriver::s_liveConnectionCount{0};
    std::mutex SQLiteDBDriver::s_initMutex;

    SQLiteDBDriver::SQLiteDBDriver()
    {
        // Thread-safe single initialization pattern
        bool alreadyInitialized = s_initialized.load(std::memory_order_acquire);
        if (!alreadyInitialized)
        {
            // Use a mutex to ensure only one thread performs initialization
            std::lock_guard<std::mutex> lock(s_initMutex);

            // Double-check that initialization hasn't happened
            // while we were waiting for the lock
            if (!s_initialized.load(std::memory_order_relaxed))
            {
                // Configure SQLite for thread safety before initialization
                int configResult = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
                if (configResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("9E0F1G2H3I4J: Error configuring SQLite for thread safety: %s",
                                 sqlite3_errstr(configResult));
                }

                // Initialize SQLite
                int initResult = sqlite3_initialize();
                if (initResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("5K6L7M8N9O0P: Error initializing SQLite: %s",
                                 sqlite3_errstr(initResult));
                }

                // Mark as initialized
                s_initialized.store(true, std::memory_order_release);
            }
        }

        // Set up memory management (per instance)
        sqlite3_soft_heap_limit64(8 * 1024 * 1024); // 8MB soft limit
    }

    SQLiteDBDriver::~SQLiteDBDriver()
    {
        try
        {
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
        }
        catch (const std::exception &e)
        {
            SQLITE_DEBUG("7W8X9Y0Z1A2B: Exception during SQLite driver shutdown: %s", e.what());
        }
    }

    void SQLiteDBDriver::cleanup()
    {
        std::scoped_lock lock(s_initMutex);
        if (s_initialized.load(std::memory_order_acquire))
        {
            if (s_liveConnectionCount.load(std::memory_order_acquire) > 0)
            {
                SQLITE_DEBUG("SQLiteDBDriver::cleanup - Skipped: %zu live connection(s) still open",
                             s_liveConnectionCount.load(std::memory_order_acquire));
                return;
            }
            // No global library cleanup needed for SQLite (sqlite3_shutdown is optional
            // and already called in the destructor)
            s_initialized.store(false, std::memory_order_release);
        }
    }

#ifdef __cpp_exceptions
    std::shared_ptr<RelationalDBConnection> SQLiteDBDriver::connectRelational(const std::string &uri,
                                                                              [[maybe_unused]] const std::string &user,
                                                                              [[maybe_unused]] const std::string &password,
                                                                              const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, uri, user, password, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }
#endif // __cpp_exceptions


    cpp_dbc::expected<std::map<std::string, std::string>, DBException> SQLiteDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        if (!uri.starts_with("cpp_dbc:sqlite://"))
        {
            return cpp_dbc::unexpected(DBException("5RHF8WK03IZG",
                                                   "Invalid SQLite URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // SQLite has no network — host is empty, port is 0
        // Extract database path (prefix "cpp_dbc:sqlite://" is 17 chars)
        std::string database = uri.substr(17);
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
        return "cpp_dbc:sqlite://" + database;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> SQLiteDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &uri,
        [[maybe_unused]] const std::string &,
        [[maybe_unused]] const std::string &,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
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

            auto connection = std::make_shared<SQLiteDBConnection>(dbIt->second, options);
            return std::shared_ptr<RelationalDBConnection>(connection);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CR1A1B2C3D4E",
                                                   std::string("connectRelational failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("CR1A1B2C3D4F",
                                                   "connectRelational failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
