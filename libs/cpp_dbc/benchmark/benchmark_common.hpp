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

// Define BENCHMARK_CHECK macro (not provided by Google Benchmark)
#define BENCHMARK_CHECK(condition)                        \
    if (!(condition))                                     \
    {                                                     \
        state.SkipWithError("CHECK failed: " #condition); \
        return;                                           \
    }

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
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

    // Helper function to generate random IDs
    std::vector<int> generateRandomIds(int maxId, int count);

    // Helper function to create test tables for benchmarks
    void createBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName);

    // Helper function to drop test tables after benchmarks
    void dropBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName);

    // Helper function to populate a table with a specific number of rows
    void populateTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName, int rowCount);
}

namespace mysql_benchmark_helpers
{
#if USE_MYSQL
    // Get a MySQL database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getMySQLConfig(const std::string &databaseName = "dev_mysql");

    // Check if a connection to MySQL can be established
    bool canConnectToMySQL();

    // Helper function to setup MySQL connection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupMySQLConnection(const std::string &tableName, int rowCount = 0);
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
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupPostgreSQLConnection(const std::string &tableName, int rowCount = 0);
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
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupSQLiteConnection(const std::string &tableName, int rowCount = 0);
#endif
}

namespace firebird_benchmark_helpers
{
#if USE_FIREBIRD
    // Get a Firebird database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getFirebirdConfig(const std::string &databaseName = "dev_firebird");

    // Check if a connection to Firebird can be established
    bool canConnectToFirebird();

    // Helper function to setup Firebird connection
    std::shared_ptr<cpp_dbc::RelationalDBConnection> setupFirebirdConnection(const std::string &tableName, int rowCount = 0);

    // Helper function to create Firebird-specific benchmark table
    void createFirebirdBenchmarkTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName);

    // Helper function to populate Firebird table with test data
    void populateFirebirdTable(std::shared_ptr<cpp_dbc::RelationalDBConnection> &conn, const std::string &tableName, int rowCount);
#endif
}

namespace mongodb_benchmark_helpers
{
#if USE_MONGODB
    // Get a MongoDB database configuration based on the given name
    cpp_dbc::config::DatabaseConfig getMongoDBConfig(const std::string &databaseName = "dev_mongodb");

    // Build a MongoDB connection string from a DatabaseConfig
    std::string buildMongoDBConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig);

    // Check if a connection to MongoDB can be established
    bool canConnectToMongoDB();

    // Helper function to get a MongoDB driver instance
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBDriver> getMongoDBDriver();

    // Helper function to setup MongoDB connection
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> setupMongoDBConnection(const std::string &collectionName, int docCount = 0);

    // Helper function to create a collection for benchmarking
    void createBenchmarkCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName);

    // Helper function to drop a collection after benchmarking
    void dropBenchmarkCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName);

    // Helper function to populate a collection with test documents
    void populateCollection(std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> &conn, const std::string &collectionName, int docCount);

    // Helper function to generate a test document
    std::string generateTestDocument(int id, const std::string &name, double value, const std::string &description);

    // Helper function to generate a random collection name
    std::string generateRandomCollectionName();
#endif
}