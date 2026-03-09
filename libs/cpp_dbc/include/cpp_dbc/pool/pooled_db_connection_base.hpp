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
 * @file pooled_db_connection_base.hpp
 * @brief CRTP base class for all family-specific pooled connection wrappers
 *
 * Extracts the common pool-management logic (close/returnToPool with race-condition
 * fix, destructor cleanup, pool metadata accessors) shared by all four
 * PooledDBConnection implementations (Relational, KV, Document, Columnar).
 *
 * Because DBConnection does NOT use virtual inheritance, each PooledDBConnection
 * has TWO DBConnection subobjects (one via DBConnectionPooled, one via the family
 * connection class). The CRTP base overrides methods from the DBConnectionPooled
 * path directly, and provides *Impl helper methods for methods that need a
 * final-class override to resolve the diamond (close, isClosed, returnToPool,
 * isPooled, getURL, reset, ping, prepareForPoolReturn, prepareForBorrow).
 *
 * Derived classes provide one-line delegators that forward to the Impl methods.
 *
 * Template parameters:
 *   Derived  — the final pooled connection class (e.g. ColumnarPooledDBConnection)
 *   ConnType — the family connection type (e.g. ColumnarDBConnection)
 *   PoolType — the family pool type (e.g. ColumnarDBConnectionPool)
 *
 * Implementations are in pooled_db_connection_base.cpp with explicit instantiations
 * for all four families to avoid header bloat.
 */

#ifndef CPP_DBC_POOL_POOLED_DB_CONNECTION_BASE_HPP
#define CPP_DBC_POOL_POOLED_DB_CONNECTION_BASE_HPP

#include "cpp_dbc/pool/db_connection_pooled.hpp"
#include <atomic>
#include <map>
#include <string>

namespace cpp_dbc
{

    // Forward declaration — needed for friend access to returnConnection/decrementActiveCount
    class DBConnectionPoolBase;

    template <typename Derived, typename ConnType, typename PoolType>
    class PooledDBConnectionBase : public DBConnectionPooled
    {
        std::shared_ptr<ConnType> m_conn;
        std::weak_ptr<PoolType> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive;
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
        // Store last-used time as nanoseconds since epoch in an atomic int64_t.
        // std::atomic<int64_t> is lock-free on every supported 64-bit platform,
        // unlike std::atomic<time_point> which is not portable to ARM32/MIPS.
        // See: libs/cpp_dbc/docs/bugs/firebird_helgrind_analysis.md (Context 1)
        static_assert(std::atomic<int64_t>::is_always_lock_free,
                      "int64_t atomic must be lock-free on this platform");
        std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()};
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

    protected:
        // ── Protected accessors for derived classes ──────────────────────────────
        const std::shared_ptr<ConnType> &getConn(std::nothrow_t) const noexcept { return m_conn; }
        bool isLocalClosed(std::nothrow_t) const noexcept { return m_closed.load(std::memory_order_acquire); }

        // Protected constructor — only callable from derived classes
        PooledDBConnectionBase(
            std::shared_ptr<ConnType> connection,
            std::weak_ptr<PoolType> pool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;

        // ── DBConnection method implementations ────────────────────────────────
        // Called by derived class one-line delegators to resolve the diamond
        // between DBConnectionPooled::DBConnection and FamilyDBConnection::DBConnection.

        cpp_dbc::expected<void, DBException> closeImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> returnToPoolImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<bool, DBException> isClosedImpl(std::nothrow_t) const noexcept;
        cpp_dbc::expected<bool, DBException> isPooledImpl(std::nothrow_t) const noexcept;
        cpp_dbc::expected<std::string, DBException> getURLImpl(std::nothrow_t) const noexcept;
        cpp_dbc::expected<void, DBException> resetImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<bool, DBException> pingImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> prepareForPoolReturnImpl(
            std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept;
        cpp_dbc::expected<void, DBException> prepareForBorrowImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<std::string, DBException> getServerVersionImpl(std::nothrow_t) noexcept;
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfoImpl(std::nothrow_t) noexcept;

#ifdef __cpp_exceptions
        // Throwing helper implementations — called by derived throwing delegators
        void closeThrow();
        bool isClosedThrow() const;
        void returnToPoolThrow();
        bool isPooledThrow() const;
        std::string getURLThrow() const;
        void resetThrow();
        bool pingThrow();
        std::string getServerVersionThrow();
        std::map<std::string, std::string> getServerInfoThrow();
#endif

    public:
        ~PooledDBConnectionBase() override;

        // ── DBConnectionPooled interface overrides ─────────────────────────────
        // These have no diamond problem — they exist only in DBConnectionPooled,
        // not in the family connection class.
        bool isPoolValid(std::nothrow_t) const noexcept override;
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime(std::nothrow_t) const noexcept override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> setActive(std::nothrow_t, bool active) noexcept override;
        bool isActive(std::nothrow_t) const noexcept override;
        std::shared_ptr<DBConnection> getUnderlyingConnection(std::nothrow_t) noexcept override;
        void markPoolClosed(std::nothrow_t, bool closed) noexcept override;
        bool isPoolClosed(std::nothrow_t) const noexcept override;
        void updateLastUsedTime(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_POOL_POOLED_DB_CONNECTION_BASE_HPP
