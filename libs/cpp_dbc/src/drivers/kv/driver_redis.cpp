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
 * @file driver_redis.cpp
 * @brief Redis database driver implementation
 */

#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#if USE_REDIS

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <cpp_dbc/common/system_utils.hpp>
#include <cpp_dbc/core/db_exception.hpp>

// hiredis headers
#include <hiredis/hiredis.h>

// Debug output is controlled by -DDEBUG_REDIS=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_REDIS) && DEBUG_REDIS) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define REDIS_DEBUG(x) std::cout << "[Redis] " << x << std::endl
#else
#define REDIS_DEBUG(x)
#endif

// Macro for mutex lock guard
#define REDIS_LOCK_GUARD(mtx) std::lock_guard<std::mutex> lock_(mtx)

namespace cpp_dbc
{
    namespace Redis
    {

        // ============================================================================
        // Static member initialization
        // ============================================================================

        std::once_flag RedisDriver::s_initFlag;
        std::atomic<bool> RedisDriver::s_initialized{false};

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
            : m_url(uri), m_dbIndex(0)
        {
            REDIS_DEBUG("RedisConnection::constructor - Connecting to: " << uri);

            // Parse the URI
            std::regex uriRegex("redis://([^:/]+)(?::([0-9]+))?(?:/([0-9]+))?");
            std::smatch matches;

            std::string host = "localhost";
            int port = 6379;

            if (std::regex_search(uri, matches, uriRegex))
            {
                if (matches.size() > 1 && matches[1].matched)
                {
                    host = matches[1].str();
                }

                if (matches.size() > 2 && matches[2].matched)
                {
                    port = std::stoi(matches[2].str());
                }

                if (matches.size() > 3 && matches[3].matched)
                {
                    m_dbIndex = std::stoi(matches[3].str());
                }
            }
            else
            {
                throw DBException("C58E02D9F1A7", "Invalid Redis URI format: " + uri,
                                  system_utils::captureCallStack());
            }

            REDIS_DEBUG("RedisConnection::constructor - Connecting to host: " << host
                                                                              << " port: " << port << " db: " << m_dbIndex);

            // Connect to Redis server
            struct timeval timeout = {3, 0}; // 3 seconds timeout
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
            for (const auto &option : options)
            {
                if (option.first == "client_name" && !option.second.empty())
                {
                    redisReply *reply = (redisReply *)redisCommand(m_context.get(),
                                                                   "CLIENT SETNAME %s",
                                                                   option.second.c_str());
                    if (reply)
                    {
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
            if (!m_closed)
            {
                try
                {
                    close();
                }
                catch (...)
                {
                    // Suppress exceptions in destructor
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
                    close();

                m_context = std::move(other.m_context);
                m_url = std::move(other.m_url);
                m_dbIndex = other.m_dbIndex;
                m_closed = other.m_closed.load();
                m_pooled = other.m_pooled;

                other.m_closed = true;
            }
            return *this;
        }

        void RedisConnection::validateConnection() const
        {
            if (m_closed || !m_context)
            {
                throw DBException("F92C4A6E7D10", "Redis connection is closed or invalid",
                                  system_utils::captureCallStack());
            }
        }

        std::string RedisConnection::extractString(const RedisReplyHandle &reply)
        {
            if (!reply.get())
            {
                return "";
            }

            if (reply.get()->type == REDIS_REPLY_STRING || reply.get()->type == REDIS_REPLY_STATUS)
            {
                return std::string(reply.get()->str, reply.get()->len);
            }
            else if (reply.get()->type == REDIS_REPLY_NIL)
            {
                return "";
            }
            else if (reply.get()->type == REDIS_REPLY_INTEGER)
            {
                return std::to_string(reply.get()->integer);
            }

            return "";
        }

        int64_t RedisConnection::extractInteger(const RedisReplyHandle &reply)
        {
            if (!reply.get())
            {
                return 0;
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
                catch (...)
                {
                    return 0;
                }
            }

            return 0;
        }

        std::vector<std::string> RedisConnection::extractArray(const RedisReplyHandle &reply)
        {
            std::vector<std::string> result;

            if (!reply.get() || reply.get()->type != REDIS_REPLY_ARRAY)
            {
                return result;
            }

            for (size_t i = 0; i < reply.get()->elements; ++i)
            {
                redisReply *element = reply.get()->element[i];
                if (element->type == REDIS_REPLY_STRING || element->type == REDIS_REPLY_STATUS)
                {
                    result.emplace_back(element->str, element->len);
                }
                else if (element->type == REDIS_REPLY_INTEGER)
                {
                    result.push_back(std::to_string(element->integer));
                }
                else if (element->type == REDIS_REPLY_NIL)
                {
                    result.push_back("");
                }
            }

            return result;
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

        bool RedisConnection::isClosed()
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

            redisReply *reply = (redisReply *)redisCommandArgv(
                m_context.get(), static_cast<int>(argv.size()), argv.data(), argvlen.data());

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

        bool RedisConnection::setString(const std::string &key, const std::string &value,
                                        std::optional<int64_t> expirySeconds)
        {
            auto result = setString(std::nothrow, key, value, expirySeconds);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::getString(const std::string &key)
        {
            auto result = getString(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::exists(const std::string &key)
        {
            auto result = exists(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::deleteKey(const std::string &key)
        {
            auto result = deleteKey(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::deleteKeys(const std::vector<std::string> &keys)
        {
            auto result = deleteKeys(std::nothrow, keys);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::expire(const std::string &key, int64_t seconds)
        {
            auto result = expire(std::nothrow, key, seconds);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::getTTL(const std::string &key)
        {
            auto result = getTTL(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // Counter operations

        int64_t RedisConnection::increment(const std::string &key, int64_t by)
        {
            auto result = increment(std::nothrow, key, by);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::decrement(const std::string &key, int64_t by)
        {
            auto result = decrement(std::nothrow, key, by);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // List operations

        int64_t RedisConnection::listPushLeft(const std::string &key, const std::string &value)
        {
            auto result = listPushLeft(std::nothrow, key, value);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::listPushRight(const std::string &key, const std::string &value)
        {
            auto result = listPushRight(std::nothrow, key, value);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::listPopLeft(const std::string &key)
        {
            auto result = listPopLeft(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::listPopRight(const std::string &key)
        {
            auto result = listPopRight(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::listRange(
            const std::string &key, int64_t start, int64_t stop)
        {
            auto result = listRange(std::nothrow, key, start, stop);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::listLength(const std::string &key)
        {
            auto result = listLength(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // Hash operations

        bool RedisConnection::hashSet(
            const std::string &key, const std::string &field, const std::string &value)
        {
            auto result = hashSet(std::nothrow, key, field, value);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::hashGet(const std::string &key, const std::string &field)
        {
            auto result = hashGet(std::nothrow, key, field);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::hashDelete(const std::string &key, const std::string &field)
        {
            auto result = hashDelete(std::nothrow, key, field);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::hashExists(const std::string &key, const std::string &field)
        {
            auto result = hashExists(std::nothrow, key, field);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::map<std::string, std::string> RedisConnection::hashGetAll(const std::string &key)
        {
            auto result = hashGetAll(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::hashLength(const std::string &key)
        {
            auto result = hashLength(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // Set operations

        bool RedisConnection::setAdd(const std::string &key, const std::string &member)
        {
            auto result = setAdd(std::nothrow, key, member);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::setRemove(const std::string &key, const std::string &member)
        {
            auto result = setRemove(std::nothrow, key, member);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::setIsMember(const std::string &key, const std::string &member)
        {
            auto result = setIsMember(std::nothrow, key, member);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::setMembers(const std::string &key)
        {
            auto result = setMembers(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::setSize(const std::string &key)
        {
            auto result = setSize(std::nothrow, key);
            if (!result)
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
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::sortedSetRemove(const std::string &key, const std::string &member)
        {
            auto result = sortedSetRemove(std::nothrow, key, member);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::optional<double> RedisConnection::sortedSetScore(
            const std::string &key, const std::string &member)
        {
            auto result = sortedSetScore(std::nothrow, key, member);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::vector<std::string> RedisConnection::sortedSetRange(
            const std::string &key, int64_t start, int64_t stop)
        {
            auto result = sortedSetRange(std::nothrow, key, start, stop);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int64_t RedisConnection::sortedSetSize(const std::string &key)
        {
            auto result = sortedSetSize(std::nothrow, key);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        // Key scan operations

        std::vector<std::string> RedisConnection::scanKeys(const std::string &pattern, int64_t count)
        {
            auto result = scanKeys(std::nothrow, pattern, count);
            if (!result)
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
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        bool RedisConnection::flushDB(bool async)
        {
            auto result = flushDB(std::nothrow, async);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisConnection::ping()
        {
            auto result = ping(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::map<std::string, std::string> RedisConnection::getServerInfo()
        {
            auto result = getServerInfo(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return *result;
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

        // ============================================================================
        // RedisDriver Implementation
        // ============================================================================

        void RedisDriver::initialize()
        {
            REDIS_DEBUG("RedisDriver::initialize - Initializing Redis driver");
            // No specific initialization needed for hiredis
            s_initialized = true;
            REDIS_DEBUG("RedisDriver::initialize - Done");
        }

        RedisDriver::RedisDriver()
        {
            REDIS_DEBUG("RedisDriver::constructor - Creating driver");
            std::call_once(s_initFlag, initialize);
            REDIS_DEBUG("RedisDriver::constructor - Done");
        }

        RedisDriver::~RedisDriver()
        {
            REDIS_DEBUG("RedisDriver::destructor - Destroying driver");
        }

        bool RedisDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 16) == "cpp_dbc:redis://";
        }

        std::shared_ptr<KVDBConnection> RedisDriver::connectKV(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options)
        {
            auto result = connectKV(std::nothrow, url, user, password, options);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        int RedisDriver::getDefaultPort() const
        {
            return 6379;
        }

        std::string RedisDriver::getURIScheme() const
        {
            return "redis";
        }

        std::map<std::string, std::string> RedisDriver::parseURI(const std::string &uri)
        {
            auto result = parseURI(std::nothrow, uri);
            if (!result)
            {
                throw result.error();
            }
            return *result;
        }

        std::string RedisDriver::buildURI(
            const std::string &host,
            int port,
            const std::string &db,
            [[maybe_unused]] const std::map<std::string, std::string> &options)
        {
            std::ostringstream uri;

            // Start with scheme
            uri << "redis://";

            // Add host
            uri << (host.empty() ? "localhost" : host);

            // Add port
            if (port > 0 && port != 6379)
            {
                uri << ":" << port;
            }

            // Add database index
            if (!db.empty() && db != "0")
            {
                uri << "/" << db;
            }

            // Redis URI doesn't support options in the URI
            // They are handled separately in the connection method

            return uri.str();
        }

        bool RedisDriver::supportsClustering() const
        {
            return true;
        }

        bool RedisDriver::supportsReplication() const
        {
            return true;
        }

        std::string RedisDriver::getDriverVersion() const
        {
            // Get the hiredis version
            return std::to_string(HIREDIS_SONAME);
        }

        void RedisDriver::cleanup()
        {
            REDIS_DEBUG("RedisDriver::cleanup - Cleaning up Redis driver");
            // No specific cleanup needed for hiredis
            s_initialized = false;
            REDIS_DEBUG("RedisDriver::cleanup - Done");
        }

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
                    args.push_back("EX");
                    args.push_back(std::to_string(*expirySeconds));
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
                return cpp_dbc::unexpected(DBException("J4K5L6M7N8O9",
                                                       std::string("deleteKey failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("P0Q1R2S3T4U5",
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
                return cpp_dbc::unexpected(DBException("V6W7X8Y9Z0A1",
                                                       std::string("deleteKeys failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("B2C3D4E5F6G7",
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
                return cpp_dbc::unexpected(DBException("H8I9J0K1L2M3",
                                                       std::string("expire failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("N4O5P6Q7R8S9",
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
                return cpp_dbc::unexpected(DBException("T0U1V2W3X4Y5",
                                                       std::string("getTTL failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("Z6A7B8C9D0E1",
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
                return cpp_dbc::unexpected(DBException("F2G3H4I5J6K7",
                                                       std::string("increment failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("L8M9N0O1P2Q3",
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
                return cpp_dbc::unexpected(DBException("R4S5T6U7V8W9",
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
                return cpp_dbc::unexpected(DBException("J2K3L4M5N6O7",
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
                return cpp_dbc::unexpected(DBException("L6M7N8O9P0Q1",
                                                       std::string("listLength failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("R2S3T4U5V6W7",
                                                       "listLength failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

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
                return cpp_dbc::unexpected(DBException("X8Y9Z0A1B2C3",
                                                       std::string("hashSet failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("D4E5F6G7H8I9",
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
                return cpp_dbc::unexpected(DBException("J0K1L2M3N4O5",
                                                       std::string("hashGet failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("P6Q7R8S9T0U1",
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
                return cpp_dbc::unexpected(DBException("V2W3X4Y5Z6A7",
                                                       std::string("hashDelete failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("B8C9D0E1F2G3",
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
                return cpp_dbc::unexpected(DBException("H4I5J6K7L8M9",
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
                return cpp_dbc::unexpected(DBException("N8O9P0Q1R2S3",
                                                       std::string("setSize failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("T4U5V6W7X8Y9",
                                                       "setSize failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

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
                    try
                    {
                        double score = std::stod(std::string(reply.get()->str, reply.get()->len));
                        return std::optional<double>(score);
                    }
                    catch (...)
                    {
                        return std::optional<double>(std::nullopt);
                    }
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

        // ============================================================================
        // RedisDriver - NOTHROW IMPLEMENTATIONS
        // ============================================================================

        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> RedisDriver::connectKV(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options) noexcept
        {
            try
            {
                REDIS_DEBUG("RedisDriver::connectKV(nothrow) - Connecting to: " << url);
                REDIS_LOCK_GUARD(m_mutex);

                if (!acceptsURL(url))
                {
                    return cpp_dbc::unexpected(DBException("A93B8C7D2E1F", "Invalid Redis URL: " + url,
                                                           system_utils::captureCallStack()));
                }

                std::string redisUrl = url;
                if (url.substr(0, 8) == "cpp_dbc:")
                {
                    redisUrl = url.substr(8);
                }

                auto conn = std::make_shared<RedisConnection>(redisUrl, user, password, options);
                REDIS_DEBUG("RedisDriver::connectKV(nothrow) - Connection established");
                return std::shared_ptr<KVDBConnection>(conn);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("P0Q1R2S3T4U5",
                                                       std::string("connectKV failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("V6W7X8Y9Z0A1",
                                                       "connectKV failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> RedisDriver::parseURI(
            std::nothrow_t, const std::string &uri) noexcept
        {
            try
            {
                std::map<std::string, std::string> result;
                std::regex uriRegex("redis://([^:/]+)(?::([0-9]+))?(?:/([0-9]+))?");
                std::smatch matches;

                if (std::regex_search(uri, matches, uriRegex))
                {
                    if (matches.size() > 1 && matches[1].matched)
                    {
                        result["host"] = matches[1].str();
                    }
                    else
                    {
                        result["host"] = "localhost";
                    }

                    if (matches.size() > 2 && matches[2].matched)
                    {
                        result["port"] = matches[2].str();
                    }
                    else
                    {
                        result["port"] = "6379";
                    }

                    if (matches.size() > 3 && matches[3].matched)
                    {
                        result["db"] = matches[3].str();
                    }
                    else
                    {
                        result["db"] = "0";
                    }
                }
                else
                {
                    return cpp_dbc::unexpected(DBException("F0E1D2C3B4A5", "Invalid Redis URI format: " + uri,
                                                           system_utils::captureCallStack()));
                }

                return result;
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("B2C3D4E5F6G7",
                                                       std::string("parseURI failed: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("H8I9J0K1L2M3",
                                                       "parseURI failed: unknown error",
                                                       system_utils::captureCallStack()));
            }
        }

    } // namespace Redis
} // namespace cpp_dbc

#endif // USE_REDIS
