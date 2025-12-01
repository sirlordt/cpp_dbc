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
 * @file benchmark_common.hpp
 * @brief Common utilities for cpp_dbc benchmarks
 */

#pragma once

#include <benchmark/benchmark.h>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#endif

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <unordered_map>
#include <mutex>

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

// Helper functions for config file paths
namespace common_benchmark_helpers
{
    std::string getExecutablePathAndName();
    std::string getOnlyExecutablePath();
    std::string getConfigFilePath();

    // Data sizes for benchmarks
    constexpr int SMALL_SIZE = 10;
    constexpr int MEDIUM_SIZE = 100;
    constexpr int LARGE_SIZE = 1000;
    constexpr int XLARGE_SIZE = 10000;

    // Helper function to generate random string data
    std::string generateRandomString(size_t length);

    // Helper function to create test tables for benchmarks
    void createBenchmarkTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName);

    // Helper function to drop test tables after benchmarks
    void dropBenchmarkTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName);

    // Helper function to populate a table with a specific number of rows
    void populateTable(std::shared_ptr<cpp_dbc::Connection> &conn, const std::string &tableName, int rowCount);
}

namespace mysql_benchmark_helpers
{
#if USE_MYSQL
    // Get a MySQL database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getMySQLConfig(const std::string &databaseName = "dev_mysql");

    // Check if a connection to MySQL can be established
    bool canConnectToMySQL();

    // Helper function to setup MySQL connection
    std::shared_ptr<cpp_dbc::Connection> setupMySQLConnection(const std::string &tableName, int rowCount = 0);
#endif
}

namespace postgresql_benchmark_helpers
{
#if USE_POSTGRESQL
    // Get a PostgreSQL database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getPostgreSQLConfig(const std::string &databaseName = "dev_postgresql");

    // Check if a connection to PostgreSQL can be established
    bool canConnectToPostgreSQL();

    // Helper function to setup PostgreSQL connection
    std::shared_ptr<cpp_dbc::Connection> setupPostgreSQLConnection(const std::string &tableName, int rowCount = 0);
#endif
}

namespace sqlite_benchmark_helpers
{
#if USE_SQLITE
    // Get a SQLite database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getSQLiteConfig(const std::string &databaseName = "dev_sqlite");

    // Get a connection string for SQLite
    std::string getSQLiteConnectionString();

    // Check if a connection to SQLite can be established
    bool canConnectToSQLite();

    // Helper function to setup SQLite connection
    std::shared_ptr<cpp_dbc::Connection> setupSQLiteConnection(const std::string &tableName, int rowCount = 0);
#endif
}