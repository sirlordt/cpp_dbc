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
#include "cpp_dbc/pool/pooled_db_connection_base.hpp"
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
        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> createDBConnection(std::nothrow_t) const noexcept;

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
                                 size_t initialSize = 5,
                                 size_t maxSize = 20,
                                 size_t minIdle = 3,
                                 size_t maxWaitMillis = 5000,
                                 size_t validationTimeoutMillis = 5000,
                                 size_t idleTimeoutMillis = 300000,
                                 size_t maxLifetimeMillis = 1800000,
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
                                                                                                size_t initialSize = 5,
                                                                                                size_t maxSize = 20,
                                                                                                size_t minIdle = 3,
                                                                                                size_t maxWaitMillis = 5000,
                                                                                                size_t validationTimeoutMillis = 5000,
                                                                                                size_t idleTimeoutMillis = 300000,
                                                                                                size_t maxLifetimeMillis = 1800000,
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
     * Inherits common pool logic (close, returnToPool, destructor, pool metadata)
     * from PooledDBConnectionBase via CRTP. Only columnar-specific methods
     * (prepareStatement, executeQuery, executeUpdate, transactions) are here.
     */
    class ColumnarPooledDBConnection final
        : public PooledDBConnectionBase<ColumnarPooledDBConnection, ColumnarDBConnection, ColumnarDBConnectionPool>
        , public ColumnarDBConnection
        , public std::enable_shared_from_this<ColumnarPooledDBConnection>
    {
    private:
        using Base = PooledDBConnectionBase<ColumnarPooledDBConnection, ColumnarDBConnection, ColumnarDBConnectionPool>;
        friend class ColumnarDBConnectionPool;

    protected:
        // Pool lifecycle overrides — delegated to CRTP base Impl methods.
        // These must be overridden here to resolve the diamond
        // (DBConnectionPooled::DBConnection vs ColumnarDBConnection::DBConnection).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
                                                                  TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override
        {
            return this->prepareForPoolReturnImpl(std::nothrow, isolationLevel);
        }
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override
        {
            return this->prepareForBorrowImpl(std::nothrow);
        }

    public:
        ColumnarPooledDBConnection(
            std::shared_ptr<ColumnarDBConnection> connection,
            std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;
        ~ColumnarPooledDBConnection() override = default;

        ColumnarPooledDBConnection(const ColumnarPooledDBConnection &) = delete;
        ColumnarPooledDBConnection &operator=(const ColumnarPooledDBConnection &) = delete;
        ColumnarPooledDBConnection(ColumnarPooledDBConnection &&) = delete;
        ColumnarPooledDBConnection &operator=(ColumnarPooledDBConnection &&) = delete;

#ifdef __cpp_exceptions
        // ── Diamond-resolving throwing delegators (DBConnection methods) ──
        void close() override { this->closeThrow(); }
        bool isClosed() const override { return this->isClosedThrow(); }
        void returnToPool() override { this->returnToPoolThrow(); }
        bool isPooled() const override { return this->isPooledThrow(); }
        std::string getURL() const override { return this->getURLThrow(); }
        void reset() override { this->resetThrow(); }
        bool ping() override { return this->pingThrow(); }

        // ── Columnar-specific throwing methods ──
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

        // ── Diamond-resolving nothrow delegators (DBConnection methods) ──
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override { return this->closeImpl(std::nothrow); }
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override { return this->resetImpl(std::nothrow); }
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override { return this->isClosedImpl(std::nothrow); }
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override { return this->returnToPoolImpl(std::nothrow); }
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override { return this->isPooledImpl(std::nothrow); }
        cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override { return this->getURLImpl(std::nothrow); }
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override { return this->pingImpl(std::nothrow); }

        // ── Columnar-specific nothrow methods ──
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
