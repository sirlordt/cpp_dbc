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
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "cpp_dbc/core/db_exception.hpp"

// hiredis headers
#include <hiredis/hiredis.h>

#include "redis_internal.hpp"

#if USE_REDIS

namespace cpp_dbc::Redis
{

    // ============================================================================
    // Static member initialization
    // ============================================================================

    std::atomic<bool> RedisDBDriver::s_initialized{false}; // NOSONAR - Must match declaration in header
    std::atomic<size_t> RedisDBDriver::s_liveConnectionCount{0};
    std::mutex RedisDBDriver::s_initMutex;

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

    RedisDBDriver::RedisDBDriver()
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

    void RedisDBDriver::cleanup()
    {
        REDIS_DEBUG("RedisDBDriver::cleanup - Cleaning up Redis driver");
        auto liveCount = s_liveConnectionCount.load(std::memory_order_acquire);
        if (liveCount > 0)
        {
            REDIS_DEBUG("RedisDBDriver::cleanup - Skipped: "
                        << liveCount
                        << " live connection(s) still open");
            return;
        }
        // No specific cleanup needed for hiredis
        s_initialized.store(false, std::memory_order_release);
        REDIS_DEBUG("RedisDBDriver::cleanup - Done");
    }

    // ============================================================================
    // RedisDBDriver - NOTHROW IMPLEMENTATIONS
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> RedisDBDriver::connectKV(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
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
            REDIS_DEBUG("RedisDBDriver::connectKV(nothrow) - Connection established");
            return std::shared_ptr<KVDBConnection>(connResult.value());
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RE1K8C9D0E1F",
                                                   std::string("connectKV failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RD7A8B9C0D1E",
                                                   "connectKV failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RD8B9C0D1E2F",
                                                   std::string("parseURI failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
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

        // Add port
        if (port > 0 && port != 6379)
        {
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
