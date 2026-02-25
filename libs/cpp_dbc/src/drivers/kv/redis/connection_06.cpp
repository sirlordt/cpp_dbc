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
    // RedisConnection - Private helper methods
    // ============================================================================

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
            catch ([[maybe_unused]] const std::exception &ex)
            {
                REDIS_DEBUG("RedisConnection::extractInteger - Failed to parse: " << ex.what());
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

    std::optional<double> RedisConnection::tryParseDouble(const std::string &str) noexcept
    {
        try
        {
            return std::stod(str);
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            REDIS_DEBUG("RedisConnection::tryParseDouble - Failed to parse: " << str << " error: " << ex.what());
            return std::nullopt;
        }
        catch (...)
        {
            REDIS_DEBUG("RedisConnection::tryParseDouble - Failed to parse: " << str << " unknown error");
            return std::nullopt;
        }
    }

    // ============================================================================
    // RedisConnection - DBConnection nothrow interface implementations
    // ============================================================================

    cpp_dbc::expected<void, DBException> RedisConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            std::scoped_lock lock_(m_mutex);
            if (m_closed)
                return {};
            REDIS_DEBUG("RedisConnection::close(nothrow) - Closing connection");
            m_context.reset();
            m_closed = true;
            REDIS_DEBUG("RedisConnection::close(nothrow) - Connection closed");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("JS1AQU3IVXMG",
                std::string("Exception in close: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("2G3CVDMF77RN",
                "Unknown exception in close",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> RedisConnection::reset(std::nothrow_t) noexcept
    {
        try
        {
            // Redis has no transaction state to reset; this is a no-op
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("BN1EIHAOCZZB",
                std::string("Exception in reset: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("WBSY09JYSLVA",
                "Unknown exception in reset",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load();
    }

    cpp_dbc::expected<void, DBException> RedisConnection::returnToPool(std::nothrow_t) noexcept
    {
        return reset(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> RedisConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> RedisConnection::getURL(std::nothrow_t) const noexcept
    {
        try
        {
            return m_url;
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("LE7RHOIPQSA5",
                std::string("Exception in getURL: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("JKHJGVFHTG27",
                "Unknown exception in getURL",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> RedisConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        try
        {
            // Redis has no transaction state or open cursors to clean up; this is a no-op
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("8TV446WG7S01",
                std::string("Exception in prepareForPoolReturn: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("84KIJ3GX10KN",
                "Unknown exception in prepareForPoolReturn",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
