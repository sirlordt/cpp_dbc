#pragma once

#include "../../../cpp_dbc.hpp"
#include "blob.hpp"
#include "connection.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <any>

namespace cpp_dbc::Firebird
{
    /**
     * @brief Firebird Driver implementation
     */
    class FirebirdDBDriver final : public RelationalDBDriver
    {
    private:
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

    public:
        FirebirdDBDriver();
        ~FirebirdDBDriver() override;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        bool acceptsURL(const std::string &url) override;

        /**
         * @brief Execute a driver-specific command
         *
         * Supported commands:
         * - "create_database": Creates a new Firebird database
         *   Required params: "url", "user", "password"
         *   Optional params: "page_size" (default: "4096"), "charset" (default: "UTF8")
         *
         * @param params Command parameters as a map of string to std::any
         * @return 0 on success
         * @throws DBException if the command fails
         */
        int command(const std::map<std::string, std::any> &params) override;

        /**
         * @brief Creates a new Firebird database
         *
         * This method creates a new database file using isc_dsql_execute_immediate.
         * It can be called without an existing connection.
         *
         * @param url The database URL (cpp_dbc:firebird://host:port/path/to/database.fdb)
         * @param user The database user (typically SYSDBA)
         * @param password The user's password
         * @param options Optional parameters:
         *                - "page_size": Database page size (default: 4096)
         *                - "charset": Default character set (default: UTF8)
         * @return true if database was created successfully
         * @throws DBException if database creation fails
         */
        bool createDatabase(const std::string &url,
                            const std::string &user,
                            const std::string &password,
                            const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

        /**
         * @brief Parses a URL: cpp_dbc:firebird://host:port/path/to/database.fdb
         * @param url The URL to parse
         * @param host Output: the host name
         * @param port Output: the port number
         * @param database Output: the database path
         * @return true if parsing was successful
         */
        bool parseURL(const std::string &url, std::string &host, int &port, std::string &database);

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
        connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
    };

    // ============================================================================
    // FirebirdBlob inline method implementations
    // These must be defined after FirebirdConnection is fully defined
    // ============================================================================

    /**
     * @brief Get the database handle from the connection
     * @return Pointer to the database handle
     */
    inline isc_db_handle *FirebirdBlob::getDbHandle() const
    {
        auto conn = getConnection();
        return conn->m_db.get();
    }

    /**
     * @brief Get the transaction handle from the connection
     * @return Pointer to the transaction handle
     */
    inline isc_tr_handle *FirebirdBlob::getTrHandle() const
    {
        auto conn = getConnection();
        return &conn->m_tr;
    }

} // namespace cpp_dbc::Firebird

#else // USE_FIREBIRD

// Stub implementations when Firebird is disabled
namespace cpp_dbc::Firebird
{
    class FirebirdDBDriver final : public RelationalDBDriver
    {
    public:
        [[noreturn]] FirebirdDBDriver()
        {
            throw DBException("R9T3U5V1W7X4", "Firebird support is not enabled in this build", system_utils::captureCallStack());
        }
        ~FirebirdDBDriver() override = default;

        FirebirdDBDriver(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver &operator=(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver(FirebirdDBDriver &&) = delete;
        FirebirdDBDriver &operator=(FirebirdDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string & /*url*/,
                                                                               const std::string & /*user*/,
                                                                               const std::string & /*password*/,
                                                                               const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) override
        {
            throw DBException("S0U4V6W2X8Y5", "Firebird support is not enabled in this build", system_utils::captureCallStack());
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
            return cpp_dbc::unexpected(DBException("S0U4V6W2X8Y6", "Firebird support is not enabled in this build", system_utils::captureCallStack()));
        }

        std::string getName() const noexcept override
        {
            return "Firebird (disabled)";
        }
    };
} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
