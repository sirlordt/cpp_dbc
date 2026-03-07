#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_MYSQL

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <memory>

namespace cpp_dbc::MySQL
{
    /**
     * @brief MySQL database driver implementation
     *
     * Concrete RelationalDBDriver for MySQL/MariaDB databases.
     * Accepts URLs in the format `cpp_dbc:mysql://host:port/database`.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectRelational("cpp_dbc:mysql://localhost:3306/mydb", "root", "pass");
     * ```
     *
     * @see RelationalDBDriver, MySQLDBConnection
     */
    class MySQLDBDriver final : public RelationalDBDriver
    {
        // Note: atomic<bool> + mutex instead of std::once_flag because
        // std::once_flag cannot be reset, but we need cleanup() to allow
        // re-initialization on subsequent driver construction.
        // Also, std::call_once can throw std::system_error, which is incompatible
        // with -fno-exceptions builds.
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

    public:
        MySQLDBDriver();
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
        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        bool acceptsURL(const std::string &url) noexcept override;

        /**
         * @brief Parse a JDBC-like URL into host, port, and database components
         * @param url URL in format "cpp_dbc:mysql://host:port/database"
         * @param host Output: extracted hostname
         * @param port Output: extracted port number
         * @param database Output: extracted database name
         * @return true if parsing succeeded
         */
        bool parseURL(const std::string &url,
                      std::string &host,
                      int &port,
                      std::string &database) const;

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;

        static void cleanup();
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

        bool acceptsURL(const std::string &url) noexcept override
        {
            return url.starts_with("cpp_dbc:mysql://");
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*url*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("NJTLBRGTJYPB", "MySQL support is not enabled in this build"));
        }

        std::string getName() const noexcept override
        {
            return "MySQL (disabled)";
        }
    };
} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
