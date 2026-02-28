/**

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
 @brief Firebird database driver implementation - FirebirdDBDriver class (static members, constructor, destructor, all methods)

 Required system package: firebird-dev (Debian/Ubuntu) or firebird-devel (RHEL/CentOS/Fedora)
 Install with: sudo apt-get install firebird-dev libfbclient2

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{
    // Static member initialization
    std::atomic<bool> FirebirdDBDriver::s_initialized{false};
    std::mutex FirebirdDBDriver::s_initMutex;

    // Global mutex to serialize Firebird statement freeing operations
    // This is needed because the Firebird client library has race conditions
    // when freeing statements from multiple threads concurrently
    // static std::mutex g_firebirdStmtFreeMutex;

    // ============================================================================
    // FirebirdDBDriver Implementation - Constructor/Destructor
    // ============================================================================

    FirebirdDBDriver::FirebirdDBDriver()
    {
        std::lock_guard<std::mutex> lock(s_initMutex);
        if (!s_initialized)
        {
            // Firebird doesn't require explicit initialization
            s_initialized = true;
        }
    }

    FirebirdDBDriver::~FirebirdDBDriver()
    {
        // Firebird doesn't require explicit cleanup
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Throwing Methods
    // ============================================================================

    #ifdef __cpp_exceptions
    std::shared_ptr<RelationalDBConnection> FirebirdDBDriver::connectRelational(const std::string &url,
                                                                                const std::string &user,
                                                                                const std::string &password,
                                                                                const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, url, user, password, options);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBDriver::acceptsURL(const std::string &url)
    {
        return url.find("cpp_dbc:firebird:") == 0;
    }

    int FirebirdDBDriver::command(const std::map<std::string, std::any> &params)
    {
        auto result = command(std::nothrow, params);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBDriver::createDatabase(const std::string &url,
                                          const std::string &user,
                                          const std::string &password,
                                          const std::map<std::string, std::string> &options)
    {
        auto result = createDatabase(std::nothrow, url, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
    #endif // __cpp_exceptions

    bool FirebirdDBDriver::parseURL(const std::string &url, std::string &host, int &port, std::string &database)
    {
        // Use centralized URL parsing from system_utils
        // Firebird URLs:
        // - cpp_dbc:firebird://host:port/path/to/database.fdb
        // - cpp_dbc:firebird://host/path/to/database.fdb
        // - cpp_dbc:firebird:///path/to/database.fdb (local)
        // - cpp_dbc:firebird://[::1]:port/path/to/database.fdb (IPv6)
        constexpr int DEFAULT_FIREBIRD_PORT = 3050;

        system_utils::ParsedDBURL parsed;
        if (!system_utils::parseDBURL(url, "cpp_dbc:firebird://", DEFAULT_FIREBIRD_PORT, parsed,
                                      true,  // allowLocalConnection
                                      true)) // requireDatabase
        {
            return false;
        }

        host = parsed.host;
        port = parsed.port;

        // For Firebird, the database path needs to include the leading slash
        if (parsed.isLocal)
        {
            // Local connection: database already has the full path with leading slash
            database = parsed.database;
        }
        else
        {
            // Remote connection: prepend slash to make it a path
            database = "/" + parsed.database;
        }

        return !database.empty();
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Nothrow Methods
    // ============================================================================

    cpp_dbc::expected<int, DBException>
    FirebirdDBDriver::command(std::nothrow_t, const std::map<std::string, std::any> &params) noexcept
    {
        FIREBIRD_DEBUG("FirebirdDriver::command(nothrow) - Starting");

        // Get the command name
        auto cmdIt = params.find("command");
        if (cmdIt == params.end())
        {
            return cpp_dbc::unexpected(DBException("J1K7L3M9N5O1", "Missing 'command' parameter", system_utils::captureCallStack()));
        }

        std::string cmd;
        try
        {
            cmd = std::any_cast<std::string>(cmdIt->second);
        }
        catch (const std::bad_any_cast &ex)
        {
            return cpp_dbc::unexpected(DBException("K2L8M4N0O6P2",
                                                   std::string("Invalid 'command' parameter type (expected string): ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CAB1PZ5CBMUV",
                                                   std::string("Exception reading 'command' parameter: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            // Intentionally silenced — unknown exception from std::any_cast
            return cpp_dbc::unexpected(DBException("OI8BZXUON0YS", "Unknown exception reading 'command' parameter",
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  Command: %s", cmd.c_str());

        if (cmd == "create_database")
        {
            // Extract required parameters
            std::string url, user, password;
            std::map<std::string, std::string> options;

            auto urlIt = params.find("url");
            if (urlIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("L3M9N5O1P7Q3", "Missing 'url' parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                url = std::any_cast<std::string>(urlIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("M4N0O6P2Q8R4",
                                                       std::string("Invalid 'url' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("PDLWNXE5A7A0",
                                                       std::string("Exception reading 'url' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("TT39UST5GNQV", "Unknown exception reading 'url' parameter",
                                                       system_utils::captureCallStack()));
            }

            auto userIt = params.find("user");
            if (userIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("N5O1P7Q3R9S5", "Missing 'user' parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                user = std::any_cast<std::string>(userIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("O6P2Q8R4S0T6",
                                                       std::string("Invalid 'user' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("9MLTND7PDO1Q",
                                                       std::string("Exception reading 'user' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("XXGUSOPM99EK", "Unknown exception reading 'user' parameter",
                                                       system_utils::captureCallStack()));
            }

            auto passIt = params.find("password");
            if (passIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("P7Q3R9S5T1U7", "Missing 'password' parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                password = std::any_cast<std::string>(passIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("Q8R4S0T6U2V8",
                                                       std::string("Invalid 'password' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("D04AODLJWCCT",
                                                       std::string("Exception reading 'password' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("7IJURPQLPQ9R", "Unknown exception reading 'password' parameter",
                                                       system_utils::captureCallStack()));
            }

            // Extract optional parameters
            auto pageSizeIt = params.find("page_size");
            if (pageSizeIt != params.end())
            {
                try
                {
                    options["page_size"] = std::any_cast<std::string>(pageSizeIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (const std::exception &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (...)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
            }

            auto charsetIt = params.find("charset");
            if (charsetIt != params.end())
            {
                try
                {
                    options["charset"] = std::any_cast<std::string>(charsetIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (const std::exception &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (...)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
            }

            auto dbResult = createDatabase(std::nothrow, url, user, password, options);
            if (!dbResult.has_value())
            {
                return cpp_dbc::unexpected(dbResult.error());
            }
            return 0;
        }
        else
        {
            return cpp_dbc::unexpected(DBException("R9S5T1U7V3W9", "Unknown command: " + cmd, system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBDriver::createDatabase(std::nothrow_t,
                                     const std::string &url,
                                     const std::string &user,
                                     const std::string &password,
                                     const std::map<std::string, std::string> &options) noexcept
    {
        std::string host;
        int port;
        std::string database;

        if (!parseURL(url, host, port, database))
        {
            return cpp_dbc::unexpected(DBException("H9I5J1K7L3M9", "Invalid Firebird URL: " + url, system_utils::captureCallStack()));
        }

        // Build the Firebird connection string for CREATE DATABASE
        std::string fbConnStr;
        if (!host.empty() && host != "localhost" && host != "127.0.0.1")
        {
            fbConnStr = host;
            if (port != 3050 && port != 0)
            {
                fbConnStr += "/" + std::to_string(port);
            }
            fbConnStr += ":";
        }
        fbConnStr += database;

        // Get page size from options
        std::string pageSize = "4096";
        auto it = options.find("page_size");
        if (it != options.end())
        {
            pageSize = it->second;
        }

        // Get charset from options
        std::string charset = "UTF8";
        it = options.find("charset");
        if (it != options.end())
        {
            charset = it->second;
        }

        // Build CREATE DATABASE SQL command
        std::string createDbSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                    "USER '" +
                                  user + "' "
                                         "PASSWORD '" +
                                  password + "' "
                                             "PAGE_SIZE " +
                                  pageSize + " "
                                             "DEFAULT CHARACTER SET " +
                                  charset;

        FIREBIRD_DEBUG("FirebirdDriver::createDatabase(nothrow) - Executing: %s", createDbSql.c_str());

        ISC_STATUS_ARRAY status;
        isc_db_handle db = 0;
        isc_tr_handle tr = 0;

        // Execute CREATE DATABASE using isc_dsql_execute_immediate
        if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to create database: %s", errorMsg.c_str());
            return cpp_dbc::unexpected(DBException("I0J6K2L8M4N0", "Failed to create database: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  Database created successfully!");

        // Detach from the newly created database
        if (db)
        {
            isc_detach_database(status, &db);
        }

        return true;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
    FirebirdDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
            std::string host;
            int port;
            std::string database;

            if (!parseURL(url, host, port, database))
            {
                return cpp_dbc::unexpected(DBException("YU88W61QFVD0", "Invalid Firebird URL: " + url,
                                                       system_utils::captureCallStack()));
            }

            auto connection = std::make_shared<FirebirdDBConnection>(host, port, database, user, password, options);
            return cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>(std::static_pointer_cast<RelationalDBConnection>(connection));
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("I1J2K3L4M5N6", std::string("Exception in connectRelational: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("J2K3L4M5N6O7", "Unknown exception in connectRelational",
                                                   system_utils::captureCallStack()));
        }
    }

    std::string FirebirdDBDriver::getName() const noexcept
    {
        return "firebird";
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
