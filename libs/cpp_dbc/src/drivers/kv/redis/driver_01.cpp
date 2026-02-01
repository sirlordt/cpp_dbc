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

#if USE_REDIS

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include "cpp_dbc/common/system_utils.hpp"
#include "cpp_dbc/core/db_exception.hpp"

// hiredis headers
#include <hiredis/hiredis.h>

#include "redis_internal.hpp"

namespace cpp_dbc::Redis
{

        // ============================================================================
        // Static member initialization
        // ============================================================================

        std::once_flag RedisDriver::s_initFlag;
        std::atomic<bool> RedisDriver::s_initialized{false}; // NOSONAR - Must match declaration in header

        // ============================================================================
        // RedisDriver Implementation
        // ============================================================================

        RedisDriver::RedisDriver()
        {
            REDIS_DEBUG("RedisDriver::constructor - Creating driver");
            std::call_once(s_initFlag, initialize);
            REDIS_DEBUG("RedisDriver::constructor - Done");
        }

        RedisDriver::~RedisDriver()
        {
            REDIS_DEBUG("RedisDriver::destructor - Destroying driver");
        }

        bool RedisDriver::acceptsURL(const std::string &url)
        {
            return url.starts_with("cpp_dbc:redis://");
        }

        std::shared_ptr<KVDBConnection> RedisDriver::connectKV(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options)
        {
            auto result = connectKV(std::nothrow, url, user, password, options);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int RedisDriver::getDefaultPort() const
        {
            return 6379;
        }

        std::string RedisDriver::getURIScheme() const
        {
            return "redis";
        }

        std::map<std::string, std::string> RedisDriver::parseURI(const std::string &uri)
        {
            auto result = parseURI(std::nothrow, uri);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisDriver::buildURI(
            const std::string &host,
            int port,
            const std::string &db,
            [[maybe_unused]] const std::map<std::string, std::string> &options)
        {
            std::ostringstream uri;

            // Start with scheme (use cpp_dbc: prefix for consistency with acceptsURL/connectKV)
            uri << "cpp_dbc:redis://";

            // Add host
            uri << (host.empty() ? "localhost" : host);

            // Add port
            if (port > 0 && port != 6379)
            {
                uri << ":" << port;
            }

            // Add database index
            if (!db.empty() && db != "0")
            {
                uri << "/" << db;
            }

            // Redis URI doesn't support options in the URI
            // They are handled separately in the connection method

            return uri.str();
        }

        bool RedisDriver::supportsClustering() const
        {
            return true;
        }

        bool RedisDriver::supportsReplication() const
        {
            return true;
        }

        std::string RedisDriver::getDriverVersion() const
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

        void RedisDriver::cleanup()
        {
            REDIS_DEBUG("RedisDriver::cleanup - Cleaning up Redis driver");
            // No specific cleanup needed for hiredis
            s_initialized = false;
            REDIS_DEBUG("RedisDriver::cleanup - Done");
        }

        // ============================================================================
        // RedisDriver - NOTHROW IMPLEMENTATIONS
        // ============================================================================

        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> RedisDriver::connectKV(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options) noexcept
        {
            try
            {
                REDIS_DEBUG("RedisDriver::connectKV(nothrow) - Connecting to: " << url);
                REDIS_LOCK_GUARD(m_mutex);

                if (!acceptsURL(url))
                {
                    return cpp_dbc::unexpected(DBException("A93B8C7D2E1F", "Invalid Redis URL: " + url,
                                                           system_utils::captureCallStack()));
                }

                std::string redisUrl = url;
                if (url.starts_with("cpp_dbc:"))
                {
                    redisUrl = url.substr(8);
                }

                auto conn = std::make_shared<RedisConnection>(redisUrl, user, password, options);
                REDIS_DEBUG("RedisDriver::connectKV(nothrow) - Connection established");
                return std::shared_ptr<KVDBConnection>(conn);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
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

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisDriver::parseURI(
            std::nothrow_t, const std::string &uri) noexcept
        {
            try
            {
                std::map<std::string, std::string> result;
                std::regex uriRegex("redis://([^:/]+)(?::([0-9]+))?(?:/([0-9]+))?");
                std::smatch matches;

                if (std::regex_search(uri, matches, uriRegex))
                {
                    if (matches.size() > 1 && matches[1].matched)
                    {
                        result["host"] = matches[1].str();
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

        std::string RedisDriver::getName() const noexcept
        {
            return "redis";
        }

        // ============================================================================
        // RedisDriver - Private methods
        // ============================================================================

        void RedisDriver::initialize()
        {
            REDIS_DEBUG("RedisDriver::initialize - Initializing Redis driver");
            // No specific initialization needed for hiredis
            s_initialized = true;
            REDIS_DEBUG("RedisDriver::initialize - Done");
        }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
