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
 * @brief Redis connection implementation - DBConnection nothrow interface
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
    // RedisDBConnection - Nothrow interface implementations (same order as .hpp)
    // ============================================================================

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

            // Release live connection count for cleanup() guard.
            // Safe against double-decrement: the m_closed check above
            // returns early if already closed, so this line executes exactly once.
            RedisDBDriver::s_liveConnectionCount.fetch_sub(1, std::memory_order_release);

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

    cpp_dbc::expected<std::string, DBException> RedisDBConnection::getURI(std::nothrow_t) const noexcept
    {
        try
        {
            return m_uri;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("LE7RHOIPQSA5",
                std::string("Exception in getURI: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("JKHJGVFHTG27",
                "Unknown exception in getURI",
                system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    RedisDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        REDIS_LOCK_GUARD(m_mutex);
        m_transactionIsolation = level;
        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    RedisDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        REDIS_LOCK_GUARD(m_mutex);
        return m_transactionIsolation;
    }

    cpp_dbc::expected<void, DBException>
    RedisDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        REDIS_LOCK_GUARD(m_mutex);
        // Redis has no transaction state or open cursors to clean up.
        // Restore isolation level if requested (store-only, no DB command).
        if (isolationLevel != TransactionIsolationLevel::TRANSACTION_NONE)
        {
            m_transactionIsolation = isolationLevel;
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> RedisDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // Redis connections are stateless; no refresh needed on borrow
        return {};
    }

} // namespace cpp_dbc::Redis

#endif // USE_REDIS
