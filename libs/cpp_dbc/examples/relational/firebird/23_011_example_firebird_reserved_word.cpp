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
 * @file 23_011_example_firebird_reserved_word.cpp
 * @brief Example testing Firebird reserved word handling
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Testing reserved word exceptions in Firebird
 * - Using quoted identifiers
 *
 * Usage:
 *   ./firebird_reserved_word_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./firebird_reserved_word_example                    # Uses dev_firebird from default config
 *   ./firebird_reserved_word_example --db=test_firebird # Uses test_firebird configuration
 */

#include "../../common/example_common.hpp"

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_FIREBIRD
void testReservedWordException(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Test 1: Reserved Word 'value' ---");

    logStep("Attempting CREATE TABLE with reserved word 'value'...");
    logData("SQL: CREATE TABLE test_reserved (id INTEGER PRIMARY KEY, value INTEGER)");

    try
    {
        conn->executeUpdate(
            "CREATE TABLE test_reserved ("
            "id INTEGER PRIMARY KEY, "
            "value INTEGER"
            ")");

        logInfo("No exception thrown - 'value' may not be reserved in this Firebird version");

        try
        {
            conn->executeUpdate("DROP TABLE test_reserved");
            logOk("Table dropped");
        }
        catch (...)
        {
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logOk("Exception thrown as expected for reserved word");
        logData("Error: " + e.what_s());
    }
}

void testReservedWordWithQuotes(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Test 2: Quoted Identifier ---");

    logStep("Creating table with quoted 'value' column...");
    logData("SQL: CREATE TABLE test_quoted (id INTEGER PRIMARY KEY, \"value\" INTEGER)");

    try
    {
        try
        {
            conn->executeUpdate("DROP TABLE test_quoted");
        }
        catch (...)
        {
        }

        conn->executeUpdate(
            "CREATE TABLE test_quoted ("
            "id INTEGER PRIMARY KEY, "
            "\"value\" INTEGER"
            ")");
        logOk("Table created with quoted identifier");

        logStep("Inserting data...");
        auto stmt = conn->prepareStatement("INSERT INTO test_quoted (id, \"value\") VALUES (?, ?)");
        stmt->setInt(1, 1);
        stmt->setInt(2, 100);
        stmt->executeUpdate();
        stmt->close();
        logOk("Data inserted");

        logStep("Querying data...");
        auto rs = conn->executeQuery("SELECT id, \"value\" FROM test_quoted");
        while (rs->next())
        {
            // Note: Unquoted "id" becomes uppercase "ID" in Firebird; quoted "value" preserves case
            logData("Row: id=" + std::to_string(rs->getInt("ID")) +
                    ", value=" + std::to_string(rs->getInt("value")));
        }
        rs->close();
        logOk("Query completed");

        conn->commit();

        logStep("Dropping table...");
        conn->executeUpdate("DROP TABLE test_quoted");
        logOk("Table dropped");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Exception: " + e.what_s());
        try
        {
            conn->rollback();
        }
        catch (...)
        {
        }
    }
}

void testOtherReservedWords(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Test 3: Other Reserved Words ---");

    std::vector<std::string> reservedWords = {
        "VALUE", "USER", "DATE", "TIME", "TIMESTAMP", "ORDER", "GROUP"};

    for (const auto &word : reservedWords)
    {
        std::string sql = "CREATE TABLE test_" + word + " (id INTEGER PRIMARY KEY, " + word + " INTEGER)";
        logStep("Testing: " + word);

        try
        {
            conn->executeUpdate(sql);
            logData("Result: Created (not reserved or allowed)");

            try
            {
                conn->executeUpdate("DROP TABLE test_" + word);
            }
            catch (...)
            {
            }
        }
        catch (const cpp_dbc::DBException &)
        {
            logData("Result: EXCEPTION - reserved word");
        }
    }
    logOk("Reserved word tests completed");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Firebird Reserved Word Example");
    logMsg("========================================");
    logMsg("");

#if !USE_FIREBIRD
    logError("Firebird support is not enabled");
    logInfo("Build with -DUSE_FIREBIRD=ON to enable Firebird support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,firebird");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("firebird_reserved_word_example", "firebird");
        return 0;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return 1;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return 1;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "firebird");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return 1;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("Firebird configuration not found");
        return 1;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    // Try to create database if it doesn't exist
    tryCreateFirebirdDatabase(dbConfig);

    try
    {
        logStep("Connecting to Firebird...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to RelationalDBConnection");
            return 1;
        }
        logOk("Connected to Firebird");

        testReservedWordException(conn);
        testReservedWordWithQuotes(conn);
        testOtherReservedWords(conn);

        logMsg("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return 1;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return 1;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return 0;
#endif
}
