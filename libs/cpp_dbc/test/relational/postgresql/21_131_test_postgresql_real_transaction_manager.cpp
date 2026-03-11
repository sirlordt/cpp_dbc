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

 @file test_postgresql_real_transaction_manager.cpp
 @brief Tests for PostgreSQL transaction management with real database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "21_001_test_postgresql_real_common.hpp"

#if USE_POSTGRESQL

// =============================================================================
// PostgreSQL TransactionContext tests
// =============================================================================

TEST_CASE("PostgreSQL TransactionContext tests", "[21_131_01_postgresql_real_transaction_manager]")
{
    // Skip if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    INFO("PostgreSQL TransactionContext tests");

    // Get PostgreSQL configuration
    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Create a real PostgreSQL connection
    auto driver = postgresql_test_helpers::getPostgreSQLDriver();
    REQUIRE(driver != nullptr);
    auto connBase = driver->connect(connStr, username, password);
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);
    REQUIRE(conn != nullptr);

    // Create a transaction context with the real connection
    cpp_dbc::TransactionContext context(conn, "test-tx-id-postgresql");

    // Check the transaction ID
    REQUIRE(context.transactionId == "test-tx-id-postgresql");

    // Check the connection is valid
    REQUIRE(context.connection != nullptr);
    REQUIRE(context.connection == conn);

    // Check that the last access time is recent
    auto now = std::chrono::steady_clock::now();
    auto lastAccess = context.lastAccessTime;
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - lastAccess).count();
    REQUIRE(diff < 5); // Should be less than 5 seconds

    // Update the last access time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    context.lastAccessTime = std::chrono::steady_clock::now();

    // Check that the last access time was updated
    auto newLastAccess = context.lastAccessTime;
    REQUIRE(newLastAccess > lastAccess);

    // Verify the connection can execute queries
    auto rs = conn->executeQuery("SELECT 1 as test_value");
    REQUIRE(rs != nullptr);
    REQUIRE(rs->next());
    REQUIRE(rs->getInt("test_value") == 1);

    // Close the connection
    conn->close();
}

// =============================================================================
// PostgreSQL TransactionManager multi-threaded tests
// =============================================================================

TEST_CASE("PostgreSQL TransactionManager multi-threaded tests", "[21_131_02_postgresql_real_transaction_manager]")
{
    SKIP("Concurrent transaction tests moved to 21_141_test_postgresql_real_connection_pool.cpp");
}

// =============================================================================
// PostgreSQL TransactionManager real database tests
// =============================================================================

TEST_CASE("Real PostgreSQL transaction manager tests", "[21_131_03_postgresql_real_transaction_manager]")
{
    SKIP("Basic transaction operation tests moved to 21_141_test_postgresql_real_connection_pool.cpp");
}

#endif
