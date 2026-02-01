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
 * @brief Redis connection implementation - NOTHROW sorted set, scan, server operations
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

        cpp_dbc::expected<bool, DBException> RedisConnection::sortedSetAdd(
            std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
        {
            try
            {
                auto reply = executeRaw("ZADD", {key, std::to_string(score), member});
                return extractInteger(reply) > 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("Z0A1B2C3D4E5",
                                                       std::string("sortedSetAdd failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("F6G7H8I9J0K1",
                                                       "sortedSetAdd failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> RedisConnection::sortedSetRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept
        {
            try
            {
                auto reply = executeRaw("ZREM", {key, member});
                return extractInteger(reply) > 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("L2M3N4O5P6Q7",
                                                       std::string("sortedSetRemove failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("R8S9T0U1V2W3",
                                                       "sortedSetRemove failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::optional<double>, DBException> RedisConnection::sortedSetScore(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept
        {
            try
            {
                auto reply = executeRaw("ZSCORE", {key, member});

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
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("X4Y5Z6A7B8C9",
                                                       std::string("sortedSetScore failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("D0E1F2G3H4I5",
                                                       "sortedSetScore failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::vector<std::string>, DBException> RedisConnection::sortedSetRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
        {
            try
            {
                auto reply = executeRaw("ZRANGE", {key, std::to_string(start), std::to_string(stop)});
                return extractArray(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("J6K7L8M9N0O1",
                                                       std::string("sortedSetRange failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("P2Q3R4S5T6U7",
                                                       "sortedSetRange failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::sortedSetSize(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("ZCARD", {key});
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("V8W9X0Y1Z2A3",
                                                       std::string("sortedSetSize failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("B4C5D6E7F8G9",
                                                       "sortedSetSize failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        // Server operations - nothrow versions

        cpp_dbc::expected<std::vector<std::string>, DBException> RedisConnection::scanKeys(
            std::nothrow_t, const std::string &pattern, int64_t count) noexcept
        {
            try
            {
                std::vector<std::string> result;
                std::string cursor = "0";
                bool done = false;

                while (!done)
                {
                    auto reply = executeRaw("SCAN", {cursor, "MATCH", pattern, "COUNT", std::to_string(count)});

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
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("H0I1J2K3L4M5",
                                                       std::string("scanKeys failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("N6O7P8Q9R0S1",
                                                       "scanKeys failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::string, DBException> RedisConnection::executeCommand(
            std::nothrow_t,
            const std::string &command,
            const std::vector<std::string> &args) noexcept
        {
            try
            {
                auto reply = executeRaw(command, args);

                if (!reply.get())
                {
                    return std::string("");
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
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("T2U3V4W5X6Y7",
                                                       std::string("executeCommand failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("Z8A9B0C1D2E3",
                                                       "executeCommand failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> RedisConnection::flushDB(
            std::nothrow_t, bool async) noexcept
        {
            try
            {
                if (async)
                {
                    auto reply = executeRaw("FLUSHDB", {"ASYNC"});
                    return reply.get()->type == REDIS_REPLY_STATUS &&
                           std::string(reply.get()->str, reply.get()->len) == "OK";
                }
                else
                {
                    auto reply = executeRaw("FLUSHDB", {});
                    return reply.get()->type == REDIS_REPLY_STATUS &&
                           std::string(reply.get()->str, reply.get()->len) == "OK";
                }
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("F4G5H6I7J8K9",
                                                       std::string("flushDB failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("L0M1N2O3P4Q5",
                                                       "flushDB failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::string, DBException> RedisConnection::ping(std::nothrow_t) noexcept
        {
            try
            {
                auto reply = executeRaw("PING", {});
                return extractString(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("R6S7T8U9V0W1",
                                                       std::string("ping failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("X2Y3Z4A5B6C7",
                                                       "ping failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisConnection::getServerInfo(
            std::nothrow_t) noexcept
        {
            try
            {
                std::map<std::string, std::string> result;
                auto reply = executeRaw("INFO", {});

                if (!reply.get() || reply.get()->type != REDIS_REPLY_STRING)
                {
                    return result;
                }

                std::string info(reply.get()->str, reply.get()->len);
                std::istringstream iss(info);
                std::string line;

                while (std::getline(iss, line))
                {
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

                return result;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("D8E9F0G1H2I3",
                                                       std::string("getServerInfo failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("J4K5L6M7N8O9",
                                                       "getServerInfo failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
