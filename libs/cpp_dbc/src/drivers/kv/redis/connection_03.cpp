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
 * @brief Redis connection implementation - private helper methods and DBConnection nothrow interface
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

    // ============================================================================
    // RedisDBConnection - Nothrow interface implementations (same order as .hpp)
    // ============================================================================

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

        return RedisReplyHandle(reply);
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

    cpp_dbc::expected<void, DBException> RedisDBConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            std::scoped_lock lock_(m_mutex);
            if (m_closed.load(std::memory_order_acquire))
            {
                return {};
            }
            REDIS_DEBUG("RedisDBConnection::close(nothrow) - Closing connection");
            m_context.reset();
            m_closed.store(true, std::memory_order_release);
            REDIS_DEBUG("RedisDBConnection::close(nothrow) - Connection closed");
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("JS1AQU3IVXMG",
                std::string("Exception in close: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("2G3CVDMF77RN",
                "Unknown exception in close",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> RedisDBConnection::reset(std::nothrow_t) noexcept
    {
        // Redis has no transaction state to reset; this is a no-op
        return {};
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> RedisDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        return reset(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> RedisDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::getURL(std::nothrow_t) const noexcept
    {
        try
        {
            return m_url;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("LE7RHOIPQSA5",
                std::string("Exception in getURL: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("JKHJGVFHTG27",
                "Unknown exception in getURL",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> RedisDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        // Redis has no transaction state or open cursors to clean up; this is a no-op
        return {};
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
