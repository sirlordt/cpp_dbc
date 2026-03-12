#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_POSTGRESQL

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBConnection; // forward declaration for registry

    /**
     * @brief PostgreSQL database driver implementation — singleton
     *
     * Concrete RelationalDBDriver for PostgreSQL databases.
     * Accepts URLs in the format `cpp_dbc:postgresql://host:port/database`.
     *
     * The driver is a singleton: use getInstance() to obtain the shared instance.
     * Every connection created through connectRelational() is registered in the
     * driver's connection registry and automatically removed when closed.
     *
     * ```cpp
     * auto driver = cpp_dbc::PostgreSQL::PostgreSQLDBDriver::getInstance();
     * auto conn = driver->connectRelational(
     *     "cpp_dbc:postgresql://localhost:5432/mydb", "postgres", "pass");
     * ```
     *
     * @see RelationalDBDriver, PostgreSQLDBConnection
     */
    class PostgreSQLDBDriver final : public RelationalDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Singleton state ───────────────────────────────────────────────────
        // Note: Uses weak_ptr-based lazy singleton. cleanup() is called
        // unconditionally in the destructor for clean shutdown.
        // See initialize() in driver_01.cpp for details.
        static std::weak_ptr<PostgreSQLDBDriver> s_instance;
        static std::mutex                        s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                          s_registryMutex;
        static std::set<std::weak_ptr<PostgreSQLDBConnection>,
                        std::owner_less<std::weak_ptr<PostgreSQLDBConnection>>>   s_connectionRegistry;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<PostgreSQLDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<PostgreSQLDBConnection> &conn) noexcept;

        static void cleanup();

        friend class PostgreSQLDBConnection;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

    public:
        PostgreSQLDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;
        ~PostgreSQLDBDriver() override;

        // Rule of 5: disable copy and move operations
        PostgreSQLDBDriver(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver &operator=(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver(PostgreSQLDBDriver &&) = delete;
        PostgreSQLDBDriver &operator=(PostgreSQLDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        /**
         * @brief Return the singleton PostgreSQLDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<PostgreSQLDBDriver> getInstance();

        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &uri,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton PostgreSQLDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<PostgreSQLDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept override;

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
        std::string getURIScheme() const noexcept override;
        std::string getDriverVersion() const noexcept override;

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         * This value reflects connections created through connectRelational() only;
         * connections created directly via PostgreSQLDBConnection::create() are not counted.
         */
        static size_t getConnectionAlive() noexcept;
    };

} // namespace cpp_dbc::PostgreSQL

#else // USE_POSTGRESQL

// Stub implementations when PostgreSQL is disabled
namespace cpp_dbc::PostgreSQL
{
    // Forward declarations only
    class PostgreSQLDBDriver final : public RelationalDBDriver
    {
    public:
        PostgreSQLDBDriver() = default;
        ~PostgreSQLDBDriver() override = default;

        PostgreSQLDBDriver(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver &operator=(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver(PostgreSQLDBDriver &&) = delete;
        PostgreSQLDBDriver &operator=(PostgreSQLDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &uri,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto r = connectRelational(std::nothrow, uri, user, password, options);
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


        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string & /*uri*/) noexcept override
        {
            return cpp_dbc::unexpected(DBException("I8PRRLJR6DYE", "PostgreSQL support is not enabled in this build"));
        }

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string & /*host*/,
            int /*port*/,
            const std::string & /*database*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("3D7SEJIB1ZCK", "PostgreSQL support is not enabled in this build"));
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*uri*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("E39F6F23D06C", "PostgreSQL support is not enabled in this build"));
        }

        std::string getName() const noexcept override
        {
            return "postgresql/disabled";
        }

        std::string getURIScheme() const noexcept override
        {
            return "cpp_dbc:postgresql://<host>:<port>/<database>";
        }

        std::string getDriverVersion() const noexcept override
        {
            return "unknown";
        }
    };
} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
