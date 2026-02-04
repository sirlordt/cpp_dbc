#pragma once

#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <map>

namespace cpp_dbc::Redis
{
        /**
         * @brief Redis driver implementation
         *
         * This class implements the KVDBDriver interface for Redis.
         */
        class RedisDriver final : public KVDBDriver
        {
        public:
            /**
             * @brief Construct a new Redis Driver
             */
            RedisDriver();

            /**
             * @brief Destructor
             */
            ~RedisDriver() override;

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
            // Note: Using atomic<bool> + mutex instead of std::once_flag because
            // std::once_flag cannot be reset, but we need cleanup() to allow
            // re-initialization on subsequent driver construction.
            static std::atomic<bool> s_initialized;
            static std::mutex s_initMutex;

            /**
             * @brief Initialize Redis driver
             */
            static void initialize();

            std::mutex m_mutex;
        };

} // namespace cpp_dbc::Redis
