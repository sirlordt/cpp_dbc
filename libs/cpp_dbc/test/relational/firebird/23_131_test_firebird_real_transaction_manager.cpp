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

 @file 23_131_test_firebird_real_transaction_manager.cpp
 @brief Tests for Firebird transaction management with real database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "23_001_test_firebird_real_common.hpp"

#if USE_FIREBIRD

// =============================================================================
// Firebird TransactionContext tests
// =============================================================================

TEST_CASE("Firebird TransactionContext tests", "[23_131_01_firebird_real_transaction_manager]")
{
    // Skip if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    INFO("Firebird TransactionContext tests");

    // Get Firebird configuration
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Create a real Firebird connection
    cpp_dbc::Firebird::FirebirdDBDriver driver;
    auto connBase = driver.connect(connStr, username, password);
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);
    REQUIRE(conn != nullptr);

    // Create a transaction context with the real connection
    cpp_dbc::TransactionContext context(conn, "test-tx-id-firebird");

    // Check the transaction ID
    REQUIRE(context.transactionId == "test-tx-id-firebird");

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

    // Verify the connection can execute queries (Firebird requires FROM RDB$DATABASE)
    auto rs = conn->executeQuery("SELECT 1 as TEST_VALUE FROM RDB$DATABASE");
    REQUIRE(rs != nullptr);
    REQUIRE(rs->next());
    REQUIRE(rs->getInt("TEST_VALUE") == 1); // Firebird returns uppercase column names

    // Close the connection
    conn->close();
}

// =============================================================================
// Firebird TransactionManager multi-threaded tests
// =============================================================================

TEST_CASE("Firebird TransactionManager multi-threaded tests", "[23_131_02_firebird_real_transaction_manager]")
{
    SKIP("Concurrent transaction tests moved to 23_141_test_firebird_real_connection_pool.cpp");
}

// =============================================================================
// Firebird TransactionManager real database tests
// =============================================================================

TEST_CASE("Real Firebird transaction manager tests", "[23_131_03_firebird_real_transaction_manager]")
{
    SKIP("Basic transaction operation tests moved to 23_141_test_firebird_real_connection_pool.cpp");
}

#endif
