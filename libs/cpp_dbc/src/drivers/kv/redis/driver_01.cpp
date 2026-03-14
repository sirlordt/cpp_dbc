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
 * @brief Redis driver implementation - throwing and NOTHROW methods
 */

#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <vector>
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "cpp_dbc/core/db_exception.hpp"

// hiredis headers
#include <hiredis/hiredis.h>

#include "redis_internal.hpp"

#if USE_REDIS

namespace cpp_dbc::Redis
{

    // ── Static member initialization ──────────────────────────────────────────
    std::atomic<bool> RedisDBDriver::s_initialized{false}; // NOSONAR(cpp:S1197) — explicit template arg for clarity in static member definition
    std::mutex RedisDBDriver::s_initMutex;
    std::weak_ptr<RedisDBDriver> RedisDBDriver::s_instance;
    std::mutex                   RedisDBDriver::s_instanceMutex;
    std::mutex                   RedisDBDriver::s_registryMutex;
    std::set<std::weak_ptr<RedisDBConnection>,
             std::owner_less<std::weak_ptr<RedisDBConnection>>> RedisDBDriver::s_connectionRegistry;

    // ============================================================================
    // RedisDBDriver Implementation
    // ============================================================================

    cpp_dbc::expected<bool, DBException> RedisDBDriver::initialize(std::nothrow_t) noexcept
    {
        REDIS_DEBUG("RedisDBDriver::initialize - Initializing Redis driver");
        // No specific initialization needed for hiredis
        s_initialized.store(true, std::memory_order_release);
        REDIS_DEBUG("RedisDBDriver::initialize - Done");
        return true;
    }

    RedisDBDriver::RedisDBDriver(RedisDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        REDIS_DEBUG("RedisDBDriver::constructor - Creating driver");
        // Use double-checked locking pattern for thread-safe initialization
        // that can be reset by cleanup()
        if (!s_initialized.load(std::memory_order_acquire))
        {
            std::scoped_lock lock(s_initMutex);
            if (!s_initialized.load(std::memory_order_relaxed))
            {
                auto initResult = initialize(std::nothrow);
                if (!initResult.has_value())
                {
                    REDIS_DEBUG("RedisDBDriver::constructor - Initialization failed: " << initResult.error().what());
                }
            }
        }
        REDIS_DEBUG("RedisDBDriver::constructor - Done");
    }

    RedisDBDriver::~RedisDBDriver()
    {
        REDIS_DEBUG("RedisDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup();
    }

#ifdef __cpp_exceptions
    std::shared_ptr<KVDBConnection> RedisDBDriver::connectKV(
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        auto result = connectKV(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

#endif // __cpp_exceptions


    std::string RedisDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:redis://<host>:<port>/<db>";
    }

    bool RedisDBDriver::supportsClustering() const noexcept
    {
        return true;
    }

    bool RedisDBDriver::supportsReplication() const noexcept
    {
        return true;
    }

    std::string RedisDBDriver::getDriverVersion() const noexcept
    {
        // Build version string from hiredis version macros
#if defined(HIREDIS_MAJOR) && defined(HIREDIS_MINOR) && defined(HIREDIS_PATCH)
        return std::to_string(HIREDIS_MAJOR) + "." +
               std::to_string(HIREDIS_MINOR) + "." +
               std::to_string(HIREDIS_PATCH);
#else
        // Fallback if individual version macros are not available
        return "unknown";
#endif
    }

    void RedisDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<RedisDBConnection> conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.insert(std::move(conn));
    }

    void RedisDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<RedisDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void RedisDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_release);

        // Close all open connections before releasing library resources.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<RedisDBConnection>> connectionsToClose;
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

    size_t RedisDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::ranges::count_if(
            s_connectionRegistry,
            [](const auto &w) { return !w.expired(); }));
    }

#ifdef __cpp_exceptions
    std::shared_ptr<RedisDBDriver> RedisDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    void RedisDBDriver::cleanup()
    {
        REDIS_DEBUG("RedisDBDriver::cleanup - Cleaning up Redis driver");
        std::scoped_lock lock(s_initMutex);
        if (!s_initialized.load(std::memory_order_acquire))
        {
            return;
        }
        // No specific cleanup needed for hiredis
        s_initialized.store(false, std::memory_order_release);
        REDIS_DEBUG("RedisDBDriver::cleanup - Done");
    }

    // ============================================================================
    // RedisDBDriver - NOTHROW IMPLEMENTATIONS
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<RedisDBDriver>, DBException>
    RedisDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        auto existing = s_instance.lock();
        if (existing)
        {
            return existing;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // RedisDBDriver() constructor only calls initialize(std::nothrow) and debug
        // macros — no recoverable exception is possible.
        auto inst = std::make_shared<RedisDBDriver>(RedisDBDriver::PrivateCtorTag{}, std::nothrow);
        s_instance = inst;
        return inst;
    }

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> RedisDBDriver::connectKV(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("V6RCMN8YQX4T",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

        REDIS_DEBUG("RedisDBDriver::connectKV(nothrow) - Connecting to: " << uri);

        auto uriCheck = acceptURI(std::nothrow, uri);
        if (!uriCheck.has_value())
        {
            return cpp_dbc::unexpected(uriCheck.error());
        }

        std::string redisUri = uri;
        if (uri.starts_with(cpp_dbc::system_constants::URI_PREFIX))
        {
            redisUri = uri.substr(cpp_dbc::system_constants::URI_PREFIX.size());
        }

        auto connResult = RedisDBConnection::create(std::nothrow, redisUri, user, password, options);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        REDIS_DEBUG("RedisDBDriver::connectKV(nothrow) - Connection established");
        return std::shared_ptr<KVDBConnection>(conn);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        try
        {
            std::map<std::string, std::string> result;
            // Support both regular hosts and bracketed IPv6 addresses (e.g., cpp_dbc:redis://[::1]:6379)
            std::regex uriRegex(R"(^cpp_dbc:redis://(\[[^\]]+\]|[^:/]+)(?::([0-9]+))?(?:/([0-9]+))?$)");
            std::smatch matches;

            if (std::regex_match(uri, matches, uriRegex))
            {
                if (matches.size() > 1 && matches[1].matched)
                {
                    std::string host = matches[1].str();
                    // Strip surrounding brackets from IPv6 addresses
                    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
                    {
                        host = host.substr(1, host.size() - 2);
                    }
                    result["host"] = host;
                }
                else
                {
                    result["host"] = "localhost";
                }

                if (matches.size() > 2 && matches[2].matched)
                {
                    result["port"] = matches[2].str();
                }
                else
                {
                    result["port"] = "6379";
                }

                if (matches.size() > 3 && matches[3].matched)
                {
                    result["db"] = matches[3].str();
                }
                else
                {
                    result["db"] = "0";
                }
            }
            else
            {
                return cpp_dbc::unexpected(DBException("F0E1D2C3B4A5", "Invalid Redis URI format: " + uri,
                                                       system_utils::captureCallStack()));
            }

            return result;
        }
        catch (const std::exception &ex)
        {
            // std::regex constructor and std::regex_match can throw std::regex_error
            // (inherits from std::exception) on malformed patterns or excessive backtracking.
            return cpp_dbc::unexpected(DBException("RD8B9C0D1E2F",
                                                   std::string("parseURI failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("RD9C0D1E2F3A",
                                                   "parseURI failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> RedisDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        std::ostringstream uri;

        // Start with scheme (use cpp_dbc: prefix for consistency with acceptURI/connectKV)
        uri << "cpp_dbc:redis://";

        // Add host — bracket raw IPv6 (e.g. "::1" → "[::1]")
        if (host.empty())
        {
            uri << "localhost";
        }
        else if (host.contains(':') &&
                 !(host.front() == '[' && host.back() == ']'))
        {
            uri << "[" << host << "]";
        }
        else
        {
            uri << host;
        }

        // Validate and add port
        if (port > 0 && port != 6379)
        {
            if (port < system_constants::PORT_MIN || port > system_constants::PORT_MAX)
            {
                return cpp_dbc::unexpected(DBException("P7FXQ9Y75G60",
                    "Cannot build Redis URI: port " + std::to_string(port) + " is outside valid range ("
                    + std::to_string(system_constants::PORT_MIN) + "-" + std::to_string(system_constants::PORT_MAX) + ")",
                    system_utils::captureCallStack()));
            }
            uri << ":" << port;
        }

        // Add database index — must be numeric to match parseURI's ([0-9]+) rule
        if (!database.empty() && database != "0")
        {
            if (!std::ranges::all_of(database, [](char c) { return c >= '0' && c <= '9'; }))
            {
                return cpp_dbc::unexpected(DBException("W4HP038OBTED",
                    "Cannot build Redis URI: database must be a numeric index, got: " + database,
                    system_utils::captureCallStack()));
            }
            uri << "/" << database;
        }

        // Redis URI doesn't support options in the URI
        // They are handled separately in the connection method

        return uri.str();
    }

    std::string RedisDBDriver::getName() const noexcept
    {
        return "redis";
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
