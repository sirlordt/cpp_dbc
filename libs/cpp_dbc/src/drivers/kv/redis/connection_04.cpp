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
 * @brief Redis connection implementation - NOTHROW hash, set operations
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

    // Hash operations - nothrow versions

    cpp_dbc::expected<bool, DBException> RedisConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        try
        {
            auto reply = executeRaw("HSET", {key, field, value});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RDFO6G7H8I9J",
                                                   std::string("hashSet failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RDGP7H8I9J0K",
                                                   "hashSet failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> RedisConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        try
        {
            auto reply = executeRaw("HGET", {key, field});
            return extractString(reply);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RD0E1F2A3B4C",
                                                   std::string("hashGet failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RDHQ8I9J0K1L",
                                                   "hashGet failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        try
        {
            auto reply = executeRaw("HDEL", {key, field});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RDIR9J0K1L2M",
                                                   std::string("hashDelete failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RDJS0K1L2M3N",
                                                   "hashDelete failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        try
        {
            auto reply = executeRaw("HEXISTS", {key, field});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RDKT1L2M3N4O",
                                                   std::string("hashExists failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("N0O1P2Q3R4S5",
                                                   "hashExists failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        try
        {
            std::map<std::string, std::string> result;
            auto reply = executeRaw("HGETALL", {key});

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("T6U7V8W9X0Y1",
                                                   std::string("hashGetAll failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("Z2A3B4C5D6E7",
                                                   "hashGetAll failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> RedisConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        try
        {
            auto reply = executeRaw("HLEN", {key});
            return extractInteger(reply);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F8G9H0I1J2K3",
                                                   std::string("hashLength failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("L4M5N6O7P8Q9",
                                                   "hashLength failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    // Set operations - nothrow versions

    cpp_dbc::expected<bool, DBException> RedisConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        try
        {
            auto reply = executeRaw("SADD", {key, member});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("R0S1T2U3V4W5",
                                                   std::string("setAdd failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("X6Y7Z8A9B0C1",
                                                   "setAdd failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        try
        {
            auto reply = executeRaw("SREM", {key, member});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D2E3F4G5H6I7",
                                                   std::string("setRemove failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("J8K9L0M1N2O3",
                                                   "setRemove failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        try
        {
            auto reply = executeRaw("SISMEMBER", {key, member});
            return extractInteger(reply) > 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("P4Q5R6S7T8U9",
                                                   std::string("setIsMember failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("V0W1X2Y3Z4A5",
                                                   "setIsMember failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> RedisConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        try
        {
            auto reply = executeRaw("SMEMBERS", {key});
            return extractArray(reply);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B6C7D8E9F0G1",
                                                   std::string("setMembers failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H2I3J4K5L6M7",
                                                   "setMembers failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> RedisConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        try
        {
            auto reply = executeRaw("SCARD", {key});
            return extractInteger(reply);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RDLU2M3N4O5P",
                                                   std::string("setSize failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RDMV3N4O5P6Q",
                                                   "setSize failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
