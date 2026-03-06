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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file relational_db_connection_pool.hpp
 @brief Connection pool for relational databases — thin wrapper around DBConnectionPoolBase

*/

#ifndef CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP
#define CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/core/relational/relational_db_connection.hpp"
#include "cpp_dbc/core/relational/relational_db_prepared_statement.hpp"
#include "cpp_dbc/core/relational/relational_db_result_set.hpp"

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
    class RelationalPooledDBConnection;

    /**
     * @brief Connection pool for relational databases
     *
     * Thin derived class that overrides only createPooledDBConnection() to produce
     * RelationalPooledDBConnection wrappers, and adds the typed getter
     * getRelationalDBConnection().
     *
     * All pool infrastructure (acquisition, validation, maintenance, direct handoff)
     * is inherited from DBConnectionPoolBase.
     *
     * @see DBConnectionPoolBase, RelationalPooledDBConnection
     */
    class RelationalDBConnectionPool : public DBConnectionPoolBase
    {
    private:
        friend class RelationalPooledDBConnection;

        // Creates a physical relational connection via DriverManager
        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> createDBConnection(std::nothrow_t) noexcept;

        // Override from DBConnectionPoolBase — creates the relational-specific pooled wrapper
        cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
            createPooledDBConnection(std::nothrow_t) noexcept override;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        RelationalDBConnectionPool(DBConnectionPool::ConstructorTag,
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

        explicit RelationalDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~RelationalDBConnectionPool() override;

        RelationalDBConnectionPool(const RelationalDBConnectionPool &) = delete;
        RelationalDBConnectionPool &operator=(const RelationalDBConnectionPool &) = delete;
        RelationalDBConnectionPool(RelationalDBConnectionPool &&) = delete;
        RelationalDBConnectionPool &operator=(RelationalDBConnectionPool &&) = delete;

        // Static factory methods
        static cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> create(std::nothrow_t,
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

        static cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;

#ifdef __cpp_exceptions
        // Family-specific typed getter (throwing)
        virtual std::shared_ptr<RelationalDBConnection> getRelationalDBConnection();
#endif

        // Family-specific typed getter (nothrow)
        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> getRelationalDBConnection(std::nothrow_t) noexcept;
    };

    // RelationalPooledDBConnection wraps a physical relational database connection
    class RelationalPooledDBConnection final : public DBConnectionPooled, public RelationalDBConnection, public std::enable_shared_from_this<RelationalPooledDBConnection>
    {
    private:
        std::shared_ptr<RelationalDBConnection> m_conn;
        std::weak_ptr<RelationalDBConnectionPool> m_pool; // Weak pointer to connection pool
        std::shared_ptr<std::atomic<bool>> m_poolAlive;   // Shared flag to check if pool is still alive
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

        friend class RelationalDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid(std::nothrow_t) const noexcept override;

    protected:
        // Pool lifecycle overrides - only callable by RelationalDBConnectionPool (declared as friend).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
                                                                  TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        RelationalPooledDBConnection(
            std::shared_ptr<RelationalDBConnection> connection,
            std::weak_ptr<RelationalDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive);
        ~RelationalPooledDBConnection() override;

#ifdef __cpp_exceptions
        // DBConnection interface methods
        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;
        bool ping() override;

        // RelationalDBConnection interface methods
        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        void commit() override;
        void rollback() override;

        bool beginTransaction() override;
        bool transactionActive() override;

        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        // DBConnection nothrow interface
        cpp_dbc::expected<void, cpp_dbc::DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, cpp_dbc::DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        // RelationalDBConnection nothrow interface
        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<void, DBException> setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;
        cpp_dbc::expected<bool, DBException> getAutoCommit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> transactionActive(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException> getTransactionIsolation(std::nothrow_t) noexcept override;

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

    // Specialized connection pools for MySQL, PostgreSQL, SQLite, and Firebird
    namespace MySQL
    {
        class MySQLConnectionPool final : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            MySQLConnectionPool(DBConnectionPool::ConstructorTag,
                                const std::string &url,
                                const std::string &username,
                                const std::string &password);

            explicit MySQLConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config);

            static cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> create(std::nothrow_t,
                                                                                               const std::string &url,
                                                                                               const std::string &username,
                                                                                               const std::string &password) noexcept;

            static cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
        };
    }

    namespace PostgreSQL
    {
        class PostgreSQLConnectionPool final : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            PostgreSQLConnectionPool(DBConnectionPool::ConstructorTag,
                                     const std::string &url,
                                     const std::string &username,
                                     const std::string &password);

            explicit PostgreSQLConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config);

            static cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> create(std::nothrow_t,
                                                                                                    const std::string &url,
                                                                                                    const std::string &username,
                                                                                                    const std::string &password) noexcept;

            static cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
        };
    }

    namespace SQLite
    {
        class SQLiteConnectionPool final : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            SQLiteConnectionPool(DBConnectionPool::ConstructorTag,
                                 const std::string &url,
                                 const std::string &username,
                                 const std::string &password);

            explicit SQLiteConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config);

            static cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> create(std::nothrow_t,
                                                                                                const std::string &url,
                                                                                                const std::string &username,
                                                                                                const std::string &password) noexcept;

            static cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
        };
    }

    namespace Firebird
    {
        class FirebirdConnectionPool final : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            FirebirdConnectionPool(DBConnectionPool::ConstructorTag,
                                   const std::string &url,
                                   const std::string &username,
                                   const std::string &password);

            explicit FirebirdConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config);

            static cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> create(std::nothrow_t,
                                                                                                  const std::string &url,
                                                                                                  const std::string &username,
                                                                                                  const std::string &password) noexcept;

            static cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP
