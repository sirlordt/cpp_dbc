#pragma once

#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"

#if USE_REDIS

#include <mutex>
#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <map>

namespace cpp_dbc::Redis
{
    class RedisDBConnection; // forward declaration for registry
    /**
     * @brief Redis driver implementation.
     *
     * This class implements the KVDBDriver interface for Redis,
     * providing connection management, URI parsing, and driver
     * registration with the DriverManager.
     *
     * ### Example
     *
     * ```cpp
     * #include "cpp_dbc/drivers/kv/driver_redis.hpp"
     * #include "cpp_dbc/core/driver_manager.hpp"
     *
     * auto driver = cpp_dbc::Redis::RedisDBDriver::getInstance();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectKV("cpp_dbc:redis://localhost:6379", "", "");
     * auto reply = conn->ping();
     * conn->close();
     * ```
     */
    class RedisDBDriver final : public KVDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // Note: Using atomic<bool> + mutex instead of std::once_flag because
        // std::once_flag cannot be reset, but we need cleanup() to allow
        // re-initialization on subsequent driver construction.
        // Also, std::call_once can throw std::system_error, which is incompatible
        // with -fno-exceptions builds.
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        // ── Singleton state ───────────────────────────────────────────────────
        static std::shared_ptr<RedisDBDriver> s_instance;
        static std::mutex                     s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                       s_registryMutex;
        static std::set<std::weak_ptr<RedisDBConnection>,
                        std::owner_less<std::weak_ptr<RedisDBConnection>>>      s_connectionRegistry;

        // ── Coalesced cleanup flag ────────────────────────────────────────────
        inline static std::atomic s_cleanupPending{false};

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<RedisDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<RedisDBConnection> &conn) noexcept;

        friend class RedisDBConnection;

        static void cleanup(std::nothrow_t) noexcept;

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        /**
         * @brief Construct a new Redis Driver
         */
        RedisDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;

        /**
         * @brief Destructor
         */
        ~RedisDBDriver() override;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        /**
         * @brief Return the singleton RedisDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<RedisDBDriver> getInstance();

        std::shared_ptr<KVDBConnection> connectKV(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton RedisDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<RedisDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

        std::string getURIScheme() const noexcept override;
        bool supportsClustering() const noexcept override;
        bool supportsReplication() const noexcept override;
        std::string getDriverVersion() const noexcept override;

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         */
        static size_t getConnectionAlive() noexcept;

        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept override;

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
    };

} // namespace cpp_dbc::Redis

#else // USE_REDIS

#include <memory>
#include <string>
#include <map>

namespace cpp_dbc::Redis
{
    class RedisDBDriver final : public KVDBDriver
    {
    public:
        RedisDBDriver() = default; // Driver not enabled in this build
        ~RedisDBDriver() override = default;

        RedisDBDriver(const RedisDBDriver &) = delete;
        RedisDBDriver &operator=(const RedisDBDriver &) = delete;
        RedisDBDriver(RedisDBDriver &&) = delete;
        RedisDBDriver &operator=(RedisDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<KVDBConnection> connectKV(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto r = connectKV(std::nothrow, uri, user, password, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================


        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
            std::nothrow_t,
            const std::string & /*uri*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("2UVDC4191LJY", "Redis support is not enabled in this build"));
        }

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string & /*uri*/) noexcept override
        {
            return cpp_dbc::unexpected(DBException("RM4SN70ZPIL7", "Redis support is not enabled in this build"));
        }

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string & /*host*/,
            int /*port*/,
            const std::string & /*database*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("WGWNIMNF0M85", "Redis support is not enabled in this build"));
        }

        std::string getURIScheme() const noexcept override { return "cpp_dbc:redis://<host>:<port>/<db>"; }
        bool supportsClustering() const noexcept override { return false; }
        bool supportsReplication() const noexcept override { return false; }
        std::string getDriverVersion() const noexcept override { return "0.0.0"; }
        std::string getName() const noexcept override { return "redis/disabled"; }
    };
} // namespace cpp_dbc::Redis

#endif // USE_REDIS
