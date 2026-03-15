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

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <iostream>
#include <vector>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

#if USE_MYSQL

namespace cpp_dbc::MySQL
{

    // ── Static member initialization ──────────────────────────────────────────
    // IMPORTANT: s_instance MUST be declared LAST. Static file-scope variables
    // are destroyed in reverse declaration order. s_instance holds the driver
    // alive (shared_ptr singleton), and its destructor calls closeAllOpenConnections()
    // which needs s_registryMutex and s_connectionRegistry to be alive.
    // If s_instance were first, it would be destroyed last — after the mutex
    // and registry are already gone → undefined behavior.
    std::mutex MySQLDBDriver::s_instanceMutex;
    std::mutex MySQLDBDriver::s_registryMutex;
    std::set<std::weak_ptr<MySQLDBConnection>,
             std::owner_less<std::weak_ptr<MySQLDBConnection>>>
        MySQLDBDriver::s_connectionRegistry;
    std::shared_ptr<MySQLDBDriver> MySQLDBDriver::s_instance;

    // ============================================================================
    // MySQLDBDriver Implementation - Private Static Helpers
    // ============================================================================

    // Note: MySQL does NOT use the double-checked locking pattern (s_initialized + s_initMutex)
    // that other drivers (MongoDB, ScyllaDB, Redis) use. The reason is that mysql_library_init()
    // is idempotent (safe to call multiple times) and mysql_library_end() must be called
    // unconditionally in the destructor for Valgrind to report zero leaks. When using
    // double-checked locking, mysql_library_end() was guarded by s_initialized, which caused
    // Valgrind to report 56 bytes "possibly lost" from mysql_server_init() — the internal
    // allocation made by mysql_init() was not freed because the guard prevented
    // mysql_library_end() from executing during static destruction.
    cpp_dbc::expected<bool, DBException> MySQLDBDriver::initialize(std::nothrow_t) noexcept
    {
        if (mysql_library_init(0, nullptr, nullptr) != 0)
        {
            return cpp_dbc::unexpected(DBException("7PEJDIPGKZ1Q",
                                                   "Failed to initialize MySQL library",
                                                   system_utils::captureCallStack()));
        }

        return true;
    }

    void MySQLDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<MySQLDBConnection> conn) noexcept
    {
        size_t registrySize = 0;
        {
            std::scoped_lock lock(s_registryMutex);
            s_connectionRegistry.insert(std::move(conn));
            registrySize = s_connectionRegistry.size();
        }

        // Coalesced cleanup: only post when the registry has grown past the
        // cleanup threshold and no cleanup task is already queued.
        if (registrySize > 25 && !s_cleanupPending.exchange(true, std::memory_order_acq_rel))
        {
            SerialQueue::global().post([]()
                                       {
                {
                    std::scoped_lock lock(s_registryMutex);
                    std::erase_if(s_connectionRegistry,
                        [](const auto &w) { return w.expired(); });
                }
                s_cleanupPending.store(false, std::memory_order_release); });
        }
    }

    void MySQLDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<MySQLDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    // See initialize() comment above for why cleanup() calls mysql_library_end()
    // unconditionally without s_initialized guard.
    void MySQLDBDriver::cleanup()
    {
        mysql_library_end();
    }

    void MySQLDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_release);

        // Close all open connections before releasing library resources.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<MySQLDBConnection>> connectionsToClose;
        {
            std::scoped_lock lock(s_registryMutex);
            for (const auto &weak : s_connectionRegistry)
            {
                auto conn = weak.lock();
                if (conn)
                {
                    connectionsToClose.push_back(std::move(conn));
                }
            }
            s_connectionRegistry.clear();
        }
        for (const auto &conn : connectionsToClose)
        {
            [[maybe_unused]] auto closeResult = conn->close(std::nothrow);
        }
    }

    // ============================================================================
    // MySQLDBDriver Implementation - Constructor + Destructor
    // ============================================================================

    MySQLDBDriver::MySQLDBDriver(MySQLDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(result.error()));
        }
    }

    MySQLDBDriver::~MySQLDBDriver()
    {
        MYSQL_DEBUG("MySQLDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup();
    }

    // ============================================================================
    // MySQLDBDriver Implementation - Singleton + Throwing API
    // ============================================================================

#ifdef __cpp_exceptions
    std::shared_ptr<MySQLDBDriver> MySQLDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

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

    // ============================================================================
    // MySQLDBDriver Implementation - Singleton (nothrow) + Connection Registry
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<MySQLDBDriver>, DBException>
    MySQLDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        if (s_instance)
        {
            return s_instance;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // Constructor errors are captured in m_initFailed / m_initError.
        auto inst = std::make_shared<MySQLDBDriver>(MySQLDBDriver::PrivateCtorTag{}, std::nothrow);
        if (inst->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*inst->m_initError));
        }
        s_instance = inst;
        return s_instance;
    }

    size_t MySQLDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::ranges::count_if(
            s_connectionRegistry,
            [](const auto &w)
            { return !w.expired(); }));
    }

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
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("W3KFMB9NX2TP",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

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
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        return std::shared_ptr<RelationalDBConnection>(conn);
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
