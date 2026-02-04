#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_POSTGRESQL

#include <map>
#include <string>
#include <memory>

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBDriver final : public RelationalDBDriver
    {
    public:
        PostgreSQLDBDriver();
        ~PostgreSQLDBDriver() override;

        // Rule of 5: disable copy and move operations
        PostgreSQLDBDriver(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver &operator=(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver(PostgreSQLDBDriver &&) = delete;
        PostgreSQLDBDriver &operator=(PostgreSQLDBDriver &&) = delete;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        bool acceptsURL(const std::string &url) override;

        // Parses a JDBC-like URL: cpp_dbc:postgresql://host:port/database
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

} // namespace cpp_dbc::PostgreSQL

#else // USE_POSTGRESQL

// Stub implementations when PostgreSQL is disabled
namespace cpp_dbc::PostgreSQL
{
    // Forward declarations only
    class PostgreSQLDBDriver final : public RelationalDBDriver
    {
    public:
        PostgreSQLDBDriver()
        {
            throw DBException("3FE734D0BDE9", "PostgreSQL support is not enabled in this build");
        }
        ~PostgreSQLDBDriver() override = default;

        PostgreSQLDBDriver(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver &operator=(const PostgreSQLDBDriver &) = delete;
        PostgreSQLDBDriver(PostgreSQLDBDriver &&) = delete;
        PostgreSQLDBDriver &operator=(PostgreSQLDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &,
                                                                               const std::string &,
                                                                               const std::string &,
                                                                               const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
        {
            throw DBException("E39F6F23D06B", "PostgreSQL support is not enabled in this build");
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
            return cpp_dbc::unexpected(DBException("E39F6F23D06C", "PostgreSQL support is not enabled in this build"));
        }

        std::string getName() const noexcept override
        {
            return "PostgreSQL (disabled)";
        }
    };
} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
