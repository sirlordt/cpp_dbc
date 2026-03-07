#pragma once

#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"

#if USE_REDIS

#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <map>

namespace cpp_dbc::Redis
{
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
     * auto driver = std::make_shared<cpp_dbc::Redis::RedisDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectKV("cpp_dbc:redis://localhost:6379", "", "");
     * auto reply = conn->ping();
     * conn->close();
     * ```
     */
    class RedisDBDriver final : public KVDBDriver
    {
    private:
        // Note: Using atomic<bool> + mutex instead of std::once_flag because
        // std::once_flag cannot be reset, but we need cleanup() to allow
        // re-initialization on subsequent driver construction.
        // Also, std::call_once can throw std::system_error, which is incompatible
        // with -fno-exceptions builds.
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        /**
         * @brief Initialize Redis driver
         */
        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

    public:
        /**
         * @brief Construct a new Redis Driver
         */
        RedisDBDriver();

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

        std::shared_ptr<KVDBConnection> connectKV(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================


        std::string getURIScheme() const noexcept override;
        bool supportsClustering() const noexcept override;
        bool supportsReplication() const noexcept override;
        std::string getDriverVersion() const noexcept override;

        /**
         * @brief Clean up Redis driver resources
         */
        static void cleanup();

        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
            std::nothrow_t,
            const std::string &url,
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
        std::shared_ptr<KVDBConnection> connectKV(
            const std::string &,
            const std::string &,
            const std::string &,
            const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
        {
            return nullptr;
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================


        cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
            std::nothrow_t,
            const std::string & /*url*/,
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
            return cpp_dbc::unexpected(DBException("RM4SN70ZPIL7", "Redis support is not enabled in this build"));
        }

        std::string getURIScheme() const noexcept override { return "cpp_dbc:redis://<host>:<port>/<db>"; }
        bool supportsClustering() const noexcept override { return false; }
        bool supportsReplication() const noexcept override { return false; }
        std::string getDriverVersion() const noexcept override { return "0.0.0"; }
        std::string getName() const noexcept override { return "redis/disabled"; }
    };
} // namespace cpp_dbc::Redis

#endif // USE_REDIS
