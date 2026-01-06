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
 * @file driver_redis.hpp
 * @brief Redis database driver
 */

#ifndef CPP_DBC_DRIVER_REDIS_HPP
#define CPP_DBC_DRIVER_REDIS_HPP

#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"

// Forward declarations for hiredis
struct redisContext;
struct redisReply;

namespace cpp_dbc
{
    namespace Redis
    {
        /**
         * @brief Custom deleter for redisReply
         */
        struct RedisReplyDeleter
        {
            void operator()(redisReply *reply);
        };

        /**
         * @brief RAII wrapper for redisReply
         *
         * This class ensures that redisReply is freed when it goes out of scope.
         */
        class RedisReplyHandle
        {
        public:
            RedisReplyHandle(redisReply *reply);
            ~RedisReplyHandle();

            // Disable copy operations
            RedisReplyHandle(const RedisReplyHandle &) = delete;
            RedisReplyHandle &operator=(const RedisReplyHandle &) = delete;

            // Enable move operations
            RedisReplyHandle(RedisReplyHandle &&other) noexcept;
            RedisReplyHandle &operator=(RedisReplyHandle &&other) noexcept;

            redisReply *get() const;

        private:
            std::unique_ptr<redisReply, RedisReplyDeleter> m_reply;
        };

        /**
         * @brief Custom deleter for redisContext
         */
        struct RedisContextDeleter
        {
            void operator()(redisContext *context);
        };

        /**
         * @brief Redis connection implementation
         *
         * This class implements the KVDBConnection interface for Redis.
         */
        class RedisConnection : public KVDBConnection, public std::enable_shared_from_this<RedisConnection>
        {
        public:
            /**
             * @brief Construct a new Redis Connection
             *
             * @param uri The Redis URI (redis://host:port)
             * @param user The username for authentication
             * @param password The password for authentication
             * @param options Additional connection options
             */
            RedisConnection(
                const std::string &uri,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

            /**
             * @brief Destructor
             */
            ~RedisConnection();

            // Delete copy operations
            RedisConnection(const RedisConnection &) = delete;
            RedisConnection &operator=(const RedisConnection &) = delete;

            // Enable move operations
            RedisConnection(RedisConnection &&other) noexcept;
            RedisConnection &operator=(RedisConnection &&other) noexcept;

            // DBConnection interface implementation
            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;
            std::string getURL() const override;

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
            std::string ping() override;
            std::map<std::string, std::string> getServerInfo() override;

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
             * @brief Get the Redis database index
             *
             * @return int The database index
             */
            int getDatabaseIndex() const;

            /**
             * @brief Select a Redis database
             *
             * @param index The database index
             * @throws DBException if the selection fails
             */
            void selectDatabase(int index);

            /**
             * @brief Set whether the connection is pooled
             *
             * @param pooled Whether the connection is managed by a connection pool
             */
            void setPooled(bool pooled);

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

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

            cpp_dbc::expected<std::string, DBException> ping(std::nothrow_t) noexcept override;

            cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(
                std::nothrow_t) noexcept override;

        private:
            /**
             * @brief Check if the connection is valid
             *
             * @throws DBException if the connection is closed or invalid
             */
            void validateConnection() const;

            /**
             * @brief Extract string value from Redis reply
             *
             * @param reply The Redis reply
             * @return std::string The string value
             */
            std::string extractString(const RedisReplyHandle &reply);

            /**
             * @brief Extract integer value from Redis reply
             *
             * @param reply The Redis reply
             * @return int64_t The integer value
             */
            int64_t extractInteger(const RedisReplyHandle &reply);

            /**
             * @brief Extract string array from Redis reply
             *
             * @param reply The Redis reply
             * @return std::vector<std::string> The string array
             */
            std::vector<std::string> extractArray(const RedisReplyHandle &reply);

            std::shared_ptr<redisContext> m_context;
            std::string m_url;
            int m_dbIndex;
            std::atomic<bool> m_closed{false};
            bool m_pooled{false};
            mutable std::mutex m_mutex;
        };

        /**
         * @brief Redis driver implementation
         *
         * This class implements the KVDBDriver interface for Redis.
         */
        class RedisDriver : public KVDBDriver
        {
        public:
            /**
             * @brief Construct a new Redis Driver
             */
            RedisDriver();

            /**
             * @brief Destructor
             */
            ~RedisDriver();

            // DBDriver interface implementation
            bool acceptsURL(const std::string &url) override;

            // KVDBDriver interface implementation
            std::shared_ptr<KVDBConnection> connectKV(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            int getDefaultPort() const override;
            std::string getURIScheme() const override;
            std::map<std::string, std::string> parseURI(const std::string &uri) override;
            std::string buildURI(
                const std::string &host,
                int port,
                const std::string &db = "",
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool supportsClustering() const override;
            bool supportsReplication() const override;
            std::string getDriverVersion() const override;

            /**
             * @brief Clean up Redis driver resources
             */
            static void cleanup();

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
                std::nothrow_t, const std::string &uri) noexcept override;

            std::string getName() const noexcept override;

        private:
            static std::once_flag s_initFlag;
            static std::atomic<bool> s_initialized;

            /**
             * @brief Initialize Redis driver
             */
            static void initialize();

            std::mutex m_mutex;
        };

    } // namespace Redis
} // namespace cpp_dbc

#endif // CPP_DBC_DRIVER_REDIS_HPP
