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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file driver_01.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBDriver class (constructor, all methods)
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <array>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{
    // ====================================================================
    // ScyllaDBDriver
    // ====================================================================

    ScyllaDBDriver::ScyllaDBDriver()
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::constructor - Initializing driver");
        // Global init if needed
        // Suppress server-side warnings (like SimpleStrategy recommendations)
        cass_log_set_level(CASS_LOG_ERROR);
    }

    std::shared_ptr<ColumnarDBConnection> ScyllaDBDriver::connectColumnar(
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        auto result = connectColumnar(std::nothrow, url, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int ScyllaDBDriver::getDefaultPort() const { return 9042; }
    std::string ScyllaDBDriver::getURIScheme() const { return "scylladb"; }

    std::map<std::string, std::string> ScyllaDBDriver::parseURI(const std::string &uri)
    {
        auto result = parseURI(std::nothrow, uri);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string ScyllaDBDriver::buildURI(const std::string &host, int port, const std::string &database, const std::map<std::string, std::string> &options)
    {
        (void)options;
        return "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + database;
    }

    bool ScyllaDBDriver::supportsClustering() const { return true; }
    bool ScyllaDBDriver::supportsAsync() const { return true; }
    std::string ScyllaDBDriver::getDriverVersion() const { return "2.16.0"; } // Example version of underlying driver

    // Nothrow API

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> ScyllaDBDriver::connectColumnar(
        std::nothrow_t,
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Connecting to " << url);

        auto params = parseURI(std::nothrow, url);
        if (!params)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Failed to parse URI");
            return cpp_dbc::unexpected(params.error());
        }

        try
        {
            std::string host = (*params)["host"];
            int port = std::stoi((*params)["port"]);
            std::string keyspace = (*params)["database"];

            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Creating connection object");
            return std::shared_ptr<ColumnarDBConnection>(std::make_shared<ScyllaDBConnection>(host, port, keyspace, user, password, options));
        }
        catch (const DBException &ex)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - DBException: " << ex.what());
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Exception: " << ex.what());
            return cpp_dbc::unexpected(DBException("891238912389", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Unknown exception");
            return cpp_dbc::unexpected(DBException("A91238912C90", "Unknown error connecting to ScyllaDB", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> ScyllaDBDriver::parseURI(std::nothrow_t, const std::string &uri) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Parsing URI: " << uri);
        std::map<std::string, std::string> result;
        // cpp_dbc:scylladb://host:port/keyspace
        constexpr std::string_view scheme = "cpp_dbc:scylladb://";
        if (!uri.starts_with(scheme))
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Invalid scheme");
            return cpp_dbc::unexpected(DBException("123891238912", "Must start with cpp_dbc:scylladb://", system_utils::captureCallStack()));
        }

        std::string rest = uri.substr(scheme.length());
        size_t colon = rest.find(':');
        size_t slash = rest.find('/');

        if (slash == std::string::npos)
        {
            // No path/database
            if (colon != std::string::npos)
            {
                // host:port
                result["host"] = rest.substr(0, colon);
                result["port"] = rest.substr(colon + 1);
            }
            else
            {
                // just host
                result["host"] = rest;
                result["port"] = "9042";
            }
            result["database"] = "";
        }
        else
        {
            // Has path/database
            if (colon != std::string::npos && colon < slash)
            {
                // host:port/db
                result["host"] = rest.substr(0, colon);
                result["port"] = rest.substr(colon + 1, slash - colon - 1);
            }
            else
            {
                // host/db
                result["host"] = rest.substr(0, slash);
                result["port"] = "9042";
            }
            result["database"] = rest.substr(slash + 1);
        }
        SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Parsed host: " << result["host"] << ", port: " << result["port"] << ", database: " << result["database"]);
        return result;
    }

    bool ScyllaDBDriver::acceptsURL(const std::string &url)
    {
        return url.starts_with("cpp_dbc:scylladb://");
    }

    std::string ScyllaDBDriver::getName() const noexcept { return "scylladb"; }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
