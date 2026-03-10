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
 @brief MySQL database driver implementation - MySQLDBDriver class (constructor, destructor, all methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#include <array>
#include <charconv>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

#if USE_MYSQL

namespace cpp_dbc::MySQL
{

    // Static member initialization
    std::atomic<bool> MySQLDBDriver::s_initialized{false}; // NOSONAR(cpp:S6012) — out-of-line static member definition must match header declaration type
    std::atomic<size_t> MySQLDBDriver::s_liveConnectionCount{0};
    std::mutex MySQLDBDriver::s_initMutex;

    // ============================================================================
    // MySQLDBDriver Implementation - Private Static Initializer
    // ============================================================================

    cpp_dbc::expected<bool, DBException> MySQLDBDriver::initialize(std::nothrow_t) noexcept
    {
        // Fast path: already initialized (acquire load for visibility)
        if (s_initialized.load(std::memory_order_acquire))
        {
            return true;
        }

        // Slow path: take lock and check again (double-checked locking)
        std::scoped_lock lock(s_initMutex);
        if (s_initialized.load(std::memory_order_acquire))
        {
            return true;
        }

        if (mysql_library_init(0, nullptr, nullptr) != 0)
        {
            return cpp_dbc::unexpected(DBException("7PEJDIPGKZ1Q",
                                                   "Failed to initialize MySQL library",
                                                   system_utils::captureCallStack()));
        }

        s_initialized.store(true, std::memory_order_release);
        return true;
    }

    // ============================================================================
    // MySQLDBDriver Implementation - Constructor + Destructor
    // ============================================================================

    MySQLDBDriver::MySQLDBDriver()
    {
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    // Intentionally defaulted — cleanup is done via the static cleanup() method
    // to allow re-initialization after driver destruction.
    MySQLDBDriver::~MySQLDBDriver() = default;

    void MySQLDBDriver::cleanup()
    {
        std::scoped_lock lock(s_initMutex);
        if (!s_initialized.load(std::memory_order_acquire))
        {
            return;
        }

        auto liveCount = s_liveConnectionCount.load(std::memory_order_acquire);
        if (liveCount > 0)
        {
            MYSQL_DEBUG("MySQLDBDriver::cleanup - Skipped: %zu live connection(s) still open", liveCount);
            return;
        }

        mysql_library_end();
        s_initialized.store(false, std::memory_order_release);
    }

#ifdef __cpp_exceptions
    std::shared_ptr<RelationalDBConnection> MySQLDBDriver::connectRelational(const std::string &uri,
                                                                             const std::string &user,
                                                                             const std::string &password,
                                                                             const std::map<std::string, std::string> &options)
    {
        auto result = connectRelational(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }
#endif // __cpp_exceptions

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> MySQLDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        if (!uri.starts_with("cpp_dbc:mysql://"))
        {
            return cpp_dbc::unexpected(DBException("6MF2P9L2JN8K",
                                                   "Invalid MySQL URI scheme: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // Use centralized URI parsing from system_utils
        // MySQL URIs: cpp_dbc:mysql://host:port/database
        // Also supports IPv6: cpp_dbc:mysql://[::1]:port/database
        constexpr int DEFAULT_MYSQL_PORT = 3306;

        system_utils::ParsedDBURI parsed;
        if (!system_utils::parseDBURI(uri, "cpp_dbc:mysql://", DEFAULT_MYSQL_PORT, parsed,
                                      false,  // allowLocalConnection
                                      false)) // requireDatabase (MySQL allows no database)
        {
            MYSQL_DEBUG("MySQLDBDriver::parseURI - Failed to parse URI: %s", uri.c_str());
            return cpp_dbc::unexpected(DBException("WIB2CEJME4IV",
                                                   "Failed to parse MySQL URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        return std::map<std::string, std::string>{
            {"host", parsed.host},
            {"port", std::to_string(parsed.port)},
            {"database", parsed.database}};
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        if (host.empty())
        {
            return cpp_dbc::unexpected(DBException("619988OTKYA2",
                                                    "Cannot build MySQL URI: host is required",
                                                    system_utils::captureCallStack()));
        }

        return system_utils::buildDBURI("cpp_dbc:mysql://", host, port, database);
    }

    // Nothrow API implementations

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> MySQLDBDriver::connectRelational(
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
        std::string host = parsed["host"];

        auto &portStr = parsed["port"];
        int port = 0;
        auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
        if (ec != std::errc{} || ptr != portStr.data() + portStr.size())
        {
            return cpp_dbc::unexpected(DBException("IYLFP3EHABUY",
                                                   "Invalid port number in URI: " + portStr,
                                                   system_utils::captureCallStack()));
        }

        std::string database = parsed["database"];

        auto connResult = MySQLDBConnection::create(std::nothrow, host, port, database, user, password, options);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        return std::shared_ptr<RelationalDBConnection>(connResult.value());
    }

    std::string MySQLDBDriver::getName() const noexcept
    {
        return "mysql";
    }

    std::string MySQLDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:mysql://<host>:<port>/<database>";
    }

    std::string MySQLDBDriver::getDriverVersion() const noexcept
    {
        return mysql_get_client_info();
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
