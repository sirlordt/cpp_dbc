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
        FIREBIRD_DEBUG("FirebirdDriver::command - Starting");

        // Get the command name
        auto cmdIt = params.find("command");
        if (cmdIt == params.end())
        {
            throw DBException("J1K7L3M9N5O1", "Missing 'command' parameter", system_utils::captureCallStack());
        }

        std::string cmd;
        try
        {
            cmd = std::any_cast<std::string>(cmdIt->second);
        }
        catch (const std::bad_any_cast &)
        {
            throw DBException("K2L8M4N0O6P2", "Invalid 'command' parameter type (expected string)",
                              system_utils::captureCallStack());
        }

        FIREBIRD_DEBUG("  Command: " << cmd);

        if (cmd == "create_database")
        {
            // Extract required parameters
            std::string url, user, password;
            std::map<std::string, std::string> options;

            auto urlIt = params.find("url");
            if (urlIt == params.end())
            {
                throw DBException("L3M9N5O1P7Q3", "Missing 'url' parameter for create_database",
                                  system_utils::captureCallStack());
            }
            try
            {
                url = std::any_cast<std::string>(urlIt->second);
            }
            catch (const std::bad_any_cast &)
            {
                throw DBException("M4N0O6P2Q8R4", "Invalid 'url' parameter type", system_utils::captureCallStack());
            }

            auto userIt = params.find("user");
            if (userIt == params.end())
            {
                throw DBException("N5O1P7Q3R9S5", "Missing 'user' parameter for create_database",
                                  system_utils::captureCallStack());
            }
            try
            {
                user = std::any_cast<std::string>(userIt->second);
            }
            catch (const std::bad_any_cast &)
            {
                throw DBException("O6P2Q8R4S0T6", "Invalid 'user' parameter type", system_utils::captureCallStack());
            }

            auto passIt = params.find("password");
            if (passIt == params.end())
            {
                throw DBException("P7Q3R9S5T1U7", "Missing 'password' parameter for create_database",
                                  system_utils::captureCallStack());
            }
            try
            {
                password = std::any_cast<std::string>(passIt->second);
            }
            catch (const std::bad_any_cast &)
            {
                throw DBException("Q8R4S0T6U2V8", "Invalid 'password' parameter type", system_utils::captureCallStack());
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
                { /* ignore invalid type */
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
                { /* ignore invalid type */
                }
            }

            // Call createDatabase
            createDatabase(url, user, password, options);
            return 0;
        }
        else
        {
            throw DBException("R9S5T1U7V3W9", "Unknown command: " + cmd, system_utils::captureCallStack());
        }
    }

    bool FirebirdDBDriver::createDatabase(const std::string &url,
                                          const std::string &user,
                                          const std::string &password,
                                          const std::map<std::string, std::string> &options)
    {
        std::string host;
        int port;
        std::string database;

        if (!parseURL(url, host, port, database))
        {
            throw DBException("H9I5J1K7L3M9", "Invalid Firebird URL: " + url, system_utils::captureCallStack());
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

        FIREBIRD_DEBUG("FirebirdDriver::createDatabase - Executing: " << createDbSql);

        ISC_STATUS_ARRAY status;
        isc_db_handle db = 0;
        isc_tr_handle tr = 0;

        // Execute CREATE DATABASE using isc_dsql_execute_immediate
        if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to create database: " << errorMsg);
            throw DBException("I0J6K2L8M4N0", "Failed to create database: " + errorMsg,
                              system_utils::captureCallStack());
        }

        FIREBIRD_DEBUG("  Database created successfully!");

        // Detach from the newly created database
        if (db)
        {
            isc_detach_database(status, &db);
        }

        return true;
    }

    bool FirebirdDBDriver::parseURL(const std::string &url, std::string &host, int &port, std::string &database)
    {
        // Expected formats:
        // cpp_dbc:firebird://host:port/path/to/database.fdb
        // cpp_dbc:firebird://host/path/to/database.fdb
        // cpp_dbc:firebird:///path/to/database.fdb (local)

        std::string workUrl = url;

        // Remove prefix - only accept cpp_dbc:firebird:// prefix
        if (workUrl.find("cpp_dbc:firebird://") == 0)
        {
            workUrl = workUrl.substr(19);
        }
        else
        {
            return false;
        }

        // Default values
        host = "localhost";
        port = 3050; // Default Firebird port

        // Check for local connection (starts with /)
        if (workUrl[0] == '/')
        {
            database = workUrl;
            return true;
        }

        // Find host:port/database
        size_t slashPos = workUrl.find('/');
        if (slashPos == std::string::npos)
        {
            return false;
        }

        std::string hostPort = workUrl.substr(0, slashPos);
        database = workUrl.substr(slashPos);

        // Parse host:port
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos)
        {
            host = hostPort.substr(0, colonPos);
            try
            {
                port = std::stoi(hostPort.substr(colonPos + 1));
            }
            catch (...)
            {
                port = 3050;
            }
        }
        else
        {
            host = hostPort;
        }

        if (host.empty())
        {
            host = "localhost";
        }

        return !database.empty();
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Nothrow Methods
    // ============================================================================

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
                return cpp_dbc::unexpected(DBException("92112756B293", "Invalid Firebird URL: " + url,
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
