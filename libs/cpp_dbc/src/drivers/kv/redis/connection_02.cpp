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
 * @file connection_02.cpp
 * @brief Redis connection implementation - hash, set, sorted set, scan, server throwing + executeRaw, getDatabaseIndex, selectDatabase, setPooled
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

        // Hash operations

        bool RedisConnection::hashSet(
            const std::string &key, const std::string &field, const std::string &value)
        {
            auto result = hashSet(std::nothrow, key, field, value);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::hashGet(const std::string &key, const std::string &field)
        {
            auto result = hashGet(std::nothrow, key, field);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::hashDelete(const std::string &key, const std::string &field)
        {
            auto result = hashDelete(std::nothrow, key, field);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::hashExists(const std::string &key, const std::string &field)
        {
            auto result = hashExists(std::nothrow, key, field);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::map<std::string, std::string> RedisConnection::hashGetAll(const std::string &key)
        {
            auto result = hashGetAll(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::hashLength(const std::string &key)
        {
            auto result = hashLength(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // Set operations

        bool RedisConnection::setAdd(const std::string &key, const std::string &member)
        {
            auto result = setAdd(std::nothrow, key, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::setRemove(const std::string &key, const std::string &member)
        {
            auto result = setRemove(std::nothrow, key, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::setIsMember(const std::string &key, const std::string &member)
        {
            auto result = setIsMember(std::nothrow, key, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::setMembers(const std::string &key)
        {
            auto result = setMembers(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::setSize(const std::string &key)
        {
            auto result = setSize(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // Sorted set operations

        bool RedisConnection::sortedSetAdd(
            const std::string &key, double score, const std::string &member)
        {
            auto result = sortedSetAdd(std::nothrow, key, score, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::sortedSetRemove(const std::string &key, const std::string &member)
        {
            auto result = sortedSetRemove(std::nothrow, key, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::optional<double> RedisConnection::sortedSetScore(
            const std::string &key, const std::string &member)
        {
            auto result = sortedSetScore(std::nothrow, key, member);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::sortedSetRange(
            const std::string &key, int64_t start, int64_t stop)
        {
            auto result = sortedSetRange(std::nothrow, key, start, stop);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::sortedSetSize(const std::string &key)
        {
            auto result = sortedSetSize(std::nothrow, key);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // Key scan operations

        std::vector<std::string> RedisConnection::scanKeys(const std::string &pattern, int64_t count)
        {
            auto result = scanKeys(std::nothrow, pattern, count);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        // Server operations

        std::string RedisConnection::executeCommand(
            const std::string &command, const std::vector<std::string> &args)
        {
            auto result = executeCommand(std::nothrow, command, args);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::flushDB(bool async)
        {
            auto result = flushDB(std::nothrow, async);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::ping()
        {
            auto result = ping(std::nothrow);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        std::map<std::string, std::string> RedisConnection::getServerInfo()
        {
            auto result = getServerInfo(std::nothrow);
            if (!result.has_value())
            {
                throw result.error();
            }
            return *result;
        }

        RedisReplyHandle RedisConnection::executeRaw(
            const std::string &command, const std::vector<std::string> &args)
        {
            REDIS_LOCK_GUARD(m_mutex);
            validateConnection();

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
                throw DBException("E8B7F2C9A0D3", "Redis command execution failed: " + command,
                                  system_utils::captureCallStack());
            }

            if (reply->type == REDIS_REPLY_ERROR)
            {
                std::string errorMsg = reply->str ? reply->str : "Unknown error";
                freeReplyObject(reply);
                throw DBException("E1B9C4A6D0F2", "Redis command error: " + errorMsg,
                                  system_utils::captureCallStack());
            }

            return RedisReplyHandle(reply);
        }

        int RedisConnection::getDatabaseIndex() const
        {
            return m_dbIndex;
        }

        void RedisConnection::selectDatabase(int index)
        {
            auto reply = executeRaw("SELECT", {std::to_string(index)});

            if (reply.get()->type == REDIS_REPLY_STATUS &&
                std::string(reply.get()->str, reply.get()->len) == "OK")
            {
                m_dbIndex = index;
            }
            else
            {
                throw DBException("B2E7F1C9A0D3", "Failed to select Redis database: " + std::to_string(index), system_utils::captureCallStack());
            }
        }

        void RedisConnection::setPooled(bool pooled)
        {
            m_pooled = pooled;
        }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
