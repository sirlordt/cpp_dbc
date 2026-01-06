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
 * @file firebird_reserved_word_example.cpp
 * @brief Example to test if Firebird throws exception when using reserved word 'value'
 *
 * This example demonstrates what happens when trying to create a table with a
 * column named 'value', which is a reserved word in Firebird SQL.
 *
 * Build and run:
 *   cd build
 *   cmake .. -DUSE_FIREBIRD=ON -DCPP_DBC_BUILD_EXAMPLES=ON
 *   make firebird_reserved_word_example
 *   ./firebird_reserved_word_example
 */

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#include <iostream>
#include <memory>
#include <any>

// Database configuration - uses the same database as the tests
const std::string FIREBIRD_HOST = "localhost";
const int FIREBIRD_PORT = 3050;
const std::string FIREBIRD_DATABASE = "/firebird/data/test_firebird.fdb";
const std::string FIREBIRD_USER = "SYSDBA";
const std::string FIREBIRD_PASSWORD = "dsystems";

#if USE_FIREBIRD

/**
 * @brief Try to create the database if it doesn't exist
 * @param driver The Firebird driver instance
 * @param url The database URL
 * @return true if database exists or was created successfully
 */
bool tryCreateDatabase(std::shared_ptr<cpp_dbc::DBDriver> driver, const std::string &url)
{
    // First, try to connect to see if database already exists
    try
    {
        auto conn = cpp_dbc::DriverManager::getDBConnection(url, FIREBIRD_USER, FIREBIRD_PASSWORD);
        std::cout << "Database exists and connection successful!" << std::endl;
        conn->close();
        return true;
    }
    catch (const std::exception &e)
    {
        // Database doesn't exist, try to create it
        std::cout << "Database doesn't exist: " << e.what() << std::endl;
        std::cout << "Attempting to create it..." << std::endl;
    }

    try
    {
        // Use the driver's command method to create the database
        std::map<std::string, std::any> params = {
            {"command", std::string("create_database")},
            {"url", url},
            {"user", FIREBIRD_USER},
            {"password", FIREBIRD_PASSWORD},
            {"page_size", std::string("4096")},
            {"charset", std::string("UTF8")}};

        driver->command(params);
        std::cout << "Database created successfully!" << std::endl;
        return true;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Failed to create database: " << e.what_s() << std::endl;
        std::cerr << std::endl;
        std::cerr << "To fix this, you may need to:" << std::endl;
        std::cerr << "1. Ensure the directory exists and is writable by the Firebird server" << std::endl;
        std::cerr << "   sudo mkdir -p /firebird/data" << std::endl;
        std::cerr << "   sudo chown firebird:firebird /firebird/data" << std::endl;
        std::cerr << "2. Configure Firebird to allow database creation in the target directory" << std::endl;
        std::cerr << "   Edit /etc/firebird/3.0/firebird.conf (or similar path)" << std::endl;
        std::cerr << "   Set: DatabaseAccess = Full" << std::endl;
        std::cerr << "3. Restart Firebird: sudo systemctl restart firebird3.0" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Alternatively, create the database manually:" << std::endl;
        std::cerr << "   isql-fb -user " << FIREBIRD_USER << " -password " << FIREBIRD_PASSWORD << std::endl;
        std::cerr << "   SQL> CREATE DATABASE '" << FIREBIRD_DATABASE << "';" << std::endl;
        std::cerr << "   SQL> quit;" << std::endl;
        return false;
    }
}

void testReservedWordException(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Test 1: CREATE TABLE with reserved word 'value' ===" << std::endl;
    std::cout << "SQL: CREATE TABLE test_reserved (id INTEGER PRIMARY KEY, value INTEGER)" << std::endl;

    try
    {
        // This should throw an exception because 'value' is a reserved word in Firebird
        conn->executeUpdate(
            "CREATE TABLE test_reserved ("
            "id INTEGER PRIMARY KEY, "
            "value INTEGER" // 'value' is a reserved word in Firebird!
            ")");

        std::cout << "WARNING: No exception was thrown! Table was created successfully." << std::endl;
        std::cout << "This means 'value' might not be a reserved word in your Firebird version." << std::endl;

        // Clean up - drop the table if it was created
        try
        {
            conn->executeUpdate("DROP TABLE test_reserved");
            std::cout << "Table dropped successfully." << std::endl;
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cout << "SUCCESS: Exception was thrown as expected!" << std::endl;
        std::cout << "Error message: " << e.what_s() << std::endl;
    }
}

void testReservedWordWithQuotes(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Test 2: CREATE TABLE with quoted 'value' (should work) ===" << std::endl;
    std::cout << "SQL: CREATE TABLE test_quoted (id INTEGER PRIMARY KEY, \"value\" INTEGER)" << std::endl;

    try
    {
        // First, try to drop the table if it exists from a previous run
        try
        {
            conn->executeUpdate("DROP TABLE test_quoted");
            std::cout << "Dropped existing test_quoted table." << std::endl;
        }
        catch (...)
        {
            // Table doesn't exist, ignore
        }

        // Using double quotes should allow using reserved words as identifiers
        conn->executeUpdate(
            "CREATE TABLE test_quoted ("
            "id INTEGER PRIMARY KEY, "
            "\"value\" INTEGER" // Quoted identifier should work
            ")");

        std::cout << "SUCCESS: Table created successfully with quoted identifier." << std::endl;

        // Insert some data
        auto stmt = conn->prepareStatement("INSERT INTO test_quoted (id, \"value\") VALUES (?, ?)");
        stmt->setInt(1, 1);
        stmt->setInt(2, 100);
        stmt->executeUpdate();
        stmt->close(); // Close PreparedStatement (required for Firebird)
        std::cout << "Data inserted successfully." << std::endl;

        // Query the data
        auto rs = conn->executeQuery("SELECT id, \"value\" FROM test_quoted");
        while (rs->next())
        {
            std::cout << "Row: id=" << rs->getInt("ID") << ", value=" << rs->getInt("value") << std::endl;
        }
        rs->close(); // Close ResultSet before DROP (required for Firebird)

        // Commit the transaction before DROP to release locks
        conn->commit();

        // Clean up
        conn->executeUpdate("DROP TABLE test_quoted");
        std::cout << "Table dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cout << "ERROR: Exception was thrown!" << std::endl;
        std::cout << "Error message: " << e.what_s() << std::endl;
    }
}

void testOtherReservedWords(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Test 3: Other reserved words ===" << std::endl;

    // List of common reserved words in Firebird
    std::vector<std::string> reservedWords = {
        "VALUE", "USER", "DATE", "TIME", "TIMESTAMP", "ORDER", "GROUP",
        "SELECT", "INSERT", "UPDATE", "DELETE", "TABLE", "INDEX"};

    for (const auto &word : reservedWords)
    {
        std::string sql = "CREATE TABLE test_" + word + " (id INTEGER PRIMARY KEY, " + word + " INTEGER)";
        std::cout << "\nTesting: " << word << std::endl;

        try
        {
            conn->executeUpdate(sql);
            std::cout << "  Result: Created (not reserved or allowed)" << std::endl;

            // Clean up
            try
            {
                conn->executeUpdate("DROP TABLE test_" + word);
            }
            catch (...)
            {
            }
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cout << "  Result: EXCEPTION - " << word << " is a reserved word" << std::endl;
            std::cout << "  Error: " << e.what_s() << std::endl;
        }
    }
}

#endif // USE_FIREBIRD

int main()
{
#if USE_FIREBIRD
    try
    {
        std::cout << "=== Firebird Reserved Word Test ===" << std::endl;
        std::cout << "This example tests if exceptions are thrown when using reserved words." << std::endl;

        // Create and register Firebird driver
        auto firebirdDriver = std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>();
        cpp_dbc::DriverManager::registerDriver(firebirdDriver);

        // Build connection URL
        std::string url = "cpp_dbc:firebird://" + FIREBIRD_HOST + ":" +
                          std::to_string(FIREBIRD_PORT) + FIREBIRD_DATABASE;

        std::cout << "\nConnecting to Firebird..." << std::endl;
        std::cout << "URL: " << url << std::endl;
        std::cout << "User: " << FIREBIRD_USER << std::endl;

        // Try to create the database if it doesn't exist
        if (!tryCreateDatabase(firebirdDriver, url))
        {
            std::cerr << "Failed to create or connect to database." << std::endl;
            return 1;
        }

        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(url, FIREBIRD_USER, FIREBIRD_PASSWORD));

        std::cout << "Connected successfully!" << std::endl;

        // Run tests
        testReservedWordException(conn);
        testReservedWordWithQuotes(conn);
        testOtherReservedWords(conn);

        // Close connection
        conn->close();
        std::cout << "\n=== All tests completed ===" << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "Firebird support is not enabled. Build with -DUSE_FIREBIRD=ON" << std::endl;
    return 1;
#endif

    return 0;
}