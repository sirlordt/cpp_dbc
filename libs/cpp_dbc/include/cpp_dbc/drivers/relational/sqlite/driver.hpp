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
    class SQLiteDBDriver final : public RelationalDBDriver
    {
    private:
        // Static members to ensure SQLite is configured only once
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

    public:
        SQLiteDBDriver();
        ~SQLiteDBDriver() override;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        bool acceptsURL(const std::string &url) override;

        // Parses a URL: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
        bool parseURL(const std::string &url, std::string &database);

        // Nothrow API
        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
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
        [[noreturn]] SQLiteDBDriver()
        {
            throw DBException("C27AD46A860B", "SQLite support is not enabled in this build", system_utils::captureCallStack());
        }
        ~SQLiteDBDriver() override = default;

        SQLiteDBDriver(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver &operator=(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver(SQLiteDBDriver &&) = delete;
        SQLiteDBDriver &operator=(SQLiteDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                               const std::string &user,
                                                                               const std::string &password,
                                                                               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            throw DBException("269CC140F035", "SQLite support is not enabled in this build", system_utils::captureCallStack());
        }

        bool acceptsURL(const std::string & /*url*/) override
        {
            return false;
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*url*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("269CC140F036", "SQLite support is not enabled in this build", system_utils::captureCallStack()));
        }

        std::string getName() const noexcept override
        {
            return "SQLite (disabled)";
        }
    };
} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
