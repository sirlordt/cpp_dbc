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
 * @file connection_04.cpp
 * @brief Redis connection implementation - NOTHROW basic, counter, list operations
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
    // RedisDBConnection - NOTHROW IMPLEMENTATIONS (Real logic)
    // ============================================================================

    cpp_dbc::expected<bool, DBException> RedisDBConnection::setString(
        std::nothrow_t,
        const std::string &key,
        const std::string &value,
        std::optional<int64_t> expirySeconds) noexcept
    {
        std::vector<std::string> args = {key, value};

        if (expirySeconds.has_value())
        {
            args.emplace_back("EX");
            args.emplace_back(std::to_string(*expirySeconds));
        }

        auto replyResult = executeRaw(std::nothrow, "SET", args);
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        const auto &reply = replyResult.value();
        bool result = reply.get()->type == REDIS_REPLY_STATUS &&
                      std::string(reply.get()->str, reply.get()->len) == "OK";
        return result;
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::getString(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "GET", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractString(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::exists(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "EXISTS", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::deleteKey(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "DEL", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::deleteKeys(
        std::nothrow_t, const std::vector<std::string> &keys) noexcept
    {
        if (keys.empty())
        {
            return int64_t{0};
        }

        auto replyResult = executeRaw(std::nothrow, "DEL", keys);
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::expire(
        std::nothrow_t, const std::string &key, int64_t seconds) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "EXPIRE", {key, std::to_string(seconds)});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::getTTL(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "TTL", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::increment(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        if (by == 1)
        {
            auto replyResult = executeRaw(std::nothrow, "INCR", {key});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            return extractInteger(std::nothrow, replyResult.value()).value();
        }
        else
        {
            auto replyResult = executeRaw(std::nothrow, "INCRBY", {key, std::to_string(by)});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            return extractInteger(std::nothrow, replyResult.value()).value();
        }
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::decrement(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        if (by == 1)
        {
            auto replyResult = executeRaw(std::nothrow, "DECR", {key});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            return extractInteger(std::nothrow, replyResult.value()).value();
        }
        else
        {
            auto replyResult = executeRaw(std::nothrow, "DECRBY", {key, std::to_string(by)});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            return extractInteger(std::nothrow, replyResult.value()).value();
        }
    }

    // List operations - nothrow versions

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::listPushLeft(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "LPUSH", {key, value});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::listPushRight(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "RPUSH", {key, value});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::listPopLeft(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "LPOP", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractString(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::listPopRight(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "RPOP", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractString(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisDBConnection::listRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "LRANGE", {key,
                                                                std::to_string(start),
                                                                std::to_string(stop)});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractArray(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::listLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "LLEN", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
