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
#include <vector>
#include <iomanip>
#include <charconv>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

#if USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
    // ====================================================================
    // ScyllaDBDriver
    // ====================================================================

    // ── Static member initialization ──────────────────────────────────────────
    // IMPORTANT: s_instance MUST be declared LAST — see mysql/driver_01.cpp for
    // the full explanation of the static destruction order requirement.
    std::atomic<bool> ScyllaDBDriver::s_initialized{false}; // NOSONAR(cpp:S1197) — explicit template arg for clarity in static member definition
    std::mutex ScyllaDBDriver::s_initMutex;
    std::mutex                      ScyllaDBDriver::s_instanceMutex;
    std::mutex                      ScyllaDBDriver::s_registryMutex;
    std::set<std::weak_ptr<ScyllaDBConnection>,
             std::owner_less<std::weak_ptr<ScyllaDBConnection>>> ScyllaDBDriver::s_connectionRegistry;
    std::atomic<bool> ScyllaDBDriver::s_cleanupPending{false};
    std::shared_ptr<ScyllaDBDriver> ScyllaDBDriver::s_instance;

    cpp_dbc::expected<bool, DBException> ScyllaDBDriver::initialize(std::nothrow_t) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::initialize - Initializing Cassandra driver");
        // Suppress server-side warnings (like SimpleStrategy recommendations)
        cass_log_set_level(CASS_LOG_ERROR);
        s_initialized.store(true, std::memory_order_release);
        SCYLLADB_DEBUG("ScyllaDBDriver::initialize - Done");
        return true;
    }

    ScyllaDBDriver::ScyllaDBDriver(ScyllaDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
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

    ScyllaDBDriver::~ScyllaDBDriver()
    {
        SCYLLADB_DEBUG("ScyllaDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup();
    }

    void ScyllaDBDriver::cleanup()
    {
        std::scoped_lock lock(s_initMutex);
        if (!s_initialized.load(std::memory_order_acquire))
        {
            return;
        }
        s_initialized.store(false, std::memory_order_release);
    }

    void ScyllaDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<ScyllaDBConnection> conn) noexcept
    {
        size_t registrySize = 0;
        {
            std::scoped_lock lock(s_registryMutex);
            s_connectionRegistry.insert(std::move(conn));
            registrySize = s_connectionRegistry.size();
        }

        // Coalesced cleanup: only post when the registry has grown past the
        // cleanup threshold and no cleanup task is already queued.
        if (registrySize > 25 && !s_cleanupPending.exchange(true, std::memory_order_acq_rel))
        {
            SerialQueue::global().post([]()
            {
                {
                    std::scoped_lock lock(s_registryMutex);
                    std::erase_if(s_connectionRegistry,
                        [](const auto &w) { return w.expired(); });
                }
                s_cleanupPending.store(false, std::memory_order_release);
            });
        }
    }

    void ScyllaDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<ScyllaDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void ScyllaDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_release);

        // Close all open connections before releasing library resources.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<ScyllaDBConnection>> connectionsToClose;
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

    size_t ScyllaDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::count_if(
            s_connectionRegistry.begin(), s_connectionRegistry.end(),
            [](const auto &w) { return !w.expired(); }));
    }

#ifdef __cpp_exceptions
    std::shared_ptr<ScyllaDBDriver> ScyllaDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

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

    cpp_dbc::expected<std::shared_ptr<ScyllaDBDriver>, DBException>
    ScyllaDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        if (s_instance)
        {
            return s_instance;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // ScyllaDBDriver constructor only calls initialize(std::nothrow) and debug
        // macros — no recoverable exception is possible.
        auto inst = std::make_shared<ScyllaDBDriver>(ScyllaDBDriver::PrivateCtorTag{}, std::nothrow);
        s_instance = inst;
        return s_instance;
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> ScyllaDBDriver::connectColumnar(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("J9PTMS7FBHR2",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

        SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Connecting to " << uri);

        auto params = parseURI(std::nothrow, uri);
        if (!params.has_value())
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Failed to parse URI");
            return cpp_dbc::unexpected(params.error());
        }

        std::string host = (*params)["host"];
        const auto &portStr = (*params)["port"];
        int port = 0;
        auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
        if (ec != std::errc{} || ptr != portStr.data() + portStr.size())
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Invalid port: " << portStr);
            return cpp_dbc::unexpected(DBException("A0YWP4OA8BTB",
                                                   "Invalid port number in URI: " + portStr,
                                                   system_utils::captureCallStack()));
        }
        std::string keyspace = (*params)["database"];

        SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Creating connection object");
        auto connResult = ScyllaDBConnection::create(std::nothrow, host, port, keyspace, user, password, options);
        if (!connResult.has_value())
        {
            SCYLLADB_DEBUG("ScyllaDBDriver::connectColumnar - Failed to create connection: " << connResult.error().what());
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        return std::shared_ptr<ColumnarDBConnection>(conn);
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
