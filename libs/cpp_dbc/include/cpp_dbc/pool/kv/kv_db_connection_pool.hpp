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
 * @file kv_db_connection_pool.hpp
 * @brief Connection pool for key-value databases — thin wrapper around DBConnectionPoolBase
 *
 * All pool infrastructure (connection lifecycle, maintenance thread, direct handoff,
 * HikariCP validation skip, phase-based lock protocol) is inherited from
 * DBConnectionPoolBase. This class only provides:
 * - createPooledDBConnection() override (creates KVPooledDBConnection wrappers)
 * - getKVDBConnection() typed getter
 * - Factory methods
 */

#ifndef CPP_DBC_KV_DB_CONNECTION_POOL_HPP
#define CPP_DBC_KV_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"

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
    class KVPooledDBConnection;

    /**
     * @brief Connection pool for key-value databases
     *
     * Thin derived class that overrides only createPooledDBConnection() to produce
     * KVPooledDBConnection wrappers, and adds the typed getter
     * getKVDBConnection().
     *
     * All pool infrastructure (acquisition, validation, maintenance, direct handoff)
     * is inherited from DBConnectionPoolBase.
     *
     * @see DBConnectionPoolBase, KVPooledDBConnection
     */
    class KVDBConnectionPool : public DBConnectionPoolBase
    {
    private:
        friend class KVPooledDBConnection;

        // Creates a physical KV connection via DriverManager
        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> createDBConnection(std::nothrow_t) noexcept;

        // Override from DBConnectionPoolBase — creates the KV-specific pooled wrapper
        cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
            createPooledDBConnection(std::nothrow_t) noexcept override;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        KVDBConnectionPool(DBConnectionPool::ConstructorTag,
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

        explicit KVDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~KVDBConnectionPool() override;

        KVDBConnectionPool(const KVDBConnectionPool &) = delete;
        KVDBConnectionPool &operator=(const KVDBConnectionPool &) = delete;
        KVDBConnectionPool(KVDBConnectionPool &&) = delete;
        KVDBConnectionPool &operator=(KVDBConnectionPool &&) = delete;

        // Static factory methods
        static cpp_dbc::expected<std::shared_ptr<KVDBConnectionPool>, DBException> create(std::nothrow_t,
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

        static cpp_dbc::expected<std::shared_ptr<KVDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;

#ifdef __cpp_exceptions
        // Family-specific typed getter (throwing)
        virtual std::shared_ptr<KVDBConnection> getKVDBConnection();
#endif

        // Family-specific typed getter (nothrow)
        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> getKVDBConnection(std::nothrow_t) noexcept;
    };

    /**
     * @brief Pooled connection implementation for key-value databases
     *
     * This class wraps a physical key-value database connection and provides
     * pooling functionality.
     */
    class KVPooledDBConnection : public DBConnectionPooled, public KVDBConnection, public std::enable_shared_from_this<KVPooledDBConnection>
    {
    private:
        std::shared_ptr<KVDBConnection> m_conn;
        std::weak_ptr<KVDBConnectionPool> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive; // Shared flag to check if pool is still alive
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
        // Store last-used time as nanoseconds since epoch in an atomic int64_t.
        // std::atomic<int64_t> is lock-free on every supported 64-bit platform,
        // unlike std::atomic<time_point> which is not portable to ARM32/MIPS.
        static_assert(std::atomic<int64_t>::is_always_lock_free,
                      "int64_t atomic must be lock-free on this platform");
        std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()};
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class KVDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid(std::nothrow_t) const noexcept override;

    protected:
        // Pool lifecycle overrides - only callable by KVDBConnectionPool (declared as friend).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
                                                                  TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        KVPooledDBConnection(
            std::shared_ptr<KVDBConnection> connection,
            std::weak_ptr<KVDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;
        ~KVPooledDBConnection() override;

        KVPooledDBConnection(const KVPooledDBConnection &) = delete;
        KVPooledDBConnection &operator=(const KVPooledDBConnection &) = delete;
        KVPooledDBConnection(KVPooledDBConnection &&) = delete;
        KVPooledDBConnection &operator=(KVPooledDBConnection &&) = delete;

#ifdef __cpp_exceptions
        // Overridden DBConnection interface methods
        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;

        // KVDBConnection interface delegated methods
        bool setString(const std::string &key, const std::string &value,
                       std::optional<int64_t> expirySeconds = std::nullopt) override;
        std::string getString(const std::string &key) override;
        bool exists(const std::string &key) override;
        bool deleteKey(const std::string &key) override;
        int64_t deleteKeys(const std::vector<std::string> &keys) override;
        bool expire(const std::string &key, int64_t seconds) override;
        int64_t getTTL(const std::string &key) override;
        int64_t increment(const std::string &key, int64_t by = 1) override;
        int64_t decrement(const std::string &key, int64_t by = 1) override;
        int64_t listPushLeft(const std::string &key, const std::string &value) override;
        int64_t listPushRight(const std::string &key, const std::string &value) override;
        std::string listPopLeft(const std::string &key) override;
        std::string listPopRight(const std::string &key) override;
        std::vector<std::string> listRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t listLength(const std::string &key) override;
        bool hashSet(const std::string &key, const std::string &field, const std::string &value) override;
        std::string hashGet(const std::string &key, const std::string &field) override;
        bool hashDelete(const std::string &key, const std::string &field) override;
        bool hashExists(const std::string &key, const std::string &field) override;
        std::map<std::string, std::string> hashGetAll(const std::string &key) override;
        int64_t hashLength(const std::string &key) override;
        bool setAdd(const std::string &key, const std::string &member) override;
        bool setRemove(const std::string &key, const std::string &member) override;
        bool setIsMember(const std::string &key, const std::string &member) override;
        std::vector<std::string> setMembers(const std::string &key) override;
        int64_t setSize(const std::string &key) override;
        bool sortedSetAdd(const std::string &key, double score, const std::string &member) override;
        bool sortedSetRemove(const std::string &key, const std::string &member) override;
        std::optional<double> sortedSetScore(const std::string &key, const std::string &member) override;
        std::vector<std::string> sortedSetRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t sortedSetSize(const std::string &key) override;
        std::vector<std::string> scanKeys(const std::string &pattern, int64_t count = 10) override;
        std::string executeCommand(const std::string &command, const std::vector<std::string> &args = {}) override;
        bool flushDB(bool async = false) override;
        bool ping() override;
        std::map<std::string, std::string> getServerInfo() override;
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API (delegated to underlying connection)
        // ====================================================================

        // DBConnection nothrow interface
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        // KVDBConnection nothrow interface
        cpp_dbc::expected<bool, DBException> setString(
            std::nothrow_t,
            const std::string &key,
            const std::string &value,
            std::optional<int64_t> expirySeconds = std::nullopt) noexcept override;

        cpp_dbc::expected<std::string, DBException> getString(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> exists(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> deleteKey(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> deleteKeys(
            std::nothrow_t, const std::vector<std::string> &keys) noexcept override;

        cpp_dbc::expected<bool, DBException> expire(
            std::nothrow_t, const std::string &key, int64_t seconds) noexcept override;

        cpp_dbc::expected<int64_t, DBException> getTTL(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> increment(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept override;

        cpp_dbc::expected<int64_t, DBException> decrement(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listPushLeft(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listPushRight(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept override;

        cpp_dbc::expected<std::string, DBException> listPopLeft(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::string, DBException> listPopRight(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> listRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listLength(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> hashSet(
            std::nothrow_t,
            const std::string &key,
            const std::string &field,
            const std::string &value) noexcept override;

        cpp_dbc::expected<std::string, DBException> hashGet(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<bool, DBException> hashDelete(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<bool, DBException> hashExists(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> hashGetAll(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> hashLength(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> setAdd(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> setRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> setIsMember(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> setMembers(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> setSize(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> sortedSetAdd(
            std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> sortedSetRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::optional<double>, DBException> sortedSetScore(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> sortedSetRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept override;

        cpp_dbc::expected<int64_t, DBException> sortedSetSize(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> scanKeys(
            std::nothrow_t, const std::string &pattern, int64_t count = 10) noexcept override;

        cpp_dbc::expected<std::string, DBException> executeCommand(
            std::nothrow_t,
            const std::string &command,
            const std::vector<std::string> &args = {}) noexcept override;

        cpp_dbc::expected<bool, DBException> flushDB(
            std::nothrow_t, bool async = false) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(
            std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime(std::nothrow_t) const noexcept override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> setActive(std::nothrow_t, bool isActive) noexcept override;
        bool isActive(std::nothrow_t) const noexcept override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection(std::nothrow_t) noexcept override;
        void markPoolClosed(std::nothrow_t, bool closed) noexcept override;
        bool isPoolClosed(std::nothrow_t) const noexcept override;
        void updateLastUsedTime(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc

// Specialized connection pool for Redis
namespace cpp_dbc::Redis
{
    /**
     * @brief Redis-specific connection pool implementation
     */
    class RedisDBConnectionPool final : public KVDBConnectionPool
    {
    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        RedisDBConnectionPool(DBConnectionPool::ConstructorTag,
                              const std::string &url,
                              const std::string &username,
                              const std::string &password) noexcept;

        explicit RedisDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~RedisDBConnectionPool() override = default;

        RedisDBConnectionPool(const RedisDBConnectionPool &) = delete;
        RedisDBConnectionPool &operator=(const RedisDBConnectionPool &) = delete;
        RedisDBConnectionPool(RedisDBConnectionPool &&) = delete;
        RedisDBConnectionPool &operator=(RedisDBConnectionPool &&) = delete;

#ifdef __cpp_exceptions
        // Throwing static factory methods
        static std::shared_ptr<RedisDBConnectionPool> create(const std::string &url,
                                                             const std::string &username,
                                                             const std::string &password);

        static std::shared_ptr<RedisDBConnectionPool> create(const config::DBConnectionPoolConfig &config);
#endif // __cpp_exceptions

        // Nothrow static factory methods
        static cpp_dbc::expected<std::shared_ptr<RedisDBConnectionPool>, DBException> create(std::nothrow_t,
                                                                                             const std::string &url,
                                                                                             const std::string &username,
                                                                                             const std::string &password) noexcept;

        static cpp_dbc::expected<std::shared_ptr<RedisDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
    };
} // namespace cpp_dbc::Redis

#endif // CPP_DBC_KV_DB_CONNECTION_POOL_HPP
