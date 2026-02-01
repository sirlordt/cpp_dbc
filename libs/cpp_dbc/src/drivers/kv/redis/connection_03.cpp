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
 * @file connection_03.cpp
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
        // RedisConnection - NOTHROW IMPLEMENTATIONS (Real logic)
        // ============================================================================

        cpp_dbc::expected<bool, DBException> RedisConnection::setString(
            std::nothrow_t,
            const std::string &key,
            const std::string &value,
            std::optional<int64_t> expirySeconds) noexcept
        {
            try
            {
                std::vector<std::string> args = {key, value};

                if (expirySeconds.has_value())
                {
                    args.emplace_back("EX");
                    args.emplace_back(std::to_string(*expirySeconds));
                }

                auto reply = executeRaw("SET", args);
                bool result = reply.get()->type == REDIS_REPLY_STATUS &&
                              std::string(reply.get()->str, reply.get()->len) == "OK";
                return result;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("E8A9B0C1D2E3",
                                                       std::string("setString failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("F4G5H6I7J8K9",
                                                       "setString failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::string, DBException> RedisConnection::getString(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("GET", {key});
                return extractString(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("L0M1N2O3P4Q5",
                                                       std::string("getString failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("R6S7T8U9V0W1",
                                                       "getString failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> RedisConnection::exists(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("EXISTS", {key});
                return extractInteger(reply) > 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("X2Y3Z4A5B6C7",
                                                       std::string("exists failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("D8E9F0G1H2I3",
                                                       "exists failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> RedisConnection::deleteKey(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("DEL", {key});
                return extractInteger(reply) > 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RD1A2B3C4D5E",
                                                       std::string("deleteKey failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RD2B3C4D5E6F",
                                                       "deleteKey failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::deleteKeys(
            std::nothrow_t, const std::vector<std::string> &keys) noexcept
        {
            try
            {
                if (keys.empty())
                {
                    return 0;
                }

                auto reply = executeRaw("DEL", keys);
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RD3C4D5E6F7A",
                                                       std::string("deleteKeys failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RD4D5E6F7A8B",
                                                       "deleteKeys failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> RedisConnection::expire(
            std::nothrow_t, const std::string &key, int64_t seconds) noexcept
        {
            try
            {
                auto reply = executeRaw("EXPIRE", {key, std::to_string(seconds)});
                return extractInteger(reply) > 0;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RD5E6F7A8B9C",
                                                       std::string("expire failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RD6F7A8B9C0D",
                                                       "expire failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::getTTL(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("TTL", {key});
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RD7G8A9B0C1D",
                                                       std::string("getTTL failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RD8H9A0B1C2D",
                                                       "getTTL failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::increment(
            std::nothrow_t, const std::string &key, int64_t by) noexcept
        {
            try
            {
                if (by == 1)
                {
                    auto reply = executeRaw("INCR", {key});
                    return extractInteger(reply);
                }
                else
                {
                    auto reply = executeRaw("INCRBY", {key, std::to_string(by)});
                    return extractInteger(reply);
                }
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RD9I0A1B2C3D",
                                                       std::string("increment failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RDAJ1B2C3D4E",
                                                       "increment failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::decrement(
            std::nothrow_t, const std::string &key, int64_t by) noexcept
        {
            try
            {
                if (by == 1)
                {
                    auto reply = executeRaw("DECR", {key});
                    return extractInteger(reply);
                }
                else
                {
                    auto reply = executeRaw("DECRBY", {key, std::to_string(by)});
                    return extractInteger(reply);
                }
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RDBK2C3D4E5F",
                                                       std::string("decrement failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("X0Y1Z2A3B4C5",
                                                       "decrement failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        // List operations - nothrow versions

        cpp_dbc::expected<int64_t, DBException> RedisConnection::listPushLeft(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept
        {
            try
            {
                auto reply = executeRaw("LPUSH", {key, value});
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("D6E7F8G9H0I1",
                                                       std::string("listPushLeft failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RDCL3D4E5F6G",
                                                       "listPushLeft failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::listPushRight(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept
        {
            try
            {
                auto reply = executeRaw("RPUSH", {key, value});
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("P8Q9R0S1T2U3",
                                                       std::string("listPushRight failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("V4W5X6Y7Z8A9",
                                                       "listPushRight failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::string, DBException> RedisConnection::listPopLeft(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("LPOP", {key});
                return extractString(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("B0C1D2E3F4G5",
                                                       std::string("listPopLeft failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("H6I7J8K9L0M1",
                                                       "listPopLeft failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::string, DBException> RedisConnection::listPopRight(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("RPOP", {key});
                return extractString(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("N2O3P4Q5R6S7",
                                                       std::string("listPopRight failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("T8U9V0W1X2Y3",
                                                       "listPopRight failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::vector<std::string>, DBException> RedisConnection::listRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
        {
            try
            {
                auto reply = executeRaw("LRANGE", {key,
                                                   std::to_string(start),
                                                   std::to_string(stop)});
                return extractArray(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("Z4A5B6C7D8E9",
                                                       std::string("listRange failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("F0G1H2I3J4K5",
                                                       "listRange failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<int64_t, DBException> RedisConnection::listLength(
            std::nothrow_t, const std::string &key) noexcept
        {
            try
            {
                auto reply = executeRaw("LLEN", {key});
                return extractInteger(reply);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("RDDM4E5F6G7H",
                                                       std::string("listLength failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("RDEN5F6G7H8I",
                                                       "listLength failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
