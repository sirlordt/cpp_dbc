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
 * @file columnar_db_connection_pool.hpp
 * @brief Connection pool for columnar databases — thin wrapper around DBConnectionPoolBase
 *
 * All pool infrastructure (connection lifecycle, maintenance thread, direct handoff,
 * HikariCP validation skip, phase-based lock protocol) is inherited from
 * DBConnectionPoolBase. This class only provides:
 * - createPooledDBConnection() override (creates ColumnarPooledDBConnection wrappers)
 * - getColumnarDBConnection() typed getter
 * - Factory methods
 */

#ifndef CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP
#define CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/columnar/columnar_db_prepared_statement.hpp"
#include "cpp_dbc/core/columnar/columnar_db_result_set.hpp"

#include <chrono>
#include <atomic>

// Forward declaration of configuration classes
namespace cpp_dbc::config
{
    class DatabaseConfig;
    class DBConnectionPoolConfig;
} // namespace cpp_dbc::config

namespace cpp_dbc
{

    // Forward declaration
    class ColumnarPooledDBConnection;

    /**
     * @brief Connection pool for columnar databases
     *
     * Thin derived class that overrides only createPooledDBConnection() to produce
     * ColumnarPooledDBConnection wrappers, and adds the typed getter
     * getColumnarDBConnection().
     *
     * All pool infrastructure (acquisition, validation, maintenance, direct handoff)
     * is inherited from DBConnectionPoolBase.
     *
     * @see DBConnectionPoolBase, ColumnarPooledDBConnection
     */
    class ColumnarDBConnectionPool : public DBConnectionPoolBase
    {
    private:
        friend class ColumnarPooledDBConnection;

        // Creates a physical columnar connection via DriverManager
        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> createDBConnection(std::nothrow_t) noexcept;

        // Override from DBConnectionPoolBase — creates the columnar-specific pooled wrapper
        cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
            createPooledDBConnection(std::nothrow_t) noexcept override;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag,
                                 const std::string &url,
                                 const std::string &username,
                                 const std::string &password,
                                 const std::map<std::string, std::string> &options = std::map<std::string, std::string>(),
                                 int initialSize = 5,
                                 int maxSize = 20,
                                 int minIdle = 3,
                                 long maxWaitMillis = 5000,
                                 long validationTimeoutMillis = 5000,
                                 long idleTimeoutMillis = 300000,
                                 long maxLifetimeMillis = 1800000,
                                 bool testOnBorrow = true,
                                 bool testOnReturn = false,
                                 TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) noexcept;

        explicit ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~ColumnarDBConnectionPool() override;

        ColumnarDBConnectionPool(const ColumnarDBConnectionPool &) = delete;
        ColumnarDBConnectionPool &operator=(const ColumnarDBConnectionPool &) = delete;
        ColumnarDBConnectionPool(ColumnarDBConnectionPool &&) = delete;
        ColumnarDBConnectionPool &operator=(ColumnarDBConnectionPool &&) = delete;

        // Static factory methods
        static cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> create(std::nothrow_t,
                                                                                                const std::string &url,
                                                                                                const std::string &username,
                                                                                                const std::string &password,
                                                                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>(),
                                                                                                int initialSize = 5,
                                                                                                int maxSize = 20,
                                                                                                int minIdle = 3,
                                                                                                long maxWaitMillis = 5000,
                                                                                                long validationTimeoutMillis = 5000,
                                                                                                long idleTimeoutMillis = 300000,
                                                                                                long maxLifetimeMillis = 1800000,
                                                                                                bool testOnBorrow = true,
                                                                                                bool testOnReturn = false,
                                                                                                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) noexcept;

        static cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;

#ifdef __cpp_exceptions
        // Family-specific typed getter (throwing)
        virtual std::shared_ptr<ColumnarDBConnection> getColumnarDBConnection();
#endif

        // Family-specific typed getter (nothrow)
        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> getColumnarDBConnection(std::nothrow_t) noexcept;
    };

    /**
     * @brief Pooled connection implementation for columnar databases
     *
     * This class wraps a physical columnar database connection and provides
     * pooling functionality.
     */
    class ColumnarPooledDBConnection final : public DBConnectionPooled, public ColumnarDBConnection, public std::enable_shared_from_this<ColumnarPooledDBConnection>
    {
    private:
        std::shared_ptr<ColumnarDBConnection> m_conn;
        std::weak_ptr<ColumnarDBConnectionPool> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive; // Shared flag to check if pool is still alive
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

        friend class ColumnarDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid(std::nothrow_t) const noexcept override;

    protected:
        // Pool lifecycle overrides - only callable by ColumnarDBConnectionPool (declared as friend).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
                                                                  TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        ColumnarPooledDBConnection(
            std::shared_ptr<ColumnarDBConnection> connection,
            std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;
        ~ColumnarPooledDBConnection() override;

        ColumnarPooledDBConnection(const ColumnarPooledDBConnection &) = delete;
        ColumnarPooledDBConnection &operator=(const ColumnarPooledDBConnection &) = delete;
        ColumnarPooledDBConnection(ColumnarPooledDBConnection &&) = delete;
        ColumnarPooledDBConnection &operator=(ColumnarPooledDBConnection &&) = delete;

#ifdef __cpp_exceptions
        // Overridden DBConnection interface methods
        void close() final;
        bool isClosed() const override;
        void returnToPool() final;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;
        bool ping() override;

        // Overridden ColumnarDBConnection interface methods
        std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) override;
        std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) override;
        uint64_t executeUpdate(const std::string &query) override;

        bool beginTransaction() override;
        void commit() override;
        void rollback() override;
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        // DBConnection nothrow interface
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        // ColumnarDBConnection nothrow interface
        cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime(std::nothrow_t) const noexcept override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> setActive(std::nothrow_t, bool active) noexcept override;
        bool isActive(std::nothrow_t) const noexcept override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection(std::nothrow_t) noexcept override;
        void markPoolClosed(std::nothrow_t, bool closed) noexcept override;
        bool isPoolClosed(std::nothrow_t) const noexcept override;
        void updateLastUsedTime(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc

// Specialized connection pool for ScyllaDB
namespace cpp_dbc::ScyllaDB
{
    /**
     * @brief ScyllaDB-specific connection pool implementation
     */
    class ScyllaDBConnectionPool final : public ColumnarDBConnectionPool
    {
    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag,
                               const std::string &url,
                               const std::string &username,
                               const std::string &password) noexcept;

        explicit ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~ScyllaDBConnectionPool() override = default;

        ScyllaDBConnectionPool(const ScyllaDBConnectionPool &) = delete;
        ScyllaDBConnectionPool &operator=(const ScyllaDBConnectionPool &) = delete;
        ScyllaDBConnectionPool(ScyllaDBConnectionPool &&) = delete;
        ScyllaDBConnectionPool &operator=(ScyllaDBConnectionPool &&) = delete;

#ifdef __cpp_exceptions
        // Throwing static factory methods
        static std::shared_ptr<ScyllaDBConnectionPool> create(const std::string &url,
                                                              const std::string &username,
                                                              const std::string &password);

        static std::shared_ptr<ScyllaDBConnectionPool> create(const config::DBConnectionPoolConfig &config);
#endif // __cpp_exceptions

        // Nothrow static factory methods
        static cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> create(std::nothrow_t,
                                                                                              const std::string &url,
                                                                                              const std::string &username,
                                                                                              const std::string &password) noexcept;

        static cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
    };
} // namespace cpp_dbc::ScyllaDB

#endif // CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP
