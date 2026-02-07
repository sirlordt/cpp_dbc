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
 * @file 21_051_example_postgresql_json.cpp
 * @brief PostgreSQL-specific example demonstrating JSON/JSONB operations
 *
 * This example demonstrates:
 * - PostgreSQL JSONB operations (->>, ->, ?, ||, jsonb_set, etc.)
 * - Inserting and querying JSON data
 * - Filtering based on JSON values
 * - Modifying JSON documents
 * - JSON path queries (PostgreSQL 12+)
 *
 * Usage:
 *   ./21_051_example_postgresql_json [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - PostgreSQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_POSTGRESQL
// Helper function to print JSON query results
void printJsonResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    // Print header
    std::ostringstream headerOss;
    for (const auto &column : columnNames)
    {
        headerOss << std::setw(20) << std::left << column;
    }
    logData(headerOss.str());

    // Print separator
    std::ostringstream sepOss;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepOss << std::string(20, '-');
    }
    logData(sepOss.str());

    // Print data
    while (rs->next())
    {
        std::ostringstream rowOss;
        for (const auto &column : columnNames)
        {
            rowOss << std::setw(20) << std::left << rs->getString(column);
        }
        logData(rowOss.str());
    }
    log("");
}

// Function to demonstrate JSON operations with PostgreSQL
void demonstratePostgreSQLJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("=== PostgreSQL JSON Operations ===");
    log("");

    try
    {
        // Create a table with a JSONB column (preferred over JSON in PostgreSQL)
        conn->executeUpdate("DROP TABLE IF EXISTS user_profiles");
        conn->executeUpdate(
            "CREATE TABLE user_profiles ("
            "id INT PRIMARY KEY, "
            "profile JSONB"
            ")");

        logOk("Table created successfully");

        // Insert JSON data using prepared statements
        auto pstmt = conn->prepareStatement(
            "INSERT INTO user_profiles (id, profile) VALUES (?, ?::jsonb)");

        // User profile 1
        pstmt->setInt(1, 1);
        pstmt->setString(2, R"({
            "username": "johndoe",
            "email": "john@example.com",
            "age": 32,
            "interests": ["programming", "hiking", "photography"],
            "address": {
                "city": "San Francisco",
                "state": "CA",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        // User profile 2
        pstmt->setInt(1, 2);
        pstmt->setString(2, R"({
            "username": "janedoe",
            "email": "jane@example.com",
            "age": 28,
            "interests": ["design", "travel", "cooking"],
            "address": {
                "city": "New York",
                "state": "NY",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        // User profile 3
        pstmt->setInt(1, 3);
        pstmt->setString(2, R"({
            "username": "bobsmith",
            "email": "bob@example.com",
            "age": 45,
            "interests": ["gardening", "woodworking", "hiking"],
            "address": {
                "city": "Seattle",
                "state": "WA",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        logOk("Data inserted successfully");

        // Example 1: Extract specific JSON fields (PostgreSQL syntax)
        log("");
        logStep("Example 1: Extracting specific JSON fields");
        auto rs = conn->executeQuery(
            "SELECT id, "
            "profile->>'username' AS username, "
            "profile->>'email' AS email, "
            "profile->>'age' AS age, "
            "profile->'address'->>'city' AS city "
            "FROM user_profiles");
        printJsonResults(rs);

        // Example 2: Filter based on JSON values
        logStep("Example 2: Filtering based on JSON values");
        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, profile->>'age' AS age "
            "FROM user_profiles "
            "WHERE (profile->>'age')::int > 30");
        printJsonResults(rs);

        // Example 3: Check if JSON array contains a value
        logStep("Example 3: Checking if JSON array contains a value");
        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, "
            "profile->'interests' AS interests, "
            "profile->'interests' ? 'hiking' AS likes_hiking "
            "FROM user_profiles");
        printJsonResults(rs);

        // Example 4: Modify JSON data
        logStep("Example 4: Modifying JSON data");
        conn->executeUpdate(
            "UPDATE user_profiles "
            "SET profile = profile || '{\"premium_member\": true, \"age\": 33}'::jsonb "
            "WHERE id = 1");

        rs = conn->executeQuery("SELECT id, profile FROM user_profiles WHERE id = 1");
        printJsonResults(rs);

        // Example 5: Add elements to JSON array
        logStep("Example 5: Adding elements to JSON array");
        conn->executeUpdate(
            "UPDATE user_profiles "
            "SET profile = jsonb_set(profile, '{interests}', "
            "profile->'interests' || '\"music\"'::jsonb) "
            "WHERE id = 2");

        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, "
            "profile->'interests' AS interests "
            "FROM user_profiles WHERE id = 2");
        printJsonResults(rs);

        // Example 6: JSON path queries (PostgreSQL 12+)
        logStep("Example 6: JSON path queries (PostgreSQL 12+)");
        try
        {
            rs = conn->executeQuery(
                "SELECT id, profile->>'username' AS username, "
                "jsonb_path_query_array(profile, '$.interests[*]') AS interest_list "
                "FROM user_profiles");
            printJsonResults(rs);
        }
        catch (const cpp_dbc::DBException &e)
        {
            logInfo("JSON path query not supported in this PostgreSQL version");
        }

        // Clean up
        conn->executeUpdate("DROP TABLE user_profiles");
        logOk("Table dropped successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("PostgreSQL JSON operation error: " + e.what_s());
        throw;
    }
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc PostgreSQL JSON Operations Example");
    log("========================================");
    log("");

#if !USE_POSTGRESQL
    logError("PostgreSQL support is not enabled");
    logInfo("Build with -DUSE_POSTGRESQL=ON to enable PostgreSQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,postgres");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("21_051_example_postgresql_json", "postgresql");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting PostgreSQL database configuration...");
    auto pgResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "postgresql");

    if (!pgResult)
    {
        logError("Failed to get PostgreSQL config: " + pgResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!pgResult.value())
    {
        logError("PostgreSQL configuration not found");
        return EXIT_ERROR_;
    }

    auto &pgConfig = *pgResult.value();
    logOk("Using database: " + pgConfig.getName() +
          " (" + pgConfig.getType() + "://" +
          pgConfig.getHost() + ":" + std::to_string(pgConfig.getPort()) +
          "/" + pgConfig.getDatabase() + ")");

    logStep("Registering PostgreSQL driver...");
    registerDriver("postgresql");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to PostgreSQL...");
        auto pgConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(
                pgConfig.createConnectionString(),
                pgConfig.getUsername(),
                pgConfig.getPassword()));

        logOk("Connected to PostgreSQL");

        // Demonstrate PostgreSQL JSON operations
        demonstratePostgreSQLJson(pgConn);

        logStep("Closing PostgreSQL connection...");
        pgConn->close();
        logOk("PostgreSQL connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("PostgreSQL error: " + e.what_s());
        return EXIT_ERROR_;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
