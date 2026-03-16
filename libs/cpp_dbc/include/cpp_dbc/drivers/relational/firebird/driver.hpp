/**

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file driver.hpp
 @brief Firebird database driver declaration - FirebirdDBDriver

*/

#pragma once

#include "../../../cpp_dbc.hpp"
#include "blob.hpp"
#include "connection.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <mutex>
#include <any>

namespace cpp_dbc::Firebird
{
    class FirebirdDBConnection; // forward declaration for registry

    /**
     * @brief Firebird database driver implementation — singleton
     *
     * Concrete RelationalDBDriver for Firebird databases.
     * Accepts URLs in the format `cpp_dbc:firebird://host:port/path/to/database.fdb`.
     * Supports database creation via the `createDatabase()` method.
     *
     * ```cpp
     * auto driver = cpp_dbc::Firebird::FirebirdDBDriver::getInstance();
     * auto conn = driver->connectRelational(
     *     "cpp_dbc:firebird://localhost:3050/tmp/test.fdb", "SYSDBA", "masterkey");
     * ```
     *
     * @see RelationalDBDriver, FirebirdDBConnection
     */
    class FirebirdDBDriver final : public RelationalDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Singleton state ───────────────────────────────────────────────────
        static std::shared_ptr<FirebirdDBDriver> s_instance;
        static std::mutex                        s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                        s_registryMutex;
        static std::set<std::weak_ptr<FirebirdDBConnection>,
                        std::owner_less<std::weak_ptr<FirebirdDBConnection>>>   s_connectionRegistry;

        // ── Coalesced cleanup flag ────────────────────────────────────────────
        static std::atomic<bool> s_cleanupPending;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<FirebirdDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<FirebirdDBConnection> &conn) noexcept;

        static void cleanup();

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        /**
         * @brief Parsed URI components for Firebird connections.
         */
        struct ParsedFirebirdComponents
        {
            std::string host;
            int port{3050};
            std::string database;
        };

        /**
         * @brief Extract host, port and database from the map returned by parseURI().
         * @param parsed The map produced by parseURI(std::nothrow, uri).
         * @return ParsedFirebirdComponents on success, or DBException on invalid port / empty database.
         */
        static cpp_dbc::expected<ParsedFirebirdComponents, DBException>
        extractComponents(std::nothrow_t, const std::map<std::string, std::string> &parsed) noexcept;

        friend class FirebirdDBConnection;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        // ── Constructor ───────────────────────────────────────────────────────
        FirebirdDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;

        // ── Destructor ────────────────────────────────────────────────────────
        ~FirebirdDBDriver() override;

        // ── Deleted copy/move — non-copyable, non-movable ─────────────────────
        FirebirdDBDriver(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver &operator=(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver(FirebirdDBDriver &&) = delete;
        FirebirdDBDriver &operator=(FirebirdDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::buildURI;
        using DBDriver::parseURI;

        /**
         * @brief Return the singleton FirebirdDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<FirebirdDBDriver> getInstance();

        std::shared_ptr<RelationalDBConnection> connectRelational(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        /**
         * @brief Execute a driver-specific command
         *
         * Supported commands:
         * - "create_database": Creates a new Firebird database
         *   Required params: "uri", "user", "password"
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
         * @param uri The database URI (cpp_dbc:firebird://host:port/path/to/database.fdb)
         * @param user The database user (typically SYSDBA)
         * @param password The user's password
         * @param options Optional parameters:
         *                - "page_size": Database page size (default: 4096)
         *                - "charset": Default character set (default: UTF8)
         * @return true if database was created successfully
         * @throws DBException if database creation fails
         */
        bool createDatabase(const std::string &uri,
                            const std::string &user,
                            const std::string &password,
                            const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton FirebirdDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<FirebirdDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept override;

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
        connectRelational(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        [[nodiscard]] cpp_dbc::expected<int, DBException>
        command(std::nothrow_t, const std::map<std::string, std::any> &params) noexcept;

        [[nodiscard]] cpp_dbc::expected<bool, DBException>
        createDatabase(std::nothrow_t,
                       const std::string &uri,
                       const std::string &user,
                       const std::string &password,
                       const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept;

        std::string getName() const noexcept override;
        std::string getURIScheme() const noexcept override;
        std::string getDriverVersion() const noexcept override;

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         */
        static size_t getConnectionAlive() noexcept;
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
        // ── Constructor and Destructor ────────────────────────────────────────
        FirebirdDBDriver() = default;
        ~FirebirdDBDriver() override = default;

        // ── Deleted copy/move — non-copyable, non-movable ─────────────────────
        FirebirdDBDriver(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver &operator=(const FirebirdDBDriver &) = delete;
        FirebirdDBDriver(FirebirdDBDriver &&) = delete;
        FirebirdDBDriver &operator=(FirebirdDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::buildURI;
        using DBDriver::parseURI;

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

        int command(const std::map<std::string, std::any> &params) override
        {
            auto r = command(std::nothrow, params);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        bool createDatabase(const std::string &,
                            const std::string &,
                            const std::string &,
                            const std::map<std::string, std::string> & = std::map<std::string, std::string>()) const
        {
            return false;
        }
#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string & /*uri*/) noexcept override
        {
            return cpp_dbc::unexpected(DBException("W7C41KZI8819", "Firebird support is not enabled in this build", system_utils::captureCallStack()));
        }

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string & /*host*/,
            int /*port*/,
            const std::string & /*database*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("07NET9XGT519", "Firebird support is not enabled in this build", system_utils::captureCallStack()));
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*uri*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("S0U4V6W2X8Y6", "Firebird support is not enabled in this build"));
        }

        [[nodiscard]] cpp_dbc::expected<int, DBException>
        command(std::nothrow_t, const std::map<std::string, std::any> &) noexcept override
        {
            return 0;
        }

        [[nodiscard]] cpp_dbc::expected<bool, DBException>
        createDatabase(std::nothrow_t,
                       const std::string &,
                       const std::string &,
                       const std::string &,
                       const std::map<std::string, std::string> & = std::map<std::string, std::string>()) const noexcept
        {
            return false;
        }

        std::string getName() const noexcept override
        {
            return "firebird/disabled";
        }

        std::string getURIScheme() const noexcept override
        {
            return "cpp_dbc:firebird://<host>:<port>/<database_server_path>";
        }

        std::string getDriverVersion() const noexcept override
        {
            return "unknown";
        }
    };
} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
