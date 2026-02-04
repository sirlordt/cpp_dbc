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
 @brief PostgreSQL database driver implementation - PostgreSQLDBDriver class (constructor, destructor, all methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // PostgreSQLDBDriver implementation
    PostgreSQLDBDriver::PostgreSQLDBDriver() = default;

    PostgreSQLDBDriver::~PostgreSQLDBDriver()
    {
        // Sleep a bit to ensure all resources are properly released
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::shared_ptr<RelationalDBConnection> PostgreSQLDBDriver::connectRelational(const std::string &url,
                                                                                  const std::string &user,
                                                                                  const std::string &password,
                                                                                  const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, url, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBDriver::acceptsURL(const std::string &url)
    {
        return url.starts_with("cpp_dbc:postgresql://");
    }

    bool PostgreSQLDBDriver::parseURL(const std::string &url,
                                      std::string &host,
                                      int &port,
                                      std::string &database)
    {
        // Parse URL of format: cpp_dbc:postgresql://host:port/database
        if (!acceptsURL(url))
        {
            return false;
        }

        // Extract host, port, and database
        std::string temp = url.substr(21); // Remove "cpp_dbc:postgresql://"

        // Find host:port separator
        size_t hostEnd = temp.find(":");
        if (hostEnd == std::string::npos)
        {
            // Try to find database separator if no port is specified
            hostEnd = temp.find("/");
            if (hostEnd == std::string::npos)
            {
                return false;
            }

            host = temp.substr(0, hostEnd);
            port = 5432; // Default PostgreSQL port
            temp = temp.substr(hostEnd);
        }
        else
        {
            host = temp.substr(0, hostEnd);

            // Find port/database separator
            size_t portEnd = temp.find("/", hostEnd + 1);
            if (portEnd == std::string::npos)
            {
                return false;
            }

            std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
            try
            {
                port = std::stoi(portStr);
            }
            catch ([[maybe_unused]] const std::invalid_argument &ex)
            {
                PG_DEBUG("PostgreSQLDBDriver::parseURL - Invalid port number: " << ex.what());
                return false;
            }
            catch ([[maybe_unused]] const std::out_of_range &ex)
            {
                PG_DEBUG("PostgreSQLDBDriver::parseURL - Port number out of range: " << ex.what());
                return false;
            }
            catch (...) // NOSONAR - Intentional catch-all for unexpected exceptions during port parsing
            {
                PG_DEBUG("PostgreSQLDBDriver::parseURL - Unknown exception during port parsing");
                return false;
            }

            temp = temp.substr(portEnd);
        }

        // Extract database name (remove leading '/')
        database = temp.substr(1);

        return true;
    }

    // Nothrow API implementation
    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> PostgreSQLDBDriver::connectRelational(
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

            // Check if the URL is in the expected format
            if (acceptsURL(url))
            {
                // URL is in the format cpp_dbc:postgresql://host:port/database
                if (!parseURL(url, host, port, database))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("1K2L3M4N5O6P", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                }
            }
            else
            {
                // Try to extract host, port, and database directly
                size_t hostStart = url.find("://");
                if (hostStart != std::string::npos)
                {
                    std::string temp = url.substr(hostStart + 3);

                    // Find host:port separator
                    size_t hostEnd = temp.find(":");
                    if (hostEnd == std::string::npos)
                    {
                        // Try to find database separator if no port is specified
                        hostEnd = temp.find("/");
                        if (hostEnd == std::string::npos)
                        {
                            return cpp_dbc::unexpected<DBException>(DBException("7Q8R9S0T1U2V", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                        }

                        host = temp.substr(0, hostEnd);
                        port = 5432; // Default PostgreSQL port
                    }
                    else
                    {
                        host = temp.substr(0, hostEnd);

                        // Find port/database separator
                        size_t portEnd = temp.find("/", hostEnd + 1);
                        if (portEnd == std::string::npos)
                        {
                            return cpp_dbc::unexpected<DBException>(DBException("5I6J7K8L9M0N", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                        }

                        std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            return cpp_dbc::unexpected<DBException>(DBException("1O2P3Q4R5S6T", "Invalid port in URL: " + url, system_utils::captureCallStack()));
                        }

                        // Extract database
                        database = temp.substr(portEnd + 1);
                    }
                }
                else
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7U8V9W0X1Y2Z", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                }
            }

            auto connection = std::make_shared<PostgreSQLDBConnection>(host, port, database, user, password, options);
            return cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>(std::static_pointer_cast<RelationalDBConnection>(connection));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2A3B4C5D6E7F", std::string("Exception in connectRelational: ") + ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8G9H0I1J2K3L", "Unknown exception in connectRelational", system_utils::captureCallStack()));
        }
    }

    std::string PostgreSQLDBDriver::getName() const noexcept
    {
        return "postgresql";
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
