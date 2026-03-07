#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace cpp_dbc::SQLite
{
    /**
     * @brief SQLite database driver implementation
     *
     * Concrete RelationalDBDriver for SQLite embedded databases.
     * Accepts URLs in the format `cpp_dbc:sqlite:/path/to/db.sqlite`
     * or `cpp_dbc:sqlite::memory:` for in-memory databases.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectRelational("cpp_dbc:sqlite:/tmp/test.db", "", "");
     * ```
     *
     * @see RelationalDBDriver, SQLiteDBConnection
     */
    class SQLiteDBDriver final : public RelationalDBDriver
    {
    private:
        // Static members to ensure SQLite is configured only once
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

    public:
        SQLiteDBDriver();
        ~SQLiteDBDriver() override;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================


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
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
        std::string getURIScheme() const noexcept override;
    };

} // namespace cpp_dbc::SQLite

#else // USE_SQLITE

// Stub implementations when SQLite is disabled
namespace cpp_dbc::SQLite
{
    // Forward declarations only
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

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &,
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
            const std::string & /*url*/,
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
    };
} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
