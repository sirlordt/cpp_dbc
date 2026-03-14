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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file driver.hpp
 * @brief ScyllaDB driver class (real and stub implementations)
 */

#pragma once

#include "../../../cpp_dbc.hpp"

#include <map>
#include <string>
#include <memory>

#include "cpp_dbc/core/columnar/columnar_db_driver.hpp"
#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/db_exception.hpp"

#if USE_SCYLLADB

#include <atomic>
#include <mutex>
#include <set>

namespace cpp_dbc::ScyllaDB
{
    class ScyllaDBConnection; // forward declaration for registry

    /**
     * @brief ScyllaDB database driver implementation — singleton
     *
     * Concrete ColumnarDBDriver for ScyllaDB/Cassandra databases using the
     * Cassandra C/C++ driver. Handles connection establishment, URI parsing,
     * and driver registration with the DriverManager.
     * Accepts URLs in the format `cpp_dbc:scylladb://host:port/keyspace`.
     *
     * ```cpp
     * auto driver = cpp_dbc::ScyllaDB::ScyllaDBDriver::getInstance();
     * auto conn = driver->connectColumnar(
     *     "cpp_dbc:scylladb://localhost:9042/mykeyspace", "", "");
     * ```
     *
     * @see ColumnarDBDriver, ScyllaDBConnection
     */
    class ScyllaDBDriver final : public cpp_dbc::ColumnarDBDriver
    {
        // ── PrivateCtorTag — prevents direct construction; use getInstance() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // Note: Using atomic<bool> + mutex instead of std::once_flag because
        // std::once_flag cannot be reset, but we need cleanup() to allow
        // re-initialization on subsequent driver construction.
        // Also, std::call_once can throw std::system_error, which is incompatible
        // with -fno-exceptions builds.
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

        // ── Singleton state ───────────────────────────────────────────────────
        static std::weak_ptr<ScyllaDBDriver> s_instance;
        static std::mutex                    s_instanceMutex;

        // ── Connection registry ───────────────────────────────────────────────
        static std::mutex                                                        s_registryMutex;
        static std::set<std::weak_ptr<ScyllaDBConnection>,
                        std::owner_less<std::weak_ptr<ScyllaDBConnection>>>      s_connectionRegistry;

        static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;

        static void registerConnection(std::nothrow_t, std::weak_ptr<ScyllaDBConnection> conn) noexcept;
        static void unregisterConnection(std::nothrow_t, const std::weak_ptr<ScyllaDBConnection> &conn) noexcept;

        friend class ScyllaDBConnection;

        void closeAllOpenConnections(std::nothrow_t) noexcept;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Driver state ──────────────────────────────────────────────────────
        // Set to true by the destructor before releasing resources.
        // Prevents new connection attempts during and after driver teardown.
        std::atomic<bool> m_closed{false};

    public:
        ScyllaDBDriver(PrivateCtorTag, std::nothrow_t) noexcept;
        ~ScyllaDBDriver() override;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        /**
         * @brief Return the singleton ScyllaDBDriver instance, creating it if necessary.
         * @throws DBException if library initialization fails.
         */
        static std::shared_ptr<ScyllaDBDriver> getInstance();

        std::shared_ptr<ColumnarDBConnection> connectColumnar(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Return the singleton ScyllaDBDriver instance, creating it if necessary.
         * @return The shared instance, or an error if initialization fails.
         */
        static cpp_dbc::expected<std::shared_ptr<ScyllaDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;

        std::string getURIScheme() const noexcept override;
        bool supportsClustering() const noexcept override;
        bool supportsAsync() const noexcept override;
        std::string getDriverVersion() const noexcept override;
        std::string getName() const noexcept override;

        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &uri) noexcept override;

        cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        static void cleanup();

        /**
         * @brief Return the number of live connections tracked by the registry.
         *
         * Counts non-expired entries in the static connection registry.
         */
        static size_t getConnectionAlive() noexcept;
    };
} // namespace cpp_dbc::ScyllaDB

#else // USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
    class ScyllaDBDriver final : public ColumnarDBDriver
    {
    public:
        ScyllaDBDriver() = default; // Driver not enabled in this build
        ~ScyllaDBDriver() override = default;

        ScyllaDBDriver(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver &operator=(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver(ScyllaDBDriver &&) = delete;
        ScyllaDBDriver &operator=(ScyllaDBDriver &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        using DBDriver::parseURI;
        using DBDriver::buildURI;

        std::shared_ptr<ColumnarDBConnection> connectColumnar(const std::string &uri,
                                                              const std::string &user,
                                                              const std::string &password,
                                                              const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto r = connectColumnar(std::nothrow, uri, user, password, options);
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

        std::string getURIScheme() const noexcept override { return "cpp_dbc:scylladb://<host>:<port>/<keyspace>"; }
        bool supportsClustering() const noexcept override { return false; }
        bool supportsAsync() const noexcept override { return false; }
        std::string getDriverVersion() const noexcept override { return "0.0.0"; }

        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(std::nothrow_t, const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("J48UI1C472FF", "ScyllaDB support is not enabled in this build"));
        }
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &) noexcept override
        {
            return cpp_dbc::unexpected(DBException("7PPDEW842J3I", "ScyllaDB support is not enabled in this build"));
        }
        cpp_dbc::expected<std::string, DBException> buildURI(std::nothrow_t, const std::string &, int, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("0HFIU6XBM1LY", "ScyllaDB support is not enabled in this build"));
        }

        std::string getName() const noexcept override
        {
            return "scylladb/disabled";
        }
    };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
