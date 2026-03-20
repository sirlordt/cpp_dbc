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

 @file driver_01.cpp
 @brief Firebird database driver implementation - FirebirdDBDriver class (static members, constructor, destructor, all methods)

 Required system package: firebird-dev (Debian/Ubuntu) or firebird-devel (RHEL/CentOS/Fedora)
 Install with: sudo apt-get install firebird-dev libfbclient2

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

#include "cpp_dbc/common/system_utils.hpp"
#include "firebird_internal.hpp"

#if USE_FIREBIRD

namespace cpp_dbc::Firebird
{
    // ── Static member initialization ──────────────────────────────────────────
    // IMPORTANT: s_instance MUST be declared LAST — see mysql/driver_01.cpp for
    // the full explanation of the static destruction order requirement.
    std::mutex                        FirebirdDBDriver::s_instanceMutex;
    std::mutex                        FirebirdDBDriver::s_registryMutex;
    std::set<std::weak_ptr<FirebirdDBConnection>,
             std::owner_less<std::weak_ptr<FirebirdDBConnection>>> FirebirdDBDriver::s_connectionRegistry;
    std::atomic<bool> FirebirdDBDriver::s_cleanupPending{false};
    std::shared_ptr<FirebirdDBDriver> FirebirdDBDriver::s_instance;

    // ============================================================================
    // FirebirdDBDriver Implementation - Private Static Helpers
    // ============================================================================

    // Firebird (fbclient) does not require explicit global library initialization.
    // The initialize() method exists for structural symmetry with other drivers.
    cpp_dbc::expected<bool, DBException> FirebirdDBDriver::initialize(std::nothrow_t) noexcept
    {
        // No initialization needed for fbclient
        return true;
    }

    void FirebirdDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<FirebirdDBConnection> conn) noexcept
    {
        size_t registrySize = 0;
        {
            std::scoped_lock lock(s_registryMutex);
            s_connectionRegistry.insert(std::move(conn));
            registrySize = s_connectionRegistry.size();
        }

        // Coalesced cleanup: only post when the registry has grown past the
        // cleanup threshold and no cleanup task is already queued.
        if (registrySize > 25 && !s_cleanupPending.exchange(true, std::memory_order_seq_cst))
        {
            SerialQueue::global().post([]()
            {
                {
                    std::scoped_lock lock(s_registryMutex);
                    std::erase_if(s_connectionRegistry,
                        [](const auto &w) { return w.expired(); });
                }
                s_cleanupPending.store(false, std::memory_order_seq_cst);
            });
        }
    }

    void FirebirdDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<FirebirdDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void FirebirdDBDriver::cleanup()
    {
        // Firebird (fbclient) does not require explicit global library cleanup.
        // Sleep a bit to ensure all resources are properly released.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void FirebirdDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_seq_cst);

        // Close all open connections before releasing library resources.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<FirebirdDBConnection>> connectionsToClose;
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

    cpp_dbc::expected<FirebirdDBDriver::ParsedFirebirdComponents, DBException>
    FirebirdDBDriver::extractComponents(std::nothrow_t, const std::map<std::string, std::string> &parsed) noexcept
    {
        ParsedFirebirdComponents comps;

        auto hostIt = parsed.find("host");
        auto portIt = parsed.find("port");
        auto dbIt = parsed.find("database");

        comps.host = (hostIt != parsed.end()) ? hostIt->second : "";
        if (portIt != parsed.end())
        {
            const auto &portStr = portIt->second;
            auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), comps.port);
            if (ec != std::errc{} || ptr != portStr.data() + portStr.size())
            {
                return cpp_dbc::unexpected(DBException("9NVVV6AC2H8A",
                                                       "Invalid port number in Firebird URI: " + portStr,
                                                       system_utils::captureCallStack()));
            }
        }
        comps.database = (dbIt != parsed.end()) ? dbIt->second : "";

        if (comps.database.empty())
        {
            return cpp_dbc::unexpected(DBException("YU88W61QFVD0",
                                                   "Invalid Firebird URI: database path is empty",
                                                   system_utils::captureCallStack()));
        }

        return comps;
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Constructor + Destructor
    // ============================================================================

    FirebirdDBDriver::FirebirdDBDriver(FirebirdDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(result.error()));
        }
    }

    FirebirdDBDriver::~FirebirdDBDriver()
    {
        FIREBIRD_DEBUG("FirebirdDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup();
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Singleton + Throwing API
    // ============================================================================

#ifdef __cpp_exceptions
    std::shared_ptr<FirebirdDBDriver> FirebirdDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBConnection> FirebirdDBDriver::connectRelational(const std::string &uri,
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

    int FirebirdDBDriver::command(const std::map<std::string, std::any> &params)
    {
        auto result = command(std::nothrow, params);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBDriver::createDatabase(const std::string &uri,
                                          const std::string &user,
                                          const std::string &password,
                                          const std::map<std::string, std::string> &options)
    {
        auto result = createDatabase(std::nothrow, uri, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ============================================================================
    // FirebirdDBDriver Implementation - Singleton (nothrow) + Connection Registry
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<FirebirdDBDriver>, DBException>
    FirebirdDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        if (s_instance)
        {
            return s_instance;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // Constructor errors are captured in m_initFailed / m_initError.
        auto inst = std::make_shared<FirebirdDBDriver>(FirebirdDBDriver::PrivateCtorTag{}, std::nothrow);
        if (inst->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*inst->m_initError));
        }
        s_instance = inst;
        return s_instance;
    }

    // ============================================================================
    // FirebirdDBDriver Implementation - Nothrow API
    // ============================================================================

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> FirebirdDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        if (!uri.starts_with("cpp_dbc:firebird://"))
        {
            return cpp_dbc::unexpected(DBException("F233GET0TLGP",
                                                   "Invalid Firebird URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        // Use centralized URI parsing from system_utils
        // Firebird URIs:
        // - cpp_dbc:firebird://host:port/path/to/database.fdb
        // - cpp_dbc:firebird://host/path/to/database.fdb
        // - cpp_dbc:firebird:///path/to/database.fdb (local)
        // - cpp_dbc:firebird://[::1]:port/path/to/database.fdb (IPv6)
        constexpr int DEFAULT_FIREBIRD_PORT = 3050;

        system_utils::ParsedDBURI parsed;
        if (!system_utils::parseDBURI(uri, "cpp_dbc:firebird://", DEFAULT_FIREBIRD_PORT, parsed,
                                      true,  // allowLocalConnection
                                      true)) // requireDatabase
        {
            FIREBIRD_DEBUG("FirebirdDBDriver::parseURI - Failed to parse URI: %s", uri.c_str());
            return cpp_dbc::unexpected(DBException("H9I5J1K7L3M9",
                                                   "Failed to parse Firebird URI: " + uri,
                                                   system_utils::captureCallStack()));
        }

        std::string database;
        // For Firebird, the database path needs to include the leading slash
        if (parsed.isLocal)
        {
            // Local connection: database already has the full path with leading slash
            database = parsed.database;
        }
        else
        {
            // Remote connection: prepend slash to make it a path
            database = "/" + parsed.database;
        }

        std::map<std::string, std::string> result;
        result["host"] = parsed.host;
        result["port"] = std::to_string(parsed.port);
        result["database"] = database;
        return result;
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> & /*options*/) noexcept
    {
        if (database.empty())
        {
            return cpp_dbc::unexpected(DBException("J31TMK3L7IK3",
                                                   "Cannot build Firebird URI: database path is empty",
                                                   system_utils::captureCallStack()));
        }

        // Build: cpp_dbc:firebird://host:port/database
        // Bracket raw IPv6 hosts (e.g. "::1" → "[::1]")
        constexpr int DEFAULT_FIREBIRD_PORT = 3050;
        std::string uri = "cpp_dbc:firebird://";
        if (!host.empty())
        {
            if (host.contains(':') &&
                !(host.front() == '[' && host.back() == ']'))
            {
                uri += "[" + host + "]";
            }
            else
            {
                uri += host;
            }
            if (port != 0 && port != DEFAULT_FIREBIRD_PORT)
            {
                uri += ":" + std::to_string(port);
            }
        }
        uri += database;
        return uri;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
    FirebirdDBDriver::connectRelational(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        if (m_closed.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("H8LMRV3JYD1N",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

        auto parseResult = parseURI(std::nothrow, uri);
        if (!parseResult.has_value())
        {
            return cpp_dbc::unexpected(parseResult.error());
        }

        auto compsResult = extractComponents(std::nothrow, parseResult.value());
        if (!compsResult.has_value())
        {
            return cpp_dbc::unexpected(compsResult.error());
        }

        const auto &comps = compsResult.value();
        auto connResult = FirebirdDBConnection::create(std::nothrow, comps.host, comps.port, comps.database, user, password, options);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        return std::shared_ptr<RelationalDBConnection>(conn);
    }

    cpp_dbc::expected<int, DBException>
    FirebirdDBDriver::command(std::nothrow_t, const std::map<std::string, std::any> &params) noexcept
    {
        FIREBIRD_DEBUG("FirebirdDriver::command(nothrow) - Starting");

        // Get the command name
        auto cmdIt = params.find("command");
        if (cmdIt == params.end())
        {
            return cpp_dbc::unexpected(DBException("J1K7L3M9N5O1", "Missing 'command' parameter", system_utils::captureCallStack()));
        }

        std::string cmd;
        try
        {
            cmd = std::any_cast<std::string>(cmdIt->second);
        }
        catch (const std::bad_any_cast &ex)
        {
            return cpp_dbc::unexpected(DBException("K2L8M4N0O6P2",
                                                   std::string("Invalid 'command' parameter type (expected string): ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CAB1PZ5CBMUV",
                                                   std::string("Exception reading 'command' parameter: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            // Intentionally silenced — unknown exception from std::any_cast
            return cpp_dbc::unexpected(DBException("OI8BZXUON0YS", "Unknown exception reading 'command' parameter",
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  Command: %s", cmd.c_str());

        if (cmd == "create_database")
        {
            // Extract required parameters
            std::string uri, user, password;
            std::map<std::string, std::string> options;

            auto uriIt = params.find("uri");
            if (uriIt == params.end())
            {
                uriIt = params.find("url");
            }
            if (uriIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("L3M9N5O1P7Q3", "Missing 'uri' (or 'url') parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                uri = std::any_cast<std::string>(uriIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("M4N0O6P2Q8R4",
                                                       std::string("Invalid 'uri'/'url' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("PDLWNXE5A7A0",
                                                       std::string("Exception reading 'uri'/'url' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("TT39UST5GNQV", "Unknown exception reading 'uri'/'url' parameter",
                                                       system_utils::captureCallStack()));
            }

            auto userIt = params.find("user");
            if (userIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("N5O1P7Q3R9S5", "Missing 'user' parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                user = std::any_cast<std::string>(userIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("O6P2Q8R4S0T6",
                                                       std::string("Invalid 'user' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("9MLTND7PDO1Q",
                                                       std::string("Exception reading 'user' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("XXGUSOPM99EK", "Unknown exception reading 'user' parameter",
                                                       system_utils::captureCallStack()));
            }

            auto passIt = params.find("password");
            if (passIt == params.end())
            {
                return cpp_dbc::unexpected(DBException("P7Q3R9S5T1U7", "Missing 'password' parameter for create_database",
                                                       system_utils::captureCallStack()));
            }
            try
            {
                password = std::any_cast<std::string>(passIt->second);
            }
            catch (const std::bad_any_cast &ex)
            {
                return cpp_dbc::unexpected(DBException("Q8R4S0T6U2V8",
                                                       std::string("Invalid 'password' parameter type: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("D04AODLJWCCT",
                                                       std::string("Exception reading 'password' parameter: ") + ex.what(),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
            {
                // Intentionally silenced — unknown exception from std::any_cast
                return cpp_dbc::unexpected(DBException("7IJURPQLPQ9R", "Unknown exception reading 'password' parameter",
                                                       system_utils::captureCallStack()));
            }

            // Extract optional parameters
            auto pageSizeIt = params.find("page_size");
            if (pageSizeIt != params.end())
            {
                try
                {
                    options["page_size"] = std::any_cast<std::string>(pageSizeIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (const std::exception &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
                {
                    // Intentionally silenced — ignore invalid optional type
                }
            }

            auto charsetIt = params.find("charset");
            if (charsetIt != params.end())
            {
                try
                {
                    options["charset"] = std::any_cast<std::string>(charsetIt->second);
                }
                catch (const std::bad_any_cast &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (const std::exception &)
                {
                    // Intentionally silenced — ignore invalid optional type
                }
                catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
                {
                    // Intentionally silenced — ignore invalid optional type
                }
            }

            auto dbResult = createDatabase(std::nothrow, uri, user, password, options);
            if (!dbResult.has_value())
            {
                return cpp_dbc::unexpected(dbResult.error());
            }
            return 0;
        }
        else
        {
            return cpp_dbc::unexpected(DBException("R9S5T1U7V3W9", "Unknown command: " + cmd, system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBDriver::createDatabase(std::nothrow_t,
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

        auto compsResult = extractComponents(std::nothrow, parseResult.value());
        if (!compsResult.has_value())
        {
            return cpp_dbc::unexpected(compsResult.error());
        }

        const auto &comps = compsResult.value();
        const auto &host = comps.host;
        int port = comps.port;
        const auto &database = comps.database;

        // Build the Firebird connection string for CREATE DATABASE
        std::string fbConnStr;
        if (!host.empty() && host != "localhost" && host != "127.0.0.1")
        {
            fbConnStr = host;
            if (port != 3050 && port != 0)
            {
                fbConnStr += "/" + std::to_string(port);
            }
            fbConnStr += ":";
        }
        fbConnStr += database;

        // Get page size from options
        std::string pageSize = "4096";
        auto it = options.find("page_size");
        if (it != options.end())
        {
            pageSize = it->second;
        }

        // Get charset from options
        std::string charset = "UTF8";
        it = options.find("charset");
        if (it != options.end())
        {
            charset = it->second;
        }

        // Build CREATE DATABASE SQL command
        std::string createDbSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                    "USER '" +
                                  user + "' "
                                         "PASSWORD '" +
                                  password + "' "
                                             "PAGE_SIZE " +
                                  pageSize + " "
                                             "DEFAULT CHARACTER SET " +
                                  charset;

        FIREBIRD_DEBUG("FirebirdDriver::createDatabase(nothrow) - Executing: %s", createDbSql.c_str());

        ISC_STATUS_ARRAY status;
        isc_db_handle db = 0;
        isc_tr_handle tr = 0;

        // Execute CREATE DATABASE using isc_dsql_execute_immediate
        if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  Failed to create database: %s", errorMsg.c_str());
            return cpp_dbc::unexpected(DBException("I0J6K2L8M4N0", "Failed to create database: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        FIREBIRD_DEBUG("  Database created successfully!");

        // Detach from the newly created database
        if (db)
        {
            isc_detach_database(status, &db);
        }

        return true;
    }

    std::string FirebirdDBDriver::getName() const noexcept
    {
        return "firebird";
    }

    std::string FirebirdDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:firebird://<host>:<port>/<database_server_path>";
    }

    std::string FirebirdDBDriver::getDriverVersion() const noexcept
    {
#if defined(FB_API_VER)
        return std::to_string(FB_API_VER);
#else
        return "unknown";
#endif
    }

    size_t FirebirdDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::ranges::count_if(
            s_connectionRegistry,
            [](const auto &w)
            {
                return !w.expired();
            }));
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
