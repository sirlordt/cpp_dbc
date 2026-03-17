#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <map>
#include <set>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace cpp_dbc::SQLite
{
    class SQLiteDBConnection; // forward declaration for registry

    /**
     * @brief SQLite database driver implementation
     *
     * Concrete RelationalDBDriver for SQLite embedded databases.
     * Accepts URLs in the format `cpp_dbc:sqlite:/path/to/db.sqlite`
     * or `cpp_dbc:sqlite::memory:` for in-memory databases.
     *
     * ```cpp
     * auto driver = cpp_dbc::SQLite::SQLiteDBDriver::getInstance();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectRelational("cpp_dbc:sqlite:/tmp/test.db", "", "");
     * ```
     *
     * @see RelationalDBDriver, SQLiteDBConnection
     */
    class SQLiteDBDriver final : public RelationalDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // Static members to ensure SQLite is configured only once
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        // ── Singleton state ───────────────────────────────────────────────────
        static std::shared_ptr<SQLiteDBDriver> s_instance;
        static std::mutex                      s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                        s_registryMutex;
        static std::set<std::weak_ptr<SQLiteDBConnection>,
                        std::owner_less<std::weak_ptr<SQLiteDBConnection>>>      s_connectionRegistry;

        // ── Coalesced cleanup flag ────────────────────────────────────────────
        static std::atomic<bool> s_cleanupPending;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<SQLiteDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<SQLiteDBConnection> &conn) noexcept;

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        friend class SQLiteDBConnection;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        SQLiteDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;
        ~SQLiteDBDriver() override;

        SQLiteDBDriver(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver &operator=(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver(SQLiteDBDriver &&) = delete;
        SQLiteDBDriver &operator=(SQLiteDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        /**
         * @brief Return the singleton SQLiteDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<SQLiteDBDriver> getInstance();

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &uri,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton SQLiteDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<SQLiteDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

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

        static void cleanup() noexcept;

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         */
        static size_t getConnectionAlive() noexcept;
    };

} // namespace cpp_dbc::SQLite

#else // USE_SQLITE

// Stub implementations when SQLite is disabled
namespace cpp_dbc::SQLite
{
    // Stub class — PrivateCtorTag pattern not applied because this class is never
    // instantiated through the factory path; it exists solely to satisfy the
    // compilation interface when USE_SQLITE=0.
    class SQLiteDBDriver final : public RelationalDBDriver
    {
    public:
        SQLiteDBDriver() = default;
        ~SQLiteDBDriver() override = default;

        SQLiteDBDriver(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver &operator=(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver(SQLiteDBDriver &&) = delete;
        SQLiteDBDriver &operator=(SQLiteDBDriver &&) = delete;

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
            return cpp_dbc::unexpected(DBException("MAGZJXNF89IG", "SQLite support is not enabled in this build", system_utils::captureCallStack()));
        }

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string & /*host*/,
            int /*port*/,
            const std::string & /*database*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("KNJHPWDXS1Z6", "SQLite support is not enabled in this build", system_utils::captureCallStack()));
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*uri*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("2448VZ77JXYM", "SQLite support is not enabled in this build", system_utils::captureCallStack()));
        }

        std::string getName() const noexcept override
        {
            return "sqlite/disabled";
        }

        std::string getURIScheme() const noexcept override
        {
            return "cpp_dbc:sqlite://<path>";
        }

        std::string getDriverVersion() const noexcept override
        {
            return "unknown";
        }
    };
} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
