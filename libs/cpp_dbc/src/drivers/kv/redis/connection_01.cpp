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
 * @file connection_01.cpp
 * @brief Redis connection implementation - deleters, handlers, constructor, destructor, move ops, DBConnection interface, basic KV, counter, list throwing
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
        // RedisReplyDeleter Implementation
        // ============================================================================

        void RedisReplyDeleter::operator()(redisReply *reply)
        {
            if (reply)
            {
                freeReplyObject(reply);
            }
        }

        // RedisReplyHandle Implementation
        // ============================================================================

        RedisReplyHandle::RedisReplyHandle(redisReply *reply)
            : m_reply(reply)
        {
            if (!m_reply)
            {
                throw DBException("D72E49F1A3B0", "Failed to create Redis reply (null pointer)",
                                  system_utils::captureCallStack());
            }
        }

        RedisReplyHandle::~RedisReplyHandle() = default;

        RedisReplyHandle::RedisReplyHandle(RedisReplyHandle &&other) noexcept
            : m_reply(std::move(other.m_reply))
        {
        }

        RedisReplyHandle &RedisReplyHandle::operator=(RedisReplyHandle &&other) noexcept
        {
            if (this != &other)
            {
                m_reply = std::move(other.m_reply);
            }
            return *this;
        }

        redisReply *RedisReplyHandle::get() const
        {
            return m_reply.get();
        }

        // ============================================================================
        // RedisContextDeleter Implementation
        // ============================================================================

        void RedisContextDeleter::operator()(redisContext *context)
        {
            if (context)
            {
                redisFree(context);
            }
        }

        // ============================================================================
        // RedisConnection Implementation
        // ============================================================================

        RedisConnection::RedisConnection(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options)
            : m_url(uri)
        {
            REDIS_DEBUG("RedisConnection::constructor - Connecting to: " << uri);

            // Parse the URI - supports both IPv4 and IPv6 (IPv6 addresses must be in brackets)
            // Examples: redis://localhost:6379/0, redis://192.168.1.1:6379, redis://[::1]:6379/0, redis://[2001:db8::1]:6379
            std::regex uriRegex(R"(redis://(\[[^\]]+\]|[^:/]+)(?::([0-9]+))?(?:/([0-9]+))?)");
            std::smatch matches;

            std::string host = "localhost";
            int port = 6379;

            if (std::regex_search(uri, matches, uriRegex))
            {
                if (matches.size() > 1 && matches[1].matched)
                {
                    host = matches[1].str();
                    // Strip brackets from IPv6 addresses (e.g., "[::1]" -> "::1")
                    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
                    {
                        host = host.substr(1, host.size() - 2);
                    }
                }

                if (matches.size() > 2 && matches[2].matched)
                {
                    try
                    {
                        port = std::stoi(matches[2].str());
                    }
                    catch (const std::exception &ex)
                    {
                        throw DBException("C58E02D9F1A8", "Invalid port in Redis URI: " + matches[2].str() + " - " + ex.what(),
                                          system_utils::captureCallStack());
                    }
                }

                if (matches.size() > 3 && matches[3].matched)
                {
                    try
                    {
                        m_dbIndex = std::stoi(matches[3].str());
                    }
                    catch (const std::exception &ex)
                    {
                        throw DBException("C58E02D9F1A9", "Invalid database index in Redis URI: " + matches[3].str() + " - " + ex.what(),
                                          system_utils::captureCallStack());
                    }
                }
            }
            else
            {
                throw DBException("C58E02D9F1A7", "Invalid Redis URI format: " + uri,
                                  system_utils::captureCallStack());
            }

            REDIS_DEBUG("RedisConnection::constructor - Connecting to host: " << host
                                                                              << " port: " << port << " db: " << m_dbIndex);

            // Connect to Redis server with configurable timeout
            // Default: 3 seconds, can be overridden via "connect_timeout" option (in milliseconds)
            int64_t timeoutMs = 3000; // Default 3000ms = 3 seconds
            auto timeoutIt = options.find("connect_timeout");
            if (timeoutIt != options.end() && !timeoutIt->second.empty())
            {
                try
                {
                    timeoutMs = std::stoll(timeoutIt->second);
                    if (timeoutMs <= 0)
                    {
                        timeoutMs = 3000; // Reset to default if invalid
                        REDIS_DEBUG("RedisConnection::constructor - Invalid connect_timeout value, using default 3000ms");
                    }
                }
                catch (const std::exception &ex)
                {
                    REDIS_DEBUG("RedisConnection::constructor - Failed to parse connect_timeout: " << ex.what() << ", using default 3000ms");
                    timeoutMs = 3000;
                }
            }

            struct timeval timeout;
            timeout.tv_sec = static_cast<time_t>(timeoutMs / 1000);
            timeout.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000) * 1000);

            REDIS_DEBUG("RedisConnection::constructor - Using connect timeout: " << timeoutMs << "ms");
            redisContext *context = redisConnectWithTimeout(host.c_str(), port, timeout);

            if (!context || context->err)
            {
                std::string errorMsg = context ? context->errstr : "Connection error";
                if (context)
                {
                    redisFree(context);
                }
                throw DBException("B49D7C01E3F5", "Redis connection failed: " + errorMsg,
                                  system_utils::captureCallStack());
            }

            // Create shared_ptr with custom deleter
            m_context = std::shared_ptr<redisContext>(context, RedisContextDeleter());

            // Authenticate if credentials provided
            if (!password.empty())
            {
                redisReply *reply = nullptr;

                if (!user.empty())
                {
                    // Redis 6.0+ AUTH with username and password
                    reply = (redisReply *)redisCommand(m_context.get(), "AUTH %s %s",
                                                       user.c_str(), password.c_str());
                }
                else
                {
                    // Redis < 6.0 AUTH with password only
                    reply = (redisReply *)redisCommand(m_context.get(), "AUTH %s", password.c_str());
                }

                if (!reply || reply->type == REDIS_REPLY_ERROR)
                {
                    std::string errorMsg = reply ? reply->str : "Authentication failed";
                    if (reply)
                    {
                        freeReplyObject(reply);
                    }
                    throw DBException("A76F5C23D89B", "Redis authentication failed: " + errorMsg,
                                      system_utils::captureCallStack());
                }

                freeReplyObject(reply);
            }

            // Select database if specified
            if (m_dbIndex > 0)
            {
                selectDatabase(m_dbIndex);
            }

            // Handle connection options
            for (const auto &[key, value] : options)
            {
                if (key == "client_name" && !value.empty())
                {
                    auto *reply = static_cast<redisReply *>(redisCommand(m_context.get(),
                                                                         "CLIENT SETNAME %s",
                                                                         value.c_str()));
                    if (reply)
                    {
                        if (reply->type == REDIS_REPLY_ERROR)
                        {
                            REDIS_DEBUG("RedisConnection::constructor - CLIENT SETNAME failed: " << reply->str);
                        }
                        freeReplyObject(reply);
                    }
                }
            }

            m_closed = false;
            REDIS_DEBUG("RedisConnection::constructor - Connected successfully");
        }

        RedisConnection::~RedisConnection()
        {
            REDIS_DEBUG("RedisConnection::destructor - Destroying connection");
            // Inline close() logic to avoid virtual call in destructor (S1699)
            if (!m_closed)
            {
                try
                {
                    std::scoped_lock lock_(m_mutex);
                    m_context.reset();
                    m_closed = true;
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    REDIS_DEBUG("RedisConnection::~RedisConnection - Exception: " << ex.what());
                }
                catch (...)
                {
                    REDIS_DEBUG("RedisConnection::~RedisConnection - Unknown exception");
                }
            }
            REDIS_DEBUG("RedisConnection::destructor - Done");
        }

        RedisConnection::RedisConnection(RedisConnection &&other) noexcept
            : m_context(std::move(other.m_context)),
              m_url(std::move(other.m_url)),
              m_dbIndex(other.m_dbIndex),
              m_closed(other.m_closed.load()),
              m_pooled(other.m_pooled)
        {
            other.m_closed = true;
        }

        RedisConnection &RedisConnection::operator=(RedisConnection &&other) noexcept
        {
            if (this != &other)
            {
                if (!m_closed)
                {
                    try
                    {
                        close();
                    }
                    catch (...)
                    {
                        // Swallow exception to maintain noexcept guarantee
                        // Connection cleanup failed but we must continue with move
                        REDIS_DEBUG("RedisConnection::operator= - Exception during close(), continuing with move");
                        m_context.reset();
                        m_closed = true;
                    }
                }

                m_context = std::move(other.m_context);
                m_url = std::move(other.m_url);
                m_dbIndex = other.m_dbIndex;
                m_closed = other.m_closed.load();
                m_pooled = other.m_pooled;

                other.m_closed = true;
            }
            return *this;
        }

        // DBConnection interface implementation

        void RedisConnection::close()
        {
            REDIS_LOCK_GUARD(m_mutex);

            if (m_closed)
                return;

            REDIS_DEBUG("RedisConnection::close - Closing connection");

            // Clear the context
            m_context.reset();
            m_closed = true;

            REDIS_DEBUG("RedisConnection::close - Connection closed");
        }

        bool RedisConnection::isClosed() const
        {
            return m_closed;
        }

        void RedisConnection::returnToPool()
        {
            // If pooled, just mark as available (pool handles this)
            // If not pooled, close the connection
            if (!m_pooled)
            {
                close();
            }
        }

        bool RedisConnection::isPooled()
        {
            return m_pooled;
        }

        std::string RedisConnection::getURL() const
        {
            return m_url;
        }

        // KVDBConnection interface implementation - Basic key-value operations

        bool RedisConnection::setString(const std::string &key, const std::string &value,
                                        std::optional<int64_t> expirySeconds)
        {
            auto result = setString(std::nothrow, key, value, expirySeconds);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::getString(const std::string &key)
        {
            auto result = getString(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::exists(const std::string &key)
        {
            auto result = exists(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::deleteKey(const std::string &key)
        {
            auto result = deleteKey(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::deleteKeys(const std::vector<std::string> &keys)
        {
            auto result = deleteKeys(std::nothrow, keys);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::expire(const std::string &key, int64_t seconds)
        {
            auto result = expire(std::nothrow, key, seconds);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::getTTL(const std::string &key)
        {
            auto result = getTTL(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // Counter operations

        int64_t RedisConnection::increment(const std::string &key, int64_t by)
        {
            auto result = increment(std::nothrow, key, by);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::decrement(const std::string &key, int64_t by)
        {
            auto result = decrement(std::nothrow, key, by);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // List operations

        int64_t RedisConnection::listPushLeft(const std::string &key, const std::string &value)
        {
            auto result = listPushLeft(std::nothrow, key, value);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::listPushRight(const std::string &key, const std::string &value)
        {
            auto result = listPushRight(std::nothrow, key, value);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::listPopLeft(const std::string &key)
        {
            auto result = listPopLeft(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::listPopRight(const std::string &key)
        {
            auto result = listPopRight(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::listRange(
            const std::string &key, int64_t start, int64_t stop)
        {
            auto result = listRange(std::nothrow, key, start, stop);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::listLength(const std::string &key)
        {
            auto result = listLength(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
