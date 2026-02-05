#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_POSTGRESQL

#include <map>
#include <string>
#include <memory>

namespace cpp_dbc::PostgreSQL
{
    /**
     * @brief PostgreSQL database driver implementation
     *
     * Concrete RelationalDBDriver for PostgreSQL databases.
     * Accepts URLs in the format `cpp_dbc:postgresql://host:port/database`.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectRelational(
     *     "cpp_dbc:postgresql://localhost:5432/mydb", "postgres", "pass");
     * ```
     *
     * @see RelationalDBDriver, PostgreSQLDBConnection
     */
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

        /**
         * @brief Parse a JDBC-like URL into host, port, and database components
         * @param url URL in format "cpp_dbc:postgresql://host:port/database"
         * @param host Output: extracted hostname
         * @param port Output: extracted port number
         * @param database Output: extracted database name
         * @return true if parsing succeeded
         */
        bool parseURL(const std::string &url,
                      std::string &host,
                      int &port,
                      std::string &database) const;

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
