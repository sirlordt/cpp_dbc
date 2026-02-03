#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_MYSQL

#include <map>
#include <string>
#include <memory>

namespace cpp_dbc::MySQL
{
        class MySQLDBDriver final : public RelationalDBDriver
        {
        public:
            MySQLDBDriver();
            ~MySQLDBDriver() override;

            // Rule of 5: disable copy and move operations
            MySQLDBDriver(const MySQLDBDriver &) = delete;
            MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
            MySQLDBDriver(MySQLDBDriver &&) = delete;
            MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

            std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                      const std::string &user,
                                                                      const std::string &password,
                                                                      const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a JDBC-like URL: cpp_dbc:mysql://host:port/database
            bool parseURL(const std::string &url,
                          std::string &host,
                          int &port,
                          std::string &database);

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            std::string getName() const noexcept override;
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
            MySQLDBDriver()
            {
                throw DBException("4FE1EBBEA99F", "MySQL support is not enabled in this build");
            }
            ~MySQLDBDriver() override = default;

            MySQLDBDriver(const MySQLDBDriver &) = delete;
            MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
            MySQLDBDriver(MySQLDBDriver &&) = delete;
            MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

            [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &,
                                                                      const std::string &,
                                                                      const std::string &,
                                                                      const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
            {
                throw DBException("23D2107DA64F", "MySQL support is not enabled in this build");
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
                return cpp_dbc::unexpected(DBException("23D2107DA650", "MySQL support is not enabled in this build"));
            }

            std::string getName() const noexcept override
            {
                return "MySQL (disabled)";
            }
        };
} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
