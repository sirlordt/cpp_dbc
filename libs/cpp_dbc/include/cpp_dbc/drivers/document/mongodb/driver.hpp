#pragma once

#include "handles.hpp"

#include <map>
#include <memory>
#include <string>

#if USE_MONGODB

#include <atomic>
#include <mutex>
#include <set>

namespace cpp_dbc::MongoDB
{
    class MongoDBConnection; // forward declaration for registry

    // ============================================================================
    // MongoDBDriver - Implements DocumentDBDriver
    // ============================================================================

    /**
     * @brief MongoDB driver implementation — singleton
     *
     * Concrete DocumentDBDriver for MongoDB databases using libmongoc.
     * Handles MongoDB C driver library initialization and cleanup.
     * Accepts URLs in the format `cpp_dbc:mongodb://host:port/database`.
     *
     * ```cpp
     * auto driver = cpp_dbc::MongoDB::MongoDBDriver::getInstance();
     * auto conn = driver->connectDocument(
     *     "cpp_dbc:mongodb://localhost:27017/mydb", "", "");
     * ```
     *
     * @see DocumentDBDriver, MongoDBConnection
     */
    class MongoDBDriver final : public DocumentDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // Note: Using atomic<bool> + mutex instead of std::once_flag for the
        // initialization guard because std::once_flag cannot be reset, but
        // cleanup() must allow re-initialization on subsequent driver construction.
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        // ── Singleton state ───────────────────────────────────────────────────
        static std::weak_ptr<MongoDBDriver> s_instance;
        static std::mutex                   s_instanceMutex;

        // ── atexit cleanup guard ──────────────────────────────────────────────
        // Ensures mongoc_cleanup() is registered with std::atexit exactly once
        // across the entire process lifetime, regardless of how many times the
        // singleton is created and destroyed. std::call_once may throw
        // std::system_error on a broken OS threading primitive, which is a
        // death sentence — std::terminate is the correct response.
        static std::once_flag s_atexitFlag;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                      s_registryMutex;
        static std::set<std::weak_ptr<MongoDBConnection>,
                        std::owner_less<std::weak_ptr<MongoDBConnection>>>     s_connectionRegistry;

        /**
         * @brief Initialize the MongoDB C driver library
         */
        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<MongoDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<MongoDBConnection> &conn) noexcept;

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        friend class MongoDBConnection;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        /**
         * @brief Construct a MongoDB driver
         *
         * This will initialize the MongoDB C driver library if not already done.
         */
        MongoDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;

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
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        /**
         * @brief Return the singleton MongoDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<MongoDBDriver> getInstance();

        std::shared_ptr<DocumentDBConnection> connectDocument(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton MongoDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<MongoDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

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

        expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         */
        static size_t getConnectionAlive() noexcept;
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
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<DocumentDBConnection> connectDocument(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = {}) override
        {
            auto r = connectDocument(std::nothrow, uri, user, password, options);
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

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &,
            int,
            const std::string &,
            const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("4JUTEUJPL254", "MongoDB support is not enabled in this build"));
        }

        std::string getName() const noexcept override { return "MongoDB (disabled)"; }
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
