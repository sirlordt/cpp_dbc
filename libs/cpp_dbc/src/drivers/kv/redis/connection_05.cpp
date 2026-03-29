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
 * @file connection_05.cpp
 * @brief Redis connection implementation - NOTHROW hash, set operations
 */

#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#if USE_REDIS

#include <algorithm>
#include <cstring>
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

    // Hash operations - nothrow versions

    cpp_dbc::expected<bool, DBException> RedisDBConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "HSET", {key, field, value});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "HGET", {key, field});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractString(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "HDEL", {key, field});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "HEXISTS", {key, field});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisDBConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        std::map<std::string, std::string> result;

        auto replyResult = executeRaw(std::nothrow, "HGETALL", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        const auto &reply = replyResult.value();

        if (!reply.get() || reply.get()->type != REDIS_REPLY_ARRAY)
        {
            return result;
        }

        // HGETALL returns an array of alternating field names and values
        for (size_t i = 0; i < reply.get()->elements; i += 2)
        {
            if (i + 1 < reply.get()->elements)
            {
                redisReply *fieldReply = reply.get()->element[i];
                redisReply *valueReply = reply.get()->element[i + 1];

                // Defensive null checks to prevent crashes
                if (!fieldReply || !valueReply)
                {
                    continue;
                }

                std::string field;
                if (fieldReply->type == REDIS_REPLY_STRING)
                {
                    field = std::string(fieldReply->str, fieldReply->len);
                }

                std::string value;
                if (valueReply->type == REDIS_REPLY_STRING)
                {
                    value = std::string(valueReply->str, valueReply->len);
                }

                if (!field.empty())
                {
                    result[field] = value;
                }
            }
        }

        return result;
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "HLEN", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    // Set operations - nothrow versions

    cpp_dbc::expected<bool, DBException> RedisDBConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SADD", {key, member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SREM", {key, member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SISMEMBER", {key, member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisDBConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SMEMBERS", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractArray(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SCARD", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
