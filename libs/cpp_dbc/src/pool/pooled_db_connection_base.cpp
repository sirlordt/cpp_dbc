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
 * @file pooled_db_connection_base.cpp
 * @brief Template implementations for PooledDBConnectionBase + explicit instantiations
 *
 * All four family-specific PooledDBConnection classes share identical logic for:
 * - close/returnToPool (complex atomic exchange + race-condition fix)
 * - destructor (active-count decrement + underlying close)
 * - pool metadata (creation time, last used time, active state, etc.)
 *
 * This file contains the template method implementations and explicit instantiations
 * for Relational, KV, Document, and Columnar families.
 */

#include "cpp_dbc/pool/pooled_db_connection_base.hpp"

// Include all four family headers — needed for explicit instantiation (Derived must be complete)
#include "cpp_dbc/pool/relational/relational_db_connection_pool.hpp"
#include "cpp_dbc/pool/kv/kv_db_connection_pool.hpp"
#include "cpp_dbc/pool/document/document_db_connection_pool.hpp"
#include "cpp_dbc/pool/columnar/columnar_db_connection_pool.hpp"

#include "cpp_dbc/common/system_utils.hpp"
#include "connection_pool_internal.hpp"

namespace cpp_dbc
{

    // ── Constructor ───────────────────────────────────────────────────────────

    template <typename Derived, typename ConnType, typename PoolType>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::PooledDBConnectionBase(
        std::shared_ptr<ConnType> connection,
        std::weak_ptr<PoolType> pool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : m_conn(std::move(connection)), m_pool(std::move(pool)), m_poolAlive(std::move(poolAlive))
    {
        // m_creationTime and m_lastUsedTimeNs use in-class initializers
    }

    // ── Destructor ────────────────────────────────────────────────────────────

    template <typename Derived, typename ConnType, typename PoolType>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::~PooledDBConnectionBase()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            // CRITICAL: Never call returnToPool() or shared_from_this() from destructor.
            // When the destructor runs, refcount is already 0, so shared_from_this()
            // throws bad_weak_ptr. Instead, do direct cleanup:
            // 1. Decrement active counter if this connection was active
            // 2. Close the underlying physical connection
            if (m_active.load(std::memory_order_acquire))
            {
                if (auto pool = m_pool.lock())
                {
                    pool->decrementActiveCount(std::nothrow);
                }
                m_active.store(false, std::memory_order_release);
            }

            auto closeResult = m_conn->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("~PooledDBConnectionBase - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

    // ── DBConnectionPooled interface overrides (no diamond) ───────────────────

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    template <typename Derived, typename ConnType, typename PoolType>
    std::chrono::time_point<std::chrono::steady_clock>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    template <typename Derived, typename ConnType, typename PoolType>
    std::chrono::time_point<std::chrono::steady_clock>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_acquire)}};
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::setActive(std::nothrow_t, bool active) noexcept
    {
        m_active.store(active, std::memory_order_release);
        return {};
    }

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    std::shared_ptr<DBConnection>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    void PooledDBConnectionBase<Derived, ConnType, PoolType>::markPoolClosed(std::nothrow_t, bool closed) noexcept
    {
        m_closed.store(closed, std::memory_order_release);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::isPoolClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    void PooledDBConnectionBase<Derived, ConnType, PoolType>::updateLastUsedTime(std::nothrow_t) noexcept
    {
        m_lastUsedTimeNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }

    // ── DBConnection Impl methods (called by derived delegators) ──────────────

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::closeImpl(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            try
            {
                // Return to pool instead of actually closing
                updateLastUsedTime(std::nothrow);

                // Check if pool is still alive using the shared atomic flag,
                // then try to obtain a shared_ptr from the weak_ptr
                auto poolShared = isPoolValid(std::nothrow) ? m_pool.lock() : nullptr;

                if (poolShared)
                {
                    // ============================================================================
                    // CRITICAL FIX: Race Condition in Connection Pool Return Flow
                    // ============================================================================
                    //
                    // BUG DESCRIPTION:
                    // ----------------
                    // A race condition existed where m_closed was reset to false AFTER
                    // returnConnection() completed. This created a window where another thread
                    // could obtain the connection from the idle queue while m_closed was still
                    // true, causing spurious "Connection is closed" errors.
                    //
                    // FIX:
                    // ----
                    // Reset m_closed to false BEFORE calling returnConnection(). This ensures
                    // that when the connection becomes available in the idle queue, m_closed
                    // is already false and any thread that obtains it will see the correct state.
                    //
                    // If returnConnection() fails or throws an exception, the catch blocks below
                    // will close the underlying connection, maintaining correct error handling.
                    // ============================================================================
                    m_closed.store(false, std::memory_order_release);

                    auto self = static_cast<Derived *>(this)->shared_from_this();
                    auto returnResult = poolShared->returnConnection(std::nothrow, self);
                    if (!returnResult.has_value())
                    {
                        // returnConnection rejected the connection — restore closed state
                        // and dispose the underlying connection to prevent resource leak
                        CP_DEBUG("PooledDBConnectionBase::closeImpl - returnConnection failed: %s", returnResult.error().what_s().data());
                        m_closed.store(true, std::memory_order_release);
                        if (m_conn)
                        {
                            m_conn->close(std::nothrow);
                        }
                        return returnResult;
                    }
                }
                else if (m_conn)
                {
                    // Pool is invalid or was destroyed between isPoolValid() and lock() —
                    // actually close the underlying connection to prevent resource leak
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        CP_DEBUG("PooledDBConnectionBase::closeImpl - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::exception &ex)
            {
                // Only shared_from_this() can throw here (std::bad_weak_ptr).
                // All other inner calls are nothrow. Close the connection and return error.
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("6J52BKYDQQOM",
                                                       "Exception in close: " + std::string(ex.what()),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
            {
                CP_DEBUG("PooledDBConnectionBase::closeImpl - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("UQQSB03ZV8CR",
                                                       "Unknown exception in close",
                                                       system_utils::captureCallStack()));
            }
        }

        return {};
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::returnToPoolImpl(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return {};
        }

        try
        {
            // Return to pool instead of actually closing
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag,
            // then try to obtain a shared_ptr from the weak_ptr.
            // Use qualified call to avoid virtual dispatch issues when called from destructor.
            auto poolShared = PooledDBConnectionBase::isPoolValid(std::nothrow) ? m_pool.lock() : nullptr;

            if (poolShared)
            {
                // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                // (see closeImpl() method for full explanation of the bug)
                m_closed.store(false, std::memory_order_release);

                auto self = static_cast<Derived *>(this)->shared_from_this();
                auto returnResult = poolShared->returnConnection(std::nothrow, self);
                if (!returnResult.has_value())
                {
                    // returnConnection rejected the connection — restore closed state
                    // and dispose the underlying connection to prevent resource leak
                    CP_DEBUG("PooledDBConnectionBase::returnToPoolImpl - returnConnection failed: %s", returnResult.error().what_s().data());
                    m_closed.store(true, std::memory_order_release);
                    if (m_conn)
                    {
                        m_conn->close(std::nothrow);
                    }
                    return returnResult;
                }
            }
            else if (m_conn)
            {
                // Pool was destroyed between isPoolValid() and lock() — close the
                // underlying connection since no pool exists to manage it anymore
                auto result = m_conn->close(std::nothrow);
                if (!result.has_value())
                {
                    CP_DEBUG("PooledDBConnectionBase::returnToPoolImpl - Failed to close orphaned connection: %s", result.error().what_s().data());
                    return result;
                }
            }
            return {};
        }
        catch (const std::exception &ex)
        {
            // Only shared_from_this() can throw here (std::bad_weak_ptr).
            // All other inner calls are nothrow. Close the connection and return error.
            CP_DEBUG("PooledDBConnectionBase::returnToPoolImpl - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("6OOO8USCUNVM",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("PooledDBConnectionBase::returnToPoolImpl - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("C5TI8P7TLY82",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<bool, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::isClosedImpl(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<bool, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::isPooledImpl(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<std::string, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::getURLImpl(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("2I2FYW64DQJV",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::resetImpl(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("W9QZE5CW0T1C",
                                                   "Underlying connection is null",
                                                   system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("GCMQQLPI0A3Y",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<bool, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::pingImpl(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FMPOXQZ16S31",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::prepareForPoolReturnImpl(
        std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("ERB7A9KN4OZ9",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    template <typename Derived, typename ConnType, typename PoolType>
    cpp_dbc::expected<void, DBException>
    PooledDBConnectionBase<Derived, ConnType, PoolType>::prepareForBorrowImpl(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FQYTSNOGDGZE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

    // ── Throwing helpers ──────────────────────────────────────────────────────

#ifdef __cpp_exceptions

    template <typename Derived, typename ConnType, typename PoolType>
    void PooledDBConnectionBase<Derived, ConnType, PoolType>::closeThrow()
    {
        auto result = closeImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::isClosedThrow() const
    {
        auto result = isClosedImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    template <typename Derived, typename ConnType, typename PoolType>
    void PooledDBConnectionBase<Derived, ConnType, PoolType>::returnToPoolThrow()
    {
        auto result = returnToPoolImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::isPooledThrow() const
    {
        auto result = isPooledImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    template <typename Derived, typename ConnType, typename PoolType>
    std::string PooledDBConnectionBase<Derived, ConnType, PoolType>::getURLThrow() const
    {
        auto result = getURLImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    template <typename Derived, typename ConnType, typename PoolType>
    void PooledDBConnectionBase<Derived, ConnType, PoolType>::resetThrow()
    {
        auto result = resetImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    template <typename Derived, typename ConnType, typename PoolType>
    bool PooledDBConnectionBase<Derived, ConnType, PoolType>::pingThrow()
    {
        auto result = pingImpl(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── Explicit instantiations ───────────────────────────────────────────────

    template class PooledDBConnectionBase<RelationalPooledDBConnection, RelationalDBConnection, RelationalDBConnectionPool>;
    template class PooledDBConnectionBase<KVPooledDBConnection, KVDBConnection, KVDBConnectionPool>;
    template class PooledDBConnectionBase<DocumentPooledDBConnection, DocumentDBConnection, DocumentDBConnectionPool>;
    template class PooledDBConnectionBase<ColumnarPooledDBConnection, ColumnarDBConnection, ColumnarDBConnectionPool>;

} // namespace cpp_dbc
