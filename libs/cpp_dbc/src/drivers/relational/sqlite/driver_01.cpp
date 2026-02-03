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
                    SQLITE_DEBUG("9E0F1G2H3I4J: Error configuring SQLite for thread safety: "
                                 << sqlite3_errstr(configResult));
                }

                // Initialize SQLite
                int initResult = sqlite3_initialize();
                if (initResult != SQLITE_OK)
                {
                    SQLITE_DEBUG("5K6L7M8N9O0P: Error initializing SQLite: "
                                 << sqlite3_errstr(initResult));
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
            SQLITE_DEBUG("Released " << releasedMemory << " bytes of SQLite memory during driver shutdown");

            // Call sqlite3_shutdown to release all resources
            int shutdownResult = sqlite3_shutdown();
            if (shutdownResult != SQLITE_OK)
            {
                SQLITE_DEBUG("1Q2R3S4T5U6V: Error shutting down SQLite: "
                             << sqlite3_errstr(shutdownResult));
            }

            // Sleep a bit to ensure all resources are properly released
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        catch (const std::exception &e)
        {
            SQLITE_DEBUG("7W8X9Y0Z1A2B: Exception during SQLite driver shutdown: " << e.what());
        }
    }

    std::shared_ptr<RelationalDBConnection> SQLiteDBDriver::connectRelational(const std::string &url,
                                                                              [[maybe_unused]] const std::string &user,
                                                                              [[maybe_unused]] const std::string &password,
                                                                              const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, url, user, password, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    bool SQLiteDBDriver::acceptsURL(const std::string &url)
    {
        return url.starts_with("cpp_dbc:sqlite://");
    }

    bool SQLiteDBDriver::parseURL(const std::string &url, std::string &database)
    {
        // Parse URL of format: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
        if (!acceptsURL(url))
        {
            return false;
        }

        // Extract database path (prefix "cpp_dbc:sqlite://" is 17 chars)
        database = url.substr(17);
        return true;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> SQLiteDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &url,
        [[maybe_unused]] const std::string &,
        [[maybe_unused]] const std::string &,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
            std::string database;

            if (acceptsURL(url))
            {
                if (!parseURL(url, database))
                {
                    return cpp_dbc::unexpected(DBException("SLEN4O5P6Q7R", "Invalid SQLite connection URL: " + url,
                                                           system_utils::captureCallStack()));
                }
            }
            else
            {
                size_t dbStart = url.find("://");
                if (dbStart != std::string::npos)
                {
                    database = url.substr(dbStart + 3);
                }
                else
                {
                    return cpp_dbc::unexpected(DBException("SLFO5P6Q7R8S", "Invalid SQLite connection URL: " + url,
                                                           system_utils::captureCallStack()));
                }
            }

            auto connection = std::make_shared<SQLiteDBConnection>(database, options);
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
        catch (...)
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
