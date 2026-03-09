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
 * @file connection_06.cpp
 * @brief Redis connection implementation - NOTHROW sorted set, scan, server operations, getDatabaseIndex, executeRaw, selectDatabase
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

    // Sorted set operations - nothrow versions

    cpp_dbc::expected<bool, DBException> RedisDBConnection::sortedSetAdd(
        std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "ZADD", {key, std::to_string(score), member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::sortedSetRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "ZREM", {key, member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value() > 0;
    }

    cpp_dbc::expected<std::optional<double>, DBException> RedisDBConnection::sortedSetScore(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "ZSCORE", {key, member});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        const auto &reply = replyResult.value();

        if (!reply.get() || reply.get()->type == REDIS_REPLY_NIL)
        {
            return std::optional<double>(std::nullopt);
        }

        if (reply.get()->type == REDIS_REPLY_STRING)
        {
            return tryParseDouble(std::string(reply.get()->str, reply.get()->len));
        }

        return std::optional<double>(std::nullopt);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisDBConnection::sortedSetRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "ZRANGE", {key, std::to_string(start), std::to_string(stop)});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractArray(std::nothrow, replyResult.value()).value();
    }

    cpp_dbc::expected<int64_t, DBException> RedisDBConnection::sortedSetSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "ZCARD", {key});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractInteger(std::nothrow, replyResult.value()).value();
    }

    // Server operations - nothrow versions

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisDBConnection::scanKeys(
        std::nothrow_t, const std::string &pattern, int64_t count) noexcept
    {
        std::vector<std::string> result;
        std::string cursor = "0";
        bool done = false;

        while (!done)
        {
            auto replyResult = executeRaw(std::nothrow, "SCAN", {cursor, "MATCH", pattern, "COUNT", std::to_string(count)});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            const auto &reply = replyResult.value();

            if (!reply.get() || reply.get()->type != REDIS_REPLY_ARRAY || reply.get()->elements != 2)
            {
                break;
            }

            redisReply *cursorReply = reply.get()->element[0];
            if (cursorReply->type == REDIS_REPLY_STRING)
            {
                cursor = std::string(cursorReply->str, cursorReply->len);
            }

            redisReply *keysReply = reply.get()->element[1];
            if (keysReply->type == REDIS_REPLY_ARRAY)
            {
                for (size_t i = 0; i < keysReply->elements; ++i)
                {
                    redisReply *keyReply = keysReply->element[i];
                    if (keyReply->type == REDIS_REPLY_STRING)
                    {
                        result.emplace_back(keyReply->str, keyReply->len);
                    }
                }
            }

            if (cursor == "0")
            {
                done = true;
            }
        }

        return result;
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::executeCommand(
        std::nothrow_t,
        const std::string &command,
        const std::vector<std::string> &args) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, command, args);
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        const auto &reply = replyResult.value();

        if (!reply.get())
        {
            return std::string{};
        }

        switch (reply.get()->type)
        {
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_STRING:
            return std::string(reply.get()->str, reply.get()->len);

        case REDIS_REPLY_INTEGER:
            return std::to_string(reply.get()->integer);

        case REDIS_REPLY_NIL:
            return std::string("(nil)");

        case REDIS_REPLY_ARRAY:
        {
            std::stringstream ss;
            ss << "(array of " << reply.get()->elements << " elements)";
            return ss.str();
        }

        default:
            return std::string("(unknown reply type)");
        }
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::flushDB(
        std::nothrow_t, bool async) noexcept
    {
        if (async)
        {
            auto replyResult = executeRaw(std::nothrow, "FLUSHDB", {"ASYNC"});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            const auto &reply = replyResult.value();
            if (!reply.get())
            {
                return cpp_dbc::unexpected(DBException("9V0TERUH0YQ2",
                                                       "flushDB failed: null reply",
                                                       system_utils::captureCallStack()));
            }
            return reply.get()->type == REDIS_REPLY_STATUS &&
                   std::string(reply.get()->str, reply.get()->len) == "OK";
        }
        else
        {
            auto replyResult = executeRaw(std::nothrow, "FLUSHDB", {});
            if (!replyResult.has_value())
            {
                return cpp_dbc::unexpected(replyResult.error());
            }
            const auto &reply = replyResult.value();
            if (!reply.get())
            {
                return cpp_dbc::unexpected(DBException("O4T6A13A7KTC",
                                                       "flushDB failed: null reply",
                                                       system_utils::captureCallStack()));
            }
            return reply.get()->type == REDIS_REPLY_STATUS &&
                   std::string(reply.get()->str, reply.get()->len) == "OK";
        }
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::ping(std::nothrow_t) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "PING", {});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        return extractString(std::nothrow, replyResult.value()).value() == "PONG";
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisDBConnection::getServerInfo(
        std::nothrow_t) noexcept
    {
        std::map<std::string, std::string> result;

        auto replyResult = executeRaw(std::nothrow, "INFO", {});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }
        const auto &reply = replyResult.value();

        if (!reply.get() || reply.get()->type != REDIS_REPLY_STRING)
        {
            return result;
        }

        std::string info(reply.get()->str, reply.get()->len);
        std::istringstream iss(info);
        std::string line;

        while (std::getline(iss, line))
        {
            // Remove trailing CR if present (Redis uses CRLF line endings)
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            size_t pos = line.find(':');
            if (pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                result[key] = value;
            }
        }

        // Ensure "ServerVersion" is always present (standard key across all drivers)
        auto it = result.find("redis_version");
        if (it != result.end())
        {
            result["ServerVersion"] = it->second;
        }

        return result;
    }

    int RedisDBConnection::getDatabaseIndex(std::nothrow_t) const noexcept
    {
        return m_dbIndex;
    }

    cpp_dbc::expected<RedisReplyHandle, DBException> RedisDBConnection::executeRaw(
        std::nothrow_t, const std::string &command, const std::vector<std::string> &args) noexcept
    {
        REDIS_LOCK_GUARD(m_mutex);

        auto connCheck = validateConnection(std::nothrow);
        if (!connCheck.has_value())
        {
            return cpp_dbc::unexpected(connCheck.error());
        }

        // Prepare the command and arguments
        std::vector<const char *> argv;
        std::vector<size_t> argvlen;

        argv.push_back(command.c_str());
        argvlen.push_back(command.length());

        for (const auto &arg : args)
        {
            argv.push_back(arg.c_str());
            argvlen.push_back(arg.length());
        }

        auto *reply = static_cast<redisReply *>(redisCommandArgv(
            m_context.get(), static_cast<int>(argv.size()), argv.data(), argvlen.data()));

        if (!reply)
        {
            return cpp_dbc::unexpected(DBException("IYULQRKMNKJ9",
                                                   "Redis command execution failed: " + command,
                                                   system_utils::captureCallStack()));
        }

        if (reply->type == REDIS_REPLY_ERROR)
        {
            std::string errorMsg = reply->str ? reply->str : "Unknown error";
            freeReplyObject(reply);
            return cpp_dbc::unexpected(DBException("MOPD5WOXBBPJ",
                                                   "Redis command error: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        return RedisReplyHandle(std::nothrow, reply);
    }

    cpp_dbc::expected<void, DBException> RedisDBConnection::selectDatabase(
        std::nothrow_t, int index) noexcept
    {
        auto replyResult = executeRaw(std::nothrow, "SELECT", {std::to_string(index)});
        if (!replyResult.has_value())
        {
            return cpp_dbc::unexpected(replyResult.error());
        }

        const auto &reply = replyResult.value();
        if (reply.get()->type == REDIS_REPLY_STATUS &&
            std::string(reply.get()->str, reply.get()->len) == "OK")
        {
            m_dbIndex = index;
            return {};
        }

        return cpp_dbc::unexpected(DBException("4Y6O0DLL7OEX",
                                               "Failed to select Redis database: " + std::to_string(index),
                                               system_utils::captureCallStack()));
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        auto infoResult = getServerInfo(std::nothrow);
        if (!infoResult.has_value())
        {
            return cpp_dbc::unexpected(infoResult.error());
        }

        const auto &info = infoResult.value();
        auto it = info.find("redis_version");
        if (it == info.end())
        {
            return cpp_dbc::unexpected(DBException(
                "ECC2DGY7VHYU",
                "Server INFO response does not contain 'redis_version'",
                system_utils::captureCallStack()));
        }

        return it->second;
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
