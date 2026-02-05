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
    std::string ScyllaDBDriver::getDriverVersion() const
    {
#if defined(CASS_VERSION_MAJOR) && defined(CASS_VERSION_MINOR) && defined(CASS_VERSION_PATCH)
        return std::to_string(CASS_VERSION_MAJOR) + "." +
               std::to_string(CASS_VERSION_MINOR) + "." +
               std::to_string(CASS_VERSION_PATCH);
#else
        return "unknown";
#endif
    }

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
            return cpp_dbc::unexpected(DBException("O6P7Q8R9S0T1", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Unknown exception");
            return cpp_dbc::unexpected(DBException("A91238912C90", "Unknown error connecting to ScyllaDB", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> ScyllaDBDriver::parseURI(std::nothrow_t, const std::string &uri) noexcept
    {
        // Use centralized URL parsing from system_utils
        // ScyllaDB URLs: cpp_dbc:scylladb://host:port/keyspace
        // Also supports IPv6: cpp_dbc:scylladb://[::1]:port/keyspace
        SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Parsing URI: " << uri);

        constexpr int DEFAULT_SCYLLADB_PORT = 9042;

        system_utils::ParsedDBURL parsed;
        if (!system_utils::parseDBURL(uri, "cpp_dbc:scylladb://", DEFAULT_SCYLLADB_PORT, parsed,
                                      false, // allowLocalConnection
                                      false)) // requireDatabase (keyspace is optional)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Invalid scheme or failed to parse");
            return cpp_dbc::unexpected(DBException("P7Q8R9S0T1U2", "Must start with cpp_dbc:scylladb://", system_utils::captureCallStack()));
        }

        std::map<std::string, std::string> result;
        result["host"] = parsed.host;
        result["port"] = std::to_string(parsed.port);
        result["database"] = parsed.database;

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
