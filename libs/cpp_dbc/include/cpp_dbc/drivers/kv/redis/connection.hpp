#pragma once

#include "reply_handle.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>

// Forward declaration for hiredis
struct redisContext;

namespace cpp_dbc::Redis
{
    /**
     * @brief Redis connection implementation.
     *
     * This class implements the KVDBConnection interface for Redis,
     * providing key-value, list, hash, set, and sorted-set operations
     * as well as server administration commands.
     * Each throwing method has a corresponding `std::nothrow_t` overload
     * that returns `cpp_dbc::expected` instead of throwing.
     *
     * ### Example
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::Redis::RedisDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("cpp_dbc:redis://localhost:6379", "", ""));
     * conn->setString("key", "value");
     * std::string val = conn->getString("key");
     * conn->close();
     * ```
     */
    class RedisDBConnection final : public KVDBConnection, public std::enable_shared_from_this<RedisDBConnection>
    {
    private:
        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::shared_ptr<redisContext> m_context;
        std::string m_uri;
        int m_dbIndex{0};
        std::atomic<bool> m_closed{false};
        mutable std::mutex m_mutex;
        bool m_initFailed{false};
        std::optional<DBException> m_initError{std::nullopt};
        bool m_counterIncremented{false};
        TransactionIsolationLevel m_transactionIsolation{TransactionIsolationLevel::TRANSACTION_NONE};

        /**
         * @brief Check if the connection is valid
         *
         * @return unexpected(DBException) if the connection is closed or invalid
         */
        cpp_dbc::expected<void, DBException> validateConnection(std::nothrow_t) const noexcept;

        /**
         * @brief Extract string value from Redis reply
         *
         * @param reply The Redis reply
         * @return expected containing the string value, or DBException on failure
         */
        cpp_dbc::expected<std::string, DBException> extractString(std::nothrow_t, const RedisReplyHandle &reply) const noexcept;

        /**
         * @brief Extract integer value from Redis reply
         *
         * @param reply The Redis reply
         * @return expected containing the integer value, or DBException on failure
         */
        cpp_dbc::expected<int64_t, DBException> extractInteger(std::nothrow_t, const RedisReplyHandle &reply) const noexcept;

        /**
         * @brief Extract string array from Redis reply
         *
         * @param reply The Redis reply
         * @return expected containing the string array, or DBException on failure
         */
        cpp_dbc::expected<std::vector<std::string>, DBException> extractArray(std::nothrow_t, const RedisReplyHandle &reply) const noexcept;

        /**
         * @brief Try to parse a double from a string
         *
         * @param str The string to parse
         * @return std::optional<double> The parsed double, or nullopt on failure
         */
        static std::optional<double> tryParseDouble(const std::string &str) noexcept;

    public:
        /**
         * @brief Nothrow constructor — performs all connection setup without throwing.
         *
         * On failure, sets m_initFailed = true and stores the error in m_initError.
         * Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        RedisDBConnection(
            PrivateCtorTag,
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options) noexcept;

        /**
         * @brief Destructor
         */
        ~RedisDBConnection() override;

        // Delete copy operations
        RedisDBConnection(const RedisDBConnection &) = delete;
        RedisDBConnection &operator=(const RedisDBConnection &) = delete;

        // Delete move operations — this class is always managed via shared_ptr.
        // Moving is unsafe because: (1) std::mutex is not movable, (2) the
        // enable_shared_from_this weak_ptr is not transferred to the new object,
        // and (3) the shared_ptr wrapper is what callers should move, not the connection itself.
        RedisDBConnection(RedisDBConnection &&) = delete;
        RedisDBConnection &operator=(RedisDBConnection &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<RedisDBConnection> create(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto r = create(std::nothrow, uri, user, password, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        // DBConnection interface implementation
        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURI() const override;
        void reset() override;

        // KVDBConnection interface implementation - Basic key-value operations
        bool setString(const std::string &key, const std::string &value,
                       std::optional<int64_t> expirySeconds = std::nullopt) override;
        std::string getString(const std::string &key) override;
        bool exists(const std::string &key) override;
        bool deleteKey(const std::string &key) override;
        int64_t deleteKeys(const std::vector<std::string> &keys) override;
        bool expire(const std::string &key, int64_t seconds) override;
        int64_t getTTL(const std::string &key) override;

        // Counter operations
        int64_t increment(const std::string &key, int64_t by = 1) override;
        int64_t decrement(const std::string &key, int64_t by = 1) override;

        // List operations
        int64_t listPushLeft(const std::string &key, const std::string &value) override;
        int64_t listPushRight(const std::string &key, const std::string &value) override;
        std::string listPopLeft(const std::string &key) override;
        std::string listPopRight(const std::string &key) override;
        std::vector<std::string> listRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t listLength(const std::string &key) override;

        // Hash operations
        bool hashSet(const std::string &key, const std::string &field, const std::string &value) override;
        std::string hashGet(const std::string &key, const std::string &field) override;
        bool hashDelete(const std::string &key, const std::string &field) override;
        bool hashExists(const std::string &key, const std::string &field) override;
        std::map<std::string, std::string> hashGetAll(const std::string &key) override;
        int64_t hashLength(const std::string &key) override;

        // Set operations
        bool setAdd(const std::string &key, const std::string &member) override;
        bool setRemove(const std::string &key, const std::string &member) override;
        bool setIsMember(const std::string &key, const std::string &member) override;
        std::vector<std::string> setMembers(const std::string &key) override;
        int64_t setSize(const std::string &key) override;

        // Sorted set operations
        bool sortedSetAdd(const std::string &key, double score, const std::string &member) override;
        bool sortedSetRemove(const std::string &key, const std::string &member) override;
        std::optional<double> sortedSetScore(const std::string &key, const std::string &member) override;
        std::vector<std::string> sortedSetRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t sortedSetSize(const std::string &key) override;

        // Key scan operations
        std::vector<std::string> scanKeys(const std::string &pattern, int64_t count = 10) override;

        // Server operations
        std::string executeCommand(const std::string &command,
                                   const std::vector<std::string> &args = {}) override;
        bool flushDB(bool async = false) override;
        bool ping() override;
        std::string getServerVersion() override;
        std::map<std::string, std::string> getServerInfo() override;
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        /**
         * @brief Execute a Redis command and return the raw reply
         *
         * This is a low-level method that allows executing arbitrary Redis commands.
         *
         * @param command The Redis command
         * @param args The command arguments
         * @return RedisReplyHandle The Redis reply
         * @throws DBException if the command fails
         */
        RedisReplyHandle executeRaw(const std::string &command, const std::vector<std::string> &args = {});

        /**
         * @brief Select a Redis database
         *
         * @param index The database index
         * @throws DBException if the selection fails
         */
        void selectDatabase(int index);

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<RedisDBConnection>, DBException> create(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // The nothrow constructor stores init errors in m_initFailed/m_initError
            // rather than throwing, so no try/catch is needed here.
            auto conn = std::make_shared<RedisDBConnection>(
                PrivateCtorTag{}, std::nothrow, uri, user, password, options);
            if (conn->m_initFailed)
            {
                return cpp_dbc::unexpected(*conn->m_initError);
            }
            return conn;
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURI(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException>
            setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept override;

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

        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        cpp_dbc::expected<std::string, DBException> getServerVersion(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(
            std::nothrow_t) noexcept override;

        /**
         * @brief Get the Redis database index
         *
         * @return int The database index
         */
        int getDatabaseIndex(std::nothrow_t) const noexcept;

        /**
         * @brief Execute a Redis command and return the raw reply (nothrow version)
         *
         * @param command The Redis command
         * @param args The command arguments
         * @return expected containing the RedisReplyHandle, or DBException on failure
         */
        cpp_dbc::expected<RedisReplyHandle, DBException> executeRaw(
            std::nothrow_t, const std::string &command, const std::vector<std::string> &args = {}) noexcept;

        /**
         * @brief Select a Redis database (nothrow version)
         *
         * @param index The database index
         * @return expected containing void on success, or DBException on failure
         */
        cpp_dbc::expected<void, DBException> selectDatabase(std::nothrow_t, int index) noexcept;

    protected:
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::Redis
