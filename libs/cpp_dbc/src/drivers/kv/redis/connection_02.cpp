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
 * @brief Redis connection implementation - hash, set, sorted set, scan, server throwing + executeRaw, selectDatabase
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

#ifdef __cpp_exceptions
    bool RedisDBConnection::hashSet(
        const std::string &key, const std::string &field, const std::string &value)
    {
        auto result = hashSet(std::nothrow, key, field, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string RedisDBConnection::hashGet(const std::string &key, const std::string &field)
    {
        auto result = hashGet(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::hashDelete(const std::string &key, const std::string &field)
    {
        auto result = hashDelete(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::hashExists(const std::string &key, const std::string &field)
    {
        auto result = hashExists(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::map<std::string, std::string> RedisDBConnection::hashGetAll(const std::string &key)
    {
        auto result = hashGetAll(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::hashLength(const std::string &key)
    {
        auto result = hashLength(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Set operations

    bool RedisDBConnection::setAdd(const std::string &key, const std::string &member)
    {
        auto result = setAdd(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::setRemove(const std::string &key, const std::string &member)
    {
        auto result = setRemove(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::setIsMember(const std::string &key, const std::string &member)
    {
        auto result = setIsMember(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> RedisDBConnection::setMembers(const std::string &key)
    {
        auto result = setMembers(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::setSize(const std::string &key)
    {
        auto result = setSize(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Sorted set operations

    bool RedisDBConnection::sortedSetAdd(
        const std::string &key, double score, const std::string &member)
    {
        auto result = sortedSetAdd(std::nothrow, key, score, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::sortedSetRemove(const std::string &key, const std::string &member)
    {
        auto result = sortedSetRemove(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::optional<double> RedisDBConnection::sortedSetScore(
        const std::string &key, const std::string &member)
    {
        auto result = sortedSetScore(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> RedisDBConnection::sortedSetRange(
        const std::string &key, int64_t start, int64_t stop)
    {
        auto result = sortedSetRange(std::nothrow, key, start, stop);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int64_t RedisDBConnection::sortedSetSize(const std::string &key)
    {
        auto result = sortedSetSize(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Key scan operations

    std::vector<std::string> RedisDBConnection::scanKeys(const std::string &pattern, int64_t count)
    {
        auto result = scanKeys(std::nothrow, pattern, count);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // Server operations

    std::string RedisDBConnection::executeCommand(
        const std::string &command, const std::vector<std::string> &args)
    {
        auto result = executeCommand(std::nothrow, command, args);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::flushDB(bool async)
    {
        auto result = flushDB(std::nothrow, async);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool RedisDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::map<std::string, std::string> RedisDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void RedisDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel RedisDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    RedisReplyHandle RedisDBConnection::executeRaw(
        const std::string &command, const std::vector<std::string> &args)
    {
        auto r = executeRaw(std::nothrow, command, args);
        if (!r.has_value())
        {
            throw r.error();
        }
        return std::move(r.value());
    }

    void RedisDBConnection::selectDatabase(int index)
    {
        auto r = selectDatabase(std::nothrow, index);
        if (!r.has_value())
        {
            throw r.error();
        }
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
