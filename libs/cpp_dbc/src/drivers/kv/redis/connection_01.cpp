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
 * @brief Redis connection implementation - deleters, handlers, nothrow constructor, private helpers, destructor, DBConnection interface (throwing), basic KV, counter, list
 */

#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#if USE_REDIS

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <charconv>
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

    RedisReplyHandle::RedisReplyHandle(std::nothrow_t, redisReply *reply) noexcept
        : m_reply(reply)
    {
        // No null check: callers have already validated the reply pointer before constructing.
        // The nothrow variant trusts its caller and simply takes ownership of the pointer.
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
    // RedisDBConnection Implementation
    // ============================================================================

    // Nothrow constructor — all connection logic lives here.
    // On failure, sets m_initFailed/m_initError instead of throwing.
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.
    RedisDBConnection::RedisDBConnection(
        RedisDBConnection::PrivateCtorTag,
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
        : m_uri(uri)
    {
        REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Connecting to: " << uri);

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
                std::string portStr = matches[2].str();
                auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
                if (ec != std::errc{} || ptr != portStr.data() + portStr.size())
                {
                    m_initFailed = true;
                    m_initError = std::make_unique<DBException>("C58E02D9F1A8", "Invalid port in Redis URI: " + portStr,
                                              system_utils::captureCallStack());
                    return;
                }
            }

            if (matches.size() > 3 && matches[3].matched)
            {
                std::string dbStr = matches[3].str();
                auto [ptr2, ec2] = std::from_chars(dbStr.data(), dbStr.data() + dbStr.size(), m_dbIndex);
                if (ec2 != std::errc{} || ptr2 != dbStr.data() + dbStr.size())
                {
                    m_initFailed = true;
                    m_initError = std::make_unique<DBException>("C58E02D9F1A9", "Invalid database index in Redis URI: " + dbStr,
                                              system_utils::captureCallStack());
                    return;
                }
            }
        }
        else
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("C58E02D9F1A7", "Invalid Redis URI format: " + uri,
                                      system_utils::captureCallStack());
            return;
        }

        REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Connecting to host: " << host
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
                    REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Invalid connect_timeout value, using default 3000ms");
                }
            }
            catch (const std::exception &ex)
            {
                REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Failed to parse connect_timeout: " << ex.what() << ", using default 3000ms");
                timeoutMs = 3000;
            }
            catch (...)
            {
                // Intentionally catching non-std exceptions from std::stoll; fall back to default
                REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Failed to parse connect_timeout: unknown exception, using default 3000ms");
                timeoutMs = 3000;
            }
        }

        /**
         * @brief Convert milliseconds to struct timeval for Redis connection timeout
         *
         * Explicit casts are used for cross-platform compatibility and to prevent
         * compiler warnings on platforms where implicit narrowing may occur:
         *
         * @note tv_sec cast rationale:
         *   - timeoutMs is int64_t, division result is int64_t
         *   - time_t may be 32-bit on older/embedded systems
         *   - Cast documents intent and prevents narrowing warnings
         *   - Safe: practical timeout values (seconds) fit in 32-bit
         *
         * @note tv_usec cast rationale:
         *   - tv_usec type varies: suseconds_t (POSIX) vs long (Windows)
         *   - decltype ensures correct target type on any platform
         *   - Safe: value range is [0, 999000], fits in any integer type
         */
        struct timeval timeout;
        timeout.tv_sec = static_cast<time_t>(timeoutMs / 1000);                              // NOSONAR - intentional cast for portability
        timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((timeoutMs % 1000) * 1000); // NOSONAR - intentional cast for portability

        REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Using connect timeout: " << timeoutMs << "ms");
        redisContext *context = redisConnectWithTimeout(host.c_str(), port, timeout);

        if (!context || context->err)
        {
            std::string errorMsg = context ? context->errstr : "Connection error";
            if (context)
            {
                redisFree(context);
            }
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("B49D7C01E3F5", "Redis connection failed: " + errorMsg,
                                      system_utils::captureCallStack());
            return;
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
                m_initFailed = true;
                m_initError = std::make_unique<DBException>("A76F5C23D89B", "Redis authentication failed: " + errorMsg,
                                          system_utils::captureCallStack());
                return;
            }

            freeReplyObject(reply);
        }

        // Select database if specified
        if (m_dbIndex > 0)
        {
            auto selectResult = selectDatabase(std::nothrow, m_dbIndex);
            if (!selectResult.has_value())
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>(selectResult.error());
                return;
            }
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
                        REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - CLIENT SETNAME failed: " << reply->str);
                    }
                    freeReplyObject(reply);
                }
            }
        }

        REDIS_DEBUG("RedisDBConnection::constructor(nothrow) - Connected successfully");
    }

    // ============================================================================
    // RedisDBConnection - Private helper methods
    // ============================================================================

    cpp_dbc::expected<void, DBException> RedisDBConnection::validateConnection(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire) || !m_context)
        {
            return cpp_dbc::unexpected(DBException("F92C4A6E7D10", "Redis connection is closed or invalid",
                                                   system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::extractString(
        std::nothrow_t, const RedisReplyHandle &reply) const noexcept
    {
        if (!reply.get())
        {
            return std::string{};
        }

        if (reply.get()->type == REDIS_REPLY_STRING || reply.get()->type == REDIS_REPLY_STATUS)
        {
            return std::string(reply.get()->str, reply.get()->len);
        }
        else if (reply.get()->type == REDIS_REPLY_NIL)
        {
            return std::string{};
        }
        else if (reply.get()->type == REDIS_REPLY_INTEGER)
        {
            return std::to_string(reply.get()->integer);
        }

        return std::string{};
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::extractInteger(
        std::nothrow_t, const RedisReplyHandle &reply) const noexcept
    {
        if (!reply.get())
        {
            return int64_t{0};
        }

        if (reply.get()->type == REDIS_REPLY_INTEGER)
        {
            return reply.get()->integer;
        }
        else if (reply.get()->type == REDIS_REPLY_STRING)
        {
            try
            {
                return std::stoll(std::string(reply.get()->str, reply.get()->len));
            }
            catch (const std::exception &ex)
            {
                REDIS_DEBUG("RedisDBConnection::extractInteger - Failed to parse: " << ex.what());
                return int64_t{0};
            }
            catch (...)
            {
                // Intentionally empty — non-std exceptions during string parse silenced; return 0
                return int64_t{0};
            }
        }

        return int64_t{0};
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisDBConnection::extractArray(
        std::nothrow_t, const RedisReplyHandle &reply) const noexcept
    {
        std::vector<std::string> result;

        if (!reply.get() || reply.get()->type != REDIS_REPLY_ARRAY)
        {
            return result;
        }

        for (size_t i = 0; i < reply.get()->elements; ++i)
        {
            redisReply *element = reply.get()->element[i];

            // Defensive null check to prevent crashes
            if (!element)
            {
                result.emplace_back("");
                continue;
            }

            if (element->type == REDIS_REPLY_STRING || element->type == REDIS_REPLY_STATUS)
            {
                result.emplace_back(element->str, element->len);
            }
            else if (element->type == REDIS_REPLY_INTEGER)
            {
                result.emplace_back(std::to_string(element->integer));
            }
            else if (element->type == REDIS_REPLY_NIL)
            {
                result.emplace_back("");
            }
        }

        return result;
    }

    std::optional<double> RedisDBConnection::tryParseDouble(const std::string &str) noexcept
    {
        try
        {
            return std::stod(str);
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            REDIS_DEBUG("RedisDBConnection::tryParseDouble - Failed to parse: " << str << " error: " << ex.what());
            return std::nullopt;
        }
        catch (...)
        {
            REDIS_DEBUG("RedisDBConnection::tryParseDouble - Failed to parse: " << str << " unknown error");
            return std::nullopt;
        }
    }

    RedisDBConnection::~RedisDBConnection()
    {
        REDIS_DEBUG("RedisDBConnection::destructor - Destroying connection");
        // Inline close() logic to avoid virtual call in destructor (S1699)
        if (!m_closed.load(std::memory_order_acquire))
        {
            try
            {
                std::scoped_lock lock_(m_mutex);
                m_context.reset();
                m_closed.store(true, std::memory_order_release);
                // Unregister from the driver registry so getConnectionAlive() reflects
                // actual live connections.
                RedisDBDriver::unregisterConnection(std::nothrow, m_self);
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                REDIS_DEBUG("RedisDBConnection::~RedisDBConnection - Exception: " << ex.what());
            }
            catch (...)
            {
                REDIS_DEBUG("RedisDBConnection::~RedisDBConnection - Unknown exception");
            }
        }
        REDIS_DEBUG("RedisDBConnection::destructor - Done");
    }

    // DBConnection interface implementation

#ifdef __cpp_exceptions
    void RedisDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RedisDBConnection::isClosed() const
    {
        return m_closed.load(std::memory_order_acquire);
    }

    void RedisDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RedisDBConnection::isPooled() const
    {
        return false;
    }

    std::string RedisDBConnection::getURI() const
    {
        return m_uri;
    }

    void RedisDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    // KVDBConnection interface implementation - Basic key-value operations

    bool RedisDBConnection::setString(const std::string &key, const std::string &value,
                                      std::optional<int64_t> expirySeconds)
    {
        auto result = setString(std::nothrow, key, value, expirySeconds);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string RedisDBConnection::getString(const std::string &key)
    {
        auto result = getString(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::exists(const std::string &key)
    {
        auto result = exists(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::deleteKey(const std::string &key)
    {
        auto result = deleteKey(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::deleteKeys(const std::vector<std::string> &keys)
    {
        auto result = deleteKeys(std::nothrow, keys);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::expire(const std::string &key, int64_t seconds)
    {
        auto result = expire(std::nothrow, key, seconds);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::getTTL(const std::string &key)
    {
        auto result = getTTL(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Counter operations

    int64_t RedisDBConnection::increment(const std::string &key, int64_t by)
    {
        auto result = increment(std::nothrow, key, by);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::decrement(const std::string &key, int64_t by)
    {
        auto result = decrement(std::nothrow, key, by);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // List operations

    int64_t RedisDBConnection::listPushLeft(const std::string &key, const std::string &value)
    {
        auto result = listPushLeft(std::nothrow, key, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::listPushRight(const std::string &key, const std::string &value)
    {
        auto result = listPushRight(std::nothrow, key, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string RedisDBConnection::listPopLeft(const std::string &key)
    {
        auto result = listPopLeft(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string RedisDBConnection::listPopRight(const std::string &key)
    {
        auto result = listPopRight(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> RedisDBConnection::listRange(
        const std::string &key, int64_t start, int64_t stop)
    {
        auto result = listRange(std::nothrow, key, start, stop);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::listLength(const std::string &key)
    {
        auto result = listLength(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
