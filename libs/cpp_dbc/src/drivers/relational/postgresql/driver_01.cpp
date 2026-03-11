/*

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

 @file driver_01.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBDriver class (constructor, destructor, all methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <cctype>
#include <iomanip>

#include "cpp_dbc/common/system_utils.hpp"
#include "postgresql_internal.hpp"

#if USE_POSTGRESQL

namespace cpp_dbc::PostgreSQL
{

    // ── Static member initialization ──────────────────────────────────────────
    std::weak_ptr<PostgreSQLDBDriver> PostgreSQLDBDriver::s_instance;
    std::mutex                        PostgreSQLDBDriver::s_instanceMutex;
    std::mutex                        PostgreSQLDBDriver::s_registryMutex;
    std::set<std::weak_ptr<PostgreSQLDBConnection>,
             std::owner_less<std::weak_ptr<PostgreSQLDBConnection>>> PostgreSQLDBDriver::s_connectionRegistry;

    // ============================================================================
    // PostgreSQLDBDriver Implementation - Private Static Helpers
    // ============================================================================

    // PostgreSQL (libpq) does not require explicit global library initialization.
    // The initialize() method exists for structural symmetry with other drivers.
    cpp_dbc::expected<bool, DBException> PostgreSQLDBDriver::initialize(std::nothrow_t) noexcept
    {
        // No initialization needed for libpq
        return true;
    }

    void PostgreSQLDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<PostgreSQLDBConnection> conn) noexcept
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        s_connectionRegistry.insert(std::move(conn));
    }

    void PostgreSQLDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<PostgreSQLDBConnection> &conn) noexcept
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void PostgreSQLDBDriver::cleanup()
    {
        // PostgreSQL (libpq) does not require explicit global library cleanup.
        // Sleep a bit to ensure all resources are properly released.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // ============================================================================
    // PostgreSQLDBDriver Implementation - Constructor + Destructor
    // ============================================================================

    PostgreSQLDBDriver::PostgreSQLDBDriver(PostgreSQLDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(result.error()));
        }
    }

    PostgreSQLDBDriver::~PostgreSQLDBDriver()
    {
        PG_DEBUG("PostgreSQLDBDriver::destructor - Destroying driver");
        cleanup();
    }

    // ============================================================================
    // PostgreSQLDBDriver Implementation - Singleton + Throwing API
    // ============================================================================

#ifdef __cpp_exceptions
    std::shared_ptr<PostgreSQLDBDriver> PostgreSQLDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBConnection> PostgreSQLDBDriver::connectRelational(const std::string &uri,
                                                                                  const std::string &user,
                                                                                  const std::string &password,
                                                                                  const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> PostgreSQLDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        if (!uri.starts_with("cpp_dbc:postgresql://"))
        {
            return cpp_dbc::unexpected(DBException("YPN18WI6BPTS",
                                                   "Invalid PostgreSQL URI scheme: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // Use centralized URI parsing from system_utils
        // PostgreSQL URIs: cpp_dbc:postgresql://host:port/database
        // Also supports IPv6: cpp_dbc:postgresql://[::1]:port/database
        constexpr int DEFAULT_POSTGRESQL_PORT = 5432;

        system_utils::ParsedDBURI parsed;
        if (!system_utils::parseDBURI(uri, "cpp_dbc:postgresql://", DEFAULT_POSTGRESQL_PORT, parsed,
                                      false, // allowLocalConnection
                                      true)) // requireDatabase (PostgreSQL requires database)
        {
            PG_DEBUG("PostgreSQLDBDriver::parseURI - Failed to parse URI: " << uri);
            return cpp_dbc::unexpected(DBException("1P567517HBSK",
                                                   "Failed to parse PostgreSQL URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        return std::map<std::string, std::string>{
            {"host", parsed.host},
            {"port", std::to_string(parsed.port)},
            {"database", parsed.database}};
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        if (host.empty())
        {
            return cpp_dbc::unexpected(DBException("WEBDF2PNV05L",
                                                    "Cannot build PostgreSQL URI: host is required",
                                                    system_utils::captureCallStack()));
        }

        if (database.empty())
        {
            return cpp_dbc::unexpected(DBException("2U5PU3Y1GHAX",
                                                   "Cannot build PostgreSQL URI: database is required",
                                                   system_utils::captureCallStack()));
        }

        return system_utils::buildDBURI("cpp_dbc:postgresql://", host, port, database);
    }

    // ============================================================================
    // PostgreSQLDBDriver Implementation - Singleton (nothrow) + Connection Registry
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<PostgreSQLDBDriver>, DBException>
    PostgreSQLDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        auto existing = s_instance.lock();
        if (existing)
        {
            return existing;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // Constructor errors are captured in m_initFailed / m_initError.
        auto inst = std::make_shared<PostgreSQLDBDriver>(PostgreSQLDBDriver::PrivateCtorTag{}, std::nothrow);
        if (inst->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*inst->m_initError));
        }
        s_instance = inst;
        return inst;
    }

    size_t PostgreSQLDBDriver::getConnectionAlive() noexcept
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        return static_cast<size_t>(std::count_if(
            s_connectionRegistry.begin(), s_connectionRegistry.end(),
            [](const auto &w) { return !w.expired(); }));
    }

    // Nothrow API implementation
    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> PostgreSQLDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        auto parseResult = parseURI(std::nothrow, uri);
        if (!parseResult.has_value())
        {
            return cpp_dbc::unexpected(parseResult.error());
        }

        auto &parsed = parseResult.value();
        const std::string &host = parsed["host"];
        const auto &portStr = parsed["port"];
        int port = 0;
        auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
        if (ec != std::errc{} || ptr != portStr.data() + portStr.size())
        {
            return cpp_dbc::unexpected(DBException("T3A7QUX6XNUT",
                                                   "Invalid port number in URI: " + portStr,
                                                   system_utils::captureCallStack()));
        }
        const std::string &database = parsed["database"];

        auto connResult = PostgreSQLDBConnection::create(std::nothrow, host, port, database, user, password, options);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        return std::shared_ptr<RelationalDBConnection>(conn);
    }

    std::string PostgreSQLDBDriver::getName() const noexcept
    {
        return "postgresql";
    }

    std::string PostgreSQLDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:postgresql://<host>:<port>/<database>";
    }

    std::string PostgreSQLDBDriver::getDriverVersion() const noexcept
    {
        // 2026-03-08T19:00:00Z
        // Bug: PQlibVersion() encoding changed in PostgreSQL 10+. Pre-10 uses
        // major*10000 + minor*100 + patch (e.g. 9.6.3 → 90603). Post-10 uses
        // major*10000 + minor (e.g. 16.3 → 160003). Old code always applied the
        // pre-10 formula, producing "16.0.3" instead of "16.3".
        // Solution: Detect the encoding by checking if ver >= 100000 (PostgreSQL 10+)
        // and parse accordingly.
        int ver = PQlibVersion();
        if (ver >= 100000)
        {
            // PostgreSQL 10+: major*10000 + minor
            int major = ver / 10000;
            int minor = ver % 10000;
            return std::to_string(major) + "." + std::to_string(minor);
        }
        // Pre-10: major*10000 + minor*100 + patch
        int major = ver / 10000;
        int minor = (ver / 100) % 100;
        int patch = ver % 100;
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
