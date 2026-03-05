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

 @file 25_041_test_mongodb_real_connection.cpp
 @brief Tests for MongoDB database operations with real connections

*/

#include <string>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "25_001_test_mongodb_real_common.hpp"

#if USE_MONGODB
// Test case to verify MongoDB connection
TEST_CASE("MongoDB connection test", "[25_041_01_mongodb_real_connection]")
{
    // Get MongoDB configuration
    auto dbConfig = mongodb_test_helpers::getMongoDBConfig("dev_mongodb");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Create connection string using helper
    std::string connStr = mongodb_test_helpers::buildMongoDBConnectionString(dbConfig);

    // Get a MongoDB driver
    auto driver = mongodb_test_helpers::getMongoDBDriver();

    // Skip if MongoDB is not reachable — availability was already checked by canConnectToMongoDB()
    // called in the TEST_CASE guard above; any DBException here is an unexpected regression.
    if (!mongodb_test_helpers::canConnectToMongoDB())
    {
        SKIP("Cannot connect to MongoDB database");
        return;
    }

    SECTION("Test MongoDB connection")
    {
        // Attempt to connect to MongoDB
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to MongoDB with connection string: " + connStr);

        auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
            driver->connectDocument(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Execute a ping to verify the connection
        REQUIRE(conn->ping() == true);

        // Verify connection state and URL
        CHECK_FALSE(conn->isClosed());

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection URL: " + conn->getURL());

        // The driver strips the "cpp_dbc:" prefix when storing the URL
        if (connStr.substr(0, 8) == "cpp_dbc:")
        {
            CHECK(conn->getURL() == connStr.substr(8));
        }
        else
        {
            CHECK(conn->getURL() == connStr);
        }

        // Close the connection
        conn->close();
        CHECK(conn->isClosed());
    }
}
#else
// Skip this test if MongoDB support is not enabled
TEST_CASE("MongoDB connection test (skipped)", "[25_041_02_mongodb_real_connection]")
{
    SKIP("MongoDB support is not enabled");
}
#endif
