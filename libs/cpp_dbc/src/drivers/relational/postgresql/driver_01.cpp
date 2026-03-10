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

#include <array>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

#if USE_POSTGRESQL

namespace cpp_dbc::PostgreSQL
{

    // Static member initialization
    std::atomic<size_t> PostgreSQLDBDriver::s_liveConnectionCount{0};

    // PostgreSQLDBDriver implementation
    PostgreSQLDBDriver::PostgreSQLDBDriver() = default;

    PostgreSQLDBDriver::~PostgreSQLDBDriver()
    {
        // Sleep a bit to ensure all resources are properly released
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void PostgreSQLDBDriver::cleanup()
    {
        // PostgreSQL (libpq) does not require global library init/cleanup like MySQL.
        // Each PGconn is self-contained. This method exists for API symmetry with
        // other drivers and to guard against premature driver destruction while
        // connections are still alive.
        auto liveCount = s_liveConnectionCount.load(std::memory_order_acquire);
        if (liveCount > 0)
        {
            PG_DEBUG("PostgreSQLDBDriver::cleanup - Skipped: "
                     << liveCount
                     << " live connection(s) still open");
            return;
        }
    }

#ifdef __cpp_exceptions
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

    // Nothrow API implementation
    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> PostgreSQLDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
            auto parseResult = parseURI(std::nothrow, uri);
            if (!parseResult.has_value())
            {
                return cpp_dbc::unexpected(parseResult.error());
            }

            auto &parsed = parseResult.value();
            const std::string &host = parsed["host"];
            int port = std::stoi(parsed["port"]);
            const std::string &database = parsed["database"];

            auto connection = std::make_shared<PostgreSQLDBConnection>(host, port, database, user, password, options);
            return cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>(std::static_pointer_cast<RelationalDBConnection>(connection));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2A3B4C5D6E7F", std::string("Exception in connectRelational: ") + ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8G9H0I1J2K3L", "Unknown exception in connectRelational", system_utils::captureCallStack()));
        }
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
