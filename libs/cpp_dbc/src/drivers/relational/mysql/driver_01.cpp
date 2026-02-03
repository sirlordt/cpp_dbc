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
 @brief MySQL database driver implementation - MySQLDBDriver class (constructor, destructor, all methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

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

    // MySQLDBDriver implementation
    MySQLDBDriver::MySQLDBDriver()
    {
        // Initialize MySQL library
        mysql_library_init(0, nullptr, nullptr);
    }

    MySQLDBDriver::~MySQLDBDriver()
    {
        // Cleanup MySQL library
        mysql_library_end();
    }

    std::shared_ptr<RelationalDBConnection> MySQLDBDriver::connectRelational(const std::string &url,
                                                                             const std::string &user,
                                                                             const std::string &password,
                                                                             const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, url, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBDriver::acceptsURL(const std::string &url)
    {
        return url.starts_with("cpp_dbc:mysql://");
    }

    bool MySQLDBDriver::parseURL(const std::string &url,
                                 std::string &host,
                                 int &port,
                                 std::string &database)
    {
        // Parse URL of format: cpp_dbc:mysql://host:port/database
        if (!acceptsURL(url))
        {
            return false;
        }

        // Extract host, port, and database
        std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

        // Find host:port separator
        size_t hostEnd = temp.find(":");
        if (hostEnd == std::string::npos)
        {
            // Try to find database separator if no port is specified
            hostEnd = temp.find("/");
            if (hostEnd == std::string::npos)
            {
                // No port and no database specified
                host = temp;
                port = 3306;   // Default MySQL port
                database = ""; // No database
                return true;
            }

            host = temp.substr(0, hostEnd);
            port = 3306; // Default MySQL port
        }
        else
        {
            host = temp.substr(0, hostEnd);

            // Find port/database separator
            size_t portEnd = temp.find("/", hostEnd + 1);
            if (portEnd == std::string::npos)
            {
                // No database specified, just port
                std::string portStr = temp.substr(hostEnd + 1);
                try
                {
                    port = std::stoi(portStr);
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    MYSQL_DEBUG("MySQLDBDriver::parseURL - Invalid port: " << ex.what());
                    return false;
                }

                // No database part
                temp = "";
            }
            else
            {
                std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                try
                {
                    port = std::stoi(portStr);
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    MYSQL_DEBUG("MySQLDBDriver::parseURL - Invalid port: " << ex.what());
                    return false;
                }

                temp = temp.substr(portEnd);
            }
        }

        // Extract database name (remove leading '/')
        if (temp.length() > 0)
        {
            database = temp.substr(1);
        }
        else
        {
            // No database specified
            database = "";
        }

        return true;
    }

    // Nothrow API implementations

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> MySQLDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
            std::string host;
            int port = 0;
            std::string database = ""; // Default to empty database

            // Helper lambda to parse port string safely
            auto parsePort = [&url](const std::string &portStr) -> cpp_dbc::expected<int, DBException>
            {
                try
                {
                    return std::stoi(portStr);
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    MYSQL_DEBUG("MySQLDBDriver::connectRelational - Invalid port in URL: " << ex.what());
                    return cpp_dbc::unexpected(DBException("P6Z7A8B9C0D1", "Invalid port in URL: " + url, system_utils::captureCallStack()));
                }
            };

            // Simple parsing for common URL formats
            if (url.starts_with("cpp_dbc:mysql://"))
            {
                std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

                // Check if there's a port specified
                size_t colonPos = temp.find(":");
                if (colonPos != std::string::npos)
                {
                    // Host with port
                    host = temp.substr(0, colonPos);

                    // Find if there's a database specified
                    size_t slashPos = temp.find("/", colonPos);
                    if (slashPos != std::string::npos)
                    {
                        // Extract port
                        std::string portStr = temp.substr(colonPos + 1, slashPos - colonPos - 1);
                        auto portResult = parsePort(portStr);
                        if (!portResult.has_value())
                        {
                            return cpp_dbc::unexpected(portResult.error());
                        }
                        port = *portResult;

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }
                    }
                    else
                    {
                        // No database specified, just port
                        std::string portStr = temp.substr(colonPos + 1);
                        auto portResult = parsePort(portStr);
                        if (!portResult.has_value())
                        {
                            return cpp_dbc::unexpected(portResult.error());
                        }
                        port = *portResult;
                    }
                }
                else
                {
                    // No port specified
                    size_t slashPos = temp.find("/");
                    if (slashPos != std::string::npos)
                    {
                        // Host with database
                        host = temp.substr(0, slashPos);

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }

                        port = 3306; // Default MySQL port
                    }
                    else
                    {
                        // Just host
                        host = temp;
                        port = 3306; // Default MySQL port
                    }
                }
            }
            else
            {
                return cpp_dbc::unexpected(DBException("Y2BIGEHLS4QE", "Invalid MySQL connection URL: " + url, system_utils::captureCallStack()));
            }

            return std::shared_ptr<RelationalDBConnection>(std::make_shared<MySQLDBConnection>(host, port, database, user, password, options));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("MY2B3C4D5E6F",
                                                   std::string("connectRelational failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MY3C4D5E6F7G",
                                                   "connectRelational failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    std::string MySQLDBDriver::getName() const noexcept
    {
        return "mysql";
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
