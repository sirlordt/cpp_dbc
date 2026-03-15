#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_MYSQL

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace cpp_dbc::MySQL
{
    class MySQLDBConnection; // forward declaration for registry

    /**
     * @brief MySQL database driver implementation — singleton
     *
     * Concrete RelationalDBDriver for MySQL/MariaDB databases.
     * Accepts URLs in the format `cpp_dbc:mysql://host:port/database`.
     *
     * The driver is a singleton: use getInstance() to obtain the shared instance.
     * Every connection created through connectRelational() is registered in the
     * driver's connection registry and automatically removed when closed.
     *
     * ```cpp
     * auto driver = cpp_dbc::MySQL::MySQLDBDriver::getInstance();
     * auto conn = driver->connectRelational("cpp_dbc:mysql://localhost:3306/mydb", "root", "pass");
     * ```
     *
     * @see RelationalDBDriver, MySQLDBConnection
     */
    class MySQLDBDriver final : public RelationalDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Singleton state ───────────────────────────────────────────────────
        // Note: MySQL does NOT use double-checked locking (s_initialized + s_initMutex)
        // because mysql_library_end() must be called unconditionally for Valgrind-clean
        // shutdown. See initialize() in driver_01.cpp for full explanation.
        static std::shared_ptr<MySQLDBDriver> s_instance;
        static std::mutex                     s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        // Tracks all live connections created through connectRelational().
        // Uses weak_ptr so the registry never prevents connection destruction.
        static std::mutex                                                    s_registryMutex;
        static std::set<std::weak_ptr<MySQLDBConnection>,
                        std::owner_less<std::weak_ptr<MySQLDBConnection>>>   s_connectionRegistry;

        // ── Coalesced cleanup flag ────────────────────────────────────────────
        // Ensures at most one registry cleanup task is queued in SerialQueue
        // at any time. See registerConnection() for the coalescence pattern.
        static std::atomic<bool> s_cleanupPending;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<MySQLDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<MySQLDBConnection> &conn) noexcept;

        static void cleanup();

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        friend class MySQLDBConnection;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        MySQLDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;
        ~MySQLDBDriver() override;

        // Rule of 5: disable copy and move operations
        MySQLDBDriver(const MySQLDBDriver &) = delete;
        MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
        MySQLDBDriver(MySQLDBDriver &&) = delete;
        MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        /**
         * @brief Return the singleton MySQLDBDriver instance, creating it if necessary.
         * @throws DBException if MySQL library initialization fails.
         */
        static std::shared_ptr<MySQLDBDriver> getInstance();

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
         * @brief Return the singleton MySQLDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<MySQLDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

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
         * connections created directly via MySQLDBConnection::create() are not counted.
         */
        static size_t getConnectionAlive() noexcept;
    };

} // namespace cpp_dbc::MySQL

#else // USE_MYSQL

// Stub implementations when MySQL is disabled
namespace cpp_dbc::MySQL
{
    // Forward declarations only
    class MySQLDBDriver final : public RelationalDBDriver
    {
    public:
        MySQLDBDriver() = default;
        ~MySQLDBDriver() override = default;

        MySQLDBDriver(const MySQLDBDriver &) = delete;
        MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
        MySQLDBDriver(MySQLDBDriver &&) = delete;
        MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

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
            std::nothrow_t, const std::string &) noexcept override
        {
            return cpp_dbc::unexpected(DBException("60KVZ1GMDYHY", "MySQL support is not enabled in this build"));
        }

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &,
            int,
            const std::string &,
            const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("YPQJVKDIBJGI", "MySQL support is not enabled in this build"));
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*uri*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("NJTLBRGTJYPB", "MySQL support is not enabled in this build"));
        }

        std::string getName() const noexcept override
        {
            return "mysql/disabled";
        }

        std::string getURIScheme() const noexcept override
        {
            return "cpp_dbc:mysql://<host>:<port>/<database>";
        }

        std::string getDriverVersion() const noexcept override
        {
            return "unknown";
        }
    };
} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
