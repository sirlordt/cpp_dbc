#pragma once

#include "handles.hpp"

#include <map>
#include <memory>
#include <string>

#if USE_MONGODB

#include <atomic>
#include <mutex>

namespace cpp_dbc::MongoDB
{
    // ============================================================================
    // MongoDBDriver - Implements DocumentDBDriver
    // ============================================================================

    /**
     * @brief MongoDB driver implementation
     *
     * Concrete DocumentDBDriver for MongoDB databases using libmongoc.
     * Handles MongoDB C driver library initialization and cleanup.
     * Accepts URLs in the format `cpp_dbc:mongodb://host:port/database`.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectDocument(
     *     "cpp_dbc:mongodb://localhost:27017/mydb", "", "");
     * ```
     *
     * @see DocumentDBDriver, MongoDBConnection
     */
    class MongoDBDriver final : public DocumentDBDriver
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

        // Prevent copying and moving
        MongoDBDriver(const MongoDBDriver &) = delete;
        MongoDBDriver &operator=(const MongoDBDriver &) = delete;
        MongoDBDriver(MongoDBDriver &&) = delete;
        MongoDBDriver &operator=(MongoDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        std::shared_ptr<DocumentDBConnection> connectDocument(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        std::map<std::string, std::string> parseURI(const std::string &uri) override;
        std::string buildURI(
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        bool acceptsURL(const std::string &url) noexcept override;

        std::string getURIScheme() const noexcept override;
        bool supportsReplicaSets() const noexcept override;
        bool supportsSharding() const noexcept override;
        std::string getDriverVersion() const noexcept override;

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
        MongoDBDriver() = default;

        ~MongoDBDriver() override = default;

        MongoDBDriver(const MongoDBDriver &) = delete;
        MongoDBDriver &operator=(const MongoDBDriver &) = delete;
        MongoDBDriver(MongoDBDriver &&) = delete;
        MongoDBDriver &operator=(MongoDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        std::shared_ptr<DBConnection> connect(
            const std::string &,
            const std::string &,
            const std::string &,
            const std::map<std::string, std::string> & = {}) override
        {
            return nullptr;
        }

        std::shared_ptr<DocumentDBConnection> connectDocument(
            const std::string &,
            const std::string &,
            const std::string &,
            const std::map<std::string, std::string> & = {}) override
        {
            return nullptr;
        }

        std::map<std::string, std::string> parseURI(const std::string &) override
        {
            return {};
        }

        std::string buildURI(
            const std::string &,
            int,
            const std::string &,
            const std::map<std::string, std::string> & = {}) override
        {
            return {};
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        bool acceptsURL(const std::string &url) noexcept override
        {
            return url.starts_with("cpp_dbc:mongodb://");
        }

        std::string getURIScheme() const noexcept override
        {
            return "cpp_dbc:mongodb://";
        }

        bool supportsReplicaSets() const noexcept override
        {
            return false;
        }

        bool supportsSharding() const noexcept override
        {
            return false;
        }

        std::string getDriverVersion() const noexcept override
        {
            return "disabled";
        }

        cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> connectDocument(std::nothrow_t, const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("YHFE2OPW1DZR", "MongoDB support is not enabled in this build"));
        }

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &) noexcept override
        {
            return cpp_dbc::unexpected(DBException("QRXRXWB7UHKN", "MongoDB support is not enabled in this build"));
        }

        std::string getName() const noexcept override { return "MongoDB (disabled)"; }
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
