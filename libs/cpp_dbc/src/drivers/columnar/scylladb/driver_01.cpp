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

#if USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
    // ====================================================================
    // ScyllaDBDriver
    // ====================================================================

    // Static member initialization
    std::atomic<bool> ScyllaDBDriver::s_initialized{false}; // NOSONAR - Explicit template arg for clarity in static member definition
    std::atomic<size_t> ScyllaDBDriver::s_liveConnectionCount{0};
    std::mutex ScyllaDBDriver::s_initMutex;

    cpp_dbc::expected<bool, DBException> ScyllaDBDriver::initialize(std::nothrow_t) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::initialize - Initializing Cassandra driver");
        // Suppress server-side warnings (like SimpleStrategy recommendations)
        cass_log_set_level(CASS_LOG_ERROR);
        s_initialized.store(true, std::memory_order_release);
        SCYLLADB_DEBUG("ScyllaDBDriver::initialize - Done");
        return true;
    }

    ScyllaDBDriver::ScyllaDBDriver()
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::constructor - Creating driver");
        // Double-checked locking: compatible with -fno-exceptions (std::call_once can throw
        // std::system_error internally). Also allows re-initialization if needed.
        if (!s_initialized.load(std::memory_order_acquire))
        {
            std::scoped_lock lock(s_initMutex);
            if (!s_initialized.load(std::memory_order_relaxed))
            {
                auto initResult = initialize(std::nothrow);
                if (!initResult.has_value())
                {
                    SCYLLADB_DEBUG("ScyllaDBDriver::constructor - Initialization failed: " << initResult.error().what());
                }
            }
        }
        SCYLLADB_DEBUG("ScyllaDBDriver::constructor - Done");
    }

    void ScyllaDBDriver::cleanup()
    {
        std::scoped_lock lock(s_initMutex);
        if (!s_initialized.load(std::memory_order_acquire))
        {
            return;
        }

        auto liveCount = s_liveConnectionCount.load(std::memory_order_acquire);
        if (liveCount > 0)
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::cleanup - Skipped: "
                           << liveCount
                           << " live connection(s) still open");
            return;
        }

        s_initialized.store(false, std::memory_order_release);
    }

#ifdef __cpp_exceptions
    std::shared_ptr<ColumnarDBConnection> ScyllaDBDriver::connectColumnar(
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        auto result = connectColumnar(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

#endif // __cpp_exceptions

    std::string ScyllaDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:scylladb://<host>:<port>/<keyspace>";
    }

    bool ScyllaDBDriver::supportsClustering() const noexcept
    {
        return true;
    }

    bool ScyllaDBDriver::supportsAsync() const noexcept
    {
        return true;
    }

    std::string ScyllaDBDriver::getDriverVersion() const noexcept
    {
#if defined(CASS_VERSION_MAJOR) && defined(CASS_VERSION_MINOR) && defined(CASS_VERSION_PATCH)
        return std::to_string(CASS_VERSION_MAJOR) + "." +
               std::to_string(CASS_VERSION_MINOR) + "." +
               std::to_string(CASS_VERSION_PATCH);
#else
        return "unknown";
#endif
    }

    std::string ScyllaDBDriver::getName() const noexcept
    {
        return "scylladb";
    }

    // Nothrow API

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> ScyllaDBDriver::connectColumnar(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Connecting to " << uri);

        auto params = parseURI(std::nothrow, uri);
        if (!params.has_value())
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Failed to parse URI");
            return cpp_dbc::unexpected(params.error());
        }

        std::string host = (*params)["host"];
        int port = std::stoi((*params)["port"]);
        std::string keyspace = (*params)["database"];

        SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Creating connection object");
        auto connResult = ScyllaDBConnection::create(std::nothrow, host, port, keyspace, user, password, options);
        if (!connResult.has_value())
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Failed to create connection: " << connResult.error().what());
            return cpp_dbc::unexpected(connResult.error());
        }
        return std::shared_ptr<ColumnarDBConnection>(connResult.value());
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> ScyllaDBDriver::parseURI(std::nothrow_t, const std::string &uri) noexcept
    {
        // Use centralized URI parsing from system_utils
        // ScyllaDB URIs: cpp_dbc:scylladb://host:port/keyspace
        // Also supports IPv6: cpp_dbc:scylladb://[::1]:port/keyspace
        SCYLLADB_DEBUG("ScyllaDBDriver::parseURI - Parsing URI: " << uri);

        constexpr int DEFAULT_SCYLLADB_PORT = 9042;

        system_utils::ParsedDBURI parsed;
        if (!system_utils::parseDBURI(uri, "cpp_dbc:scylladb://", DEFAULT_SCYLLADB_PORT, parsed,
                                      false,  // allowLocalConnection
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

    cpp_dbc::expected<std::string, DBException> ScyllaDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        if (host.empty())
        {
            return cpp_dbc::unexpected(DBException("LVFLE56HHX7K",
                                                    "Cannot build ScyllaDB URI: host is required",
                                                    system_utils::captureCallStack()));
        }

        // 2026-03-08T22:00:00Z
        // Bug: Raw IPv6 hosts (e.g. "::1") were concatenated without brackets,
        // producing invalid URIs like "cpp_dbc:scylladb://::1:9042/ks".
        // Solution: Detect raw IPv6 and bracket it so the output is
        // "cpp_dbc:scylladb://[::1]:9042/ks".
        std::string effectiveHost = host;
        if (host.contains(':') &&
            !(host.front() == '[' && host.back() == ']'))
        {
            effectiveHost = "[" + host + "]";
        }

        // Append the database/keyspace only when non-empty to avoid a trailing slash
        // on URLs without a keyspace (e.g. "cpp_dbc:scylladb://host:port").
        return "cpp_dbc:scylladb://" + effectiveHost + ":" + std::to_string(port) +
               (database.empty() ? "" : "/" + database);
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
