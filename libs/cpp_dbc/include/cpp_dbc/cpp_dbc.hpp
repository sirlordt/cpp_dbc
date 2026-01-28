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

 @file cpp_dbc.hpp
 @brief Main header for the cpp_dbc library - Multi-paradigm database connectivity

*/

#ifndef CPP_DBC_HPP
#define CPP_DBC_HPP

// Configuration macros for conditional compilation
#ifndef USE_MYSQL
#define USE_MYSQL 1 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 1 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_SQLITE
#define USE_SQLITE 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_MONGODB
#define USE_MONGODB 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_SCYLLADB
#define USE_SCYLLADB 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_REDIS
#define USE_REDIS 0 // NOSONAR - Macro required for conditional compilation
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <any>
#include <optional>

// Core headers
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/streams.hpp"
#include "cpp_dbc/core/db_result_set.hpp"
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_driver.hpp"

// Relational database headers
#include "cpp_dbc/core/relational/relational_db_result_set.hpp"
#include "cpp_dbc/core/relational/relational_db_prepared_statement.hpp"
#include "cpp_dbc/core/relational/relational_db_connection.hpp"
#include "cpp_dbc/core/relational/relational_db_driver.hpp"

// Common utilities
#include "cpp_dbc/backward.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// Driver implementations (conditionally included)
#if USE_MYSQL
#include "cpp_dbc/drivers/relational/driver_mysql.hpp"
#endif

#if USE_POSTGRESQL
#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"
#endif

#if USE_SQLITE
#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#endif

#if USE_FIREBIRD
#include "cpp_dbc/drivers/relational/driver_firebird.hpp"
#endif

#if USE_MONGODB
#include "cpp_dbc/drivers/document/driver_mongodb.hpp"
#endif

#if USE_SCYLLADB
#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"
#endif

#if USE_REDIS
#include "cpp_dbc/drivers/kv/driver_redis.hpp"
#endif

// Forward declaration of configuration classes
namespace cpp_dbc::config
{
    class DatabaseConfig;
    class DatabaseConfigManager;
}

namespace cpp_dbc
{

    // Manager class to retrieve driver instances
    class DriverManager
    {
    private:
        static std::map<std::string, std::shared_ptr<DBDriver>> drivers;

    public:
        static void registerDriver(std::shared_ptr<DBDriver> driver);
        static void registerDriver(const std::string &name, std::shared_ptr<DBDriver> driver);

        // Generic connection method - returns base DBConnection
        static std::shared_ptr<DBConnection> getDBConnection(const std::string &url,
                                                             const std::string &user,
                                                             const std::string &password,
                                                             const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

        // Methods for integration with configuration classes
        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfig &dbConfig);

        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfigManager &configManager,
                                                             const std::string &configName);

        // Get list of registered driver names
        static std::vector<std::string> getRegisteredDrivers();

        // Clear all registered drivers (useful for testing)
        static void clearDrivers();

        // Unregister a specific driver
        static void unregisterDriver(const std::string &name);

        // Get a driver by name
        static std::optional<std::shared_ptr<DBDriver>> getDriver(const std::string &name);
    };

} // namespace cpp_dbc

#endif // CPP_DBC_HPP
