#pragma once

#include "handles.hpp"

#if USE_MONGODB

#include <map>
#include <string>
#include <memory>

namespace cpp_dbc::MongoDB
{
        // ============================================================================
        // MongoDBDriver - Implements DocumentDBDriver
        // ============================================================================

        /**
         * @brief MongoDB driver implementation
         *
         * This class provides the entry point for creating MongoDB connections.
         * It handles MongoDB library initialization and cleanup.
         *
         * Key safety features:
         * - Thread-safe initialization using call_once
         * - Proper library cleanup on destruction
         * - URI parsing and validation
         */
        class MongoDBDriver final : public DocumentDBDriver
        {
        private:
            /**
             * @brief Flag indicating if mongoc has been initialized
             */
            static std::once_flag s_initFlag;

            /**
             * @brief Flag indicating if mongoc should be cleaned up
             */
            static std::atomic<bool> s_initialized;

            /**
             * @brief Initialize the MongoDB C driver library
             */
            static void initializeMongoc();

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

        public:
            /**
             * @brief Construct a MongoDB driver
             *
             * This will initialize the MongoDB C driver library if not already done.
             */
            MongoDBDriver();

            /**
             * @brief Destructor
             *
             * Note: mongoc_cleanup() is NOT called here because it should only
             * be called once when the application exits. Use cleanup() explicitly
             * if needed.
             */
            ~MongoDBDriver() override;

            // Prevent copying
            MongoDBDriver(const MongoDBDriver &) = delete;
            MongoDBDriver &operator=(const MongoDBDriver &) = delete;

            // DBDriver interface
            std::shared_ptr<DBConnection> connect(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // DocumentDBDriver interface
            std::shared_ptr<DocumentDBConnection> connectDocument(
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
                const std::string &database,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool supportsReplicaSets() const override;
            bool supportsSharding() const override;
            std::string getDriverVersion() const override;

            // MongoDB-specific methods

            /**
             * @brief Explicitly cleanup the MongoDB C driver library
             *
             * This should only be called once when the application is exiting.
             * After calling this, no more MongoDB operations should be performed.
             */
            static void cleanup();

            /**
             * @brief Check if the MongoDB library has been initialized
             * @return true if initialized
             */
            static bool isInitialized();

            /**
             * @brief Validate a MongoDB URI
             * @param uri The URI to validate
             * @return true if the URI is valid
             */
            static bool validateURI(const std::string &uri);

            // Nothrow versions
            expected<std::shared_ptr<DocumentDBConnection>, DBException> connectDocument(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            expected<std::map<std::string, std::string>, DBException> parseURI(
                std::nothrow_t, const std::string &uri) noexcept override;

            std::string getName() const noexcept override;
        };

} // namespace cpp_dbc::MongoDB

#else // USE_MONGODB

// Stub implementations when MongoDB is disabled
namespace cpp_dbc::MongoDB
{
        /**
         * @brief Stub MongoDB driver when MongoDB support is disabled
         * This class throws an exception on any operation, indicating that
         * MongoDB support was not compiled into the library.*
         */
        class MongoDBDriver final : public DocumentDBDriver
        {
        public:
            MongoDBDriver()
            {
                throw DBException("F3E813E10E8C", "MongoDB support is not enabled in this build");
            }

            ~MongoDBDriver() override = default;

            MongoDBDriver(const MongoDBDriver &) = delete;
            MongoDBDriver &operator=(const MongoDBDriver &) = delete;
            MongoDBDriver(MongoDBDriver &&) = delete;
            MongoDBDriver &operator=(MongoDBDriver &&) = delete;

            std::shared_ptr<DBConnection> connect(
                const std::string &,
                const std::string &,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("AC208113FF23", "MongoDB support is not enabled in this build");
            }

            bool acceptsURL(const std::string &) override
            {
                return false;
            }

            std::shared_ptr<DocumentDBConnection> connectDocument(
                const std::string &,
                const std::string &,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("2CC107C18A39", "MongoDB support is not enabled in this build");
            }

            int getDefaultPort() const override
            {
                return 27017;
            }

            std::string getURIScheme() const override
            {
                return "mongodb";
            }

            std::map<std::string, std::string> parseURI(const std::string &) override
            {
                throw DBException("1BB61E9DD031", "MongoDB support is not enabled in this build");
            }

            std::string buildURI(
                const std::string &,
                int,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("11202995FCE7", "MongoDB support is not enabled in this build");
            }

            bool supportsReplicaSets() const override
            {
                return false;
            }

            bool supportsSharding() const override
            {
                return false;
            }

            std::string getDriverVersion() const override
            {
                return "disabled";
            }

            std::string getName() const noexcept override;
        };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
