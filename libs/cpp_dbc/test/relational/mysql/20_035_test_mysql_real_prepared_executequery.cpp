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

 @file 20_035_test_mysql_real_prepared_executequery.cpp
 @brief Tests for MySQL PreparedStatement::executeQuery() — materialized mode

*/

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "20_001_test_mysql_real_common.hpp"

#if USE_MYSQL

TEST_CASE("MySQL PreparedStatement::executeQuery() — materialized mode", "[20_035_01_mysql_prepared_executequery]")
{
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test table
    conn->executeUpdate("DROP TABLE IF EXISTS test_pstmt_eq");
    conn->executeUpdate(
        "CREATE TABLE test_pstmt_eq ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(200), "
        "value DOUBLE, "
        "flag BOOLEAN, "
        "created_date DATE, "
        "created_ts DATETIME, "
        "notes TEXT, "
        "bin_data BLOB"
        ")");

    SECTION("Basic SELECT with int parameter")
    {
        // Insert via executeUpdate (known-working path)
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, value) VALUES (?, ?, ?)");
        for (int i = 1; i <= 5; ++i)
        {
            ins->setInt(1, i);
            ins->setString(2, "Row " + std::to_string(i));
            ins->setDouble(3, i * 2.5);
            REQUIRE(ins->executeUpdate() == 1);
        }

        // executeQuery with prepared statement — the path under test
        auto sel = conn->prepareStatement("SELECT id, name, value FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 3);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 3);
        REQUIRE(rs->getString("name") == "Row 3");
        REQUIRE(rs->getDouble("value") == Catch::Approx(7.5));
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SELECT returning multiple rows")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, value) VALUES (?, ?, ?)");
        for (int i = 1; i <= 10; ++i)
        {
            ins->setInt(1, i);
            ins->setString(2, "Item " + std::to_string(i));
            ins->setDouble(3, i * 0.1);
            ins->executeUpdate();
        }

        auto sel = conn->prepareStatement(
            "SELECT id, name, value FROM test_pstmt_eq WHERE id <= ? ORDER BY id");
        sel->setInt(1, 5);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);

        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("id") == count);
            REQUIRE(rs->getString("name") == "Item " + std::to_string(count));
        }
        REQUIRE(count == 5);
    }

    SECTION("SELECT returning empty result set")
    {
        auto sel = conn->prepareStatement(
            "SELECT id, name FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 9999);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->isEmpty());
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SELECT with string parameter")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name) VALUES (?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Hello World");
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT id, name FROM test_pstmt_eq WHERE name = ?");
        sel->setString(1, "Hello World");
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Hello World");
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SELECT with multiple parameter types")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, value, flag, created_date, created_ts) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Multi-type test");
        ins->setDouble(3, 3.14159);
        ins->setBoolean(4, true);
        ins->setDate(5, "2025-06-15");
        ins->setTimestamp(6, "2025-06-15 10:30:00");
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT id, name, value, flag, created_date, created_ts "
            "FROM test_pstmt_eq WHERE id = ? AND flag = ?");
        sel->setInt(1, 1);
        sel->setBoolean(2, true);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Multi-type test");
        REQUIRE(rs->getDouble("value") == Catch::Approx(3.14159));
        REQUIRE(rs->getBoolean("flag") == true);
        REQUIRE(rs->getDate("created_date") == "2025-06-15");
        REQUIRE(rs->getTimestamp("created_ts") == "2025-06-15 10:30:00");
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SELECT with NULL values")
    {
        conn->executeUpdate(
            "INSERT INTO test_pstmt_eq (id, name, value, notes) VALUES (1, NULL, NULL, NULL)");

        auto sel = conn->prepareStatement(
            "SELECT id, name, value, notes FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->isNull("name"));
        REQUIRE(rs->isNull("value"));
        REQUIRE(rs->isNull("notes"));
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SELECT with BLOB data via prepared executeQuery")
    {
        std::vector<uint8_t> blobData = common_test_helpers::generateRandomBinaryData(5000);

        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, bin_data) VALUES (?, ?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Blob row");
        ins->setBytes(3, blobData);
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT id, name, bin_data FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Blob row");

        auto retrieved = rs->getBytes("bin_data");
        REQUIRE(common_test_helpers::compareBinaryData(blobData, retrieved));
        REQUIRE_FALSE(rs->next());
    }

    SECTION("Reuse prepared statement for multiple executeQuery calls")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name) VALUES (?, ?)");
        for (int i = 1; i <= 5; ++i)
        {
            ins->setInt(1, i);
            ins->setString(2, "Reuse " + std::to_string(i));
            ins->executeUpdate();
        }

        auto sel = conn->prepareStatement(
            "SELECT id, name FROM test_pstmt_eq WHERE id = ?");

        // First query
        sel->setInt(1, 2);
        auto rs1 = sel->executeQuery();
        REQUIRE(rs1 != nullptr);
        REQUIRE(rs1->next());
        REQUIRE(rs1->getString("name") == "Reuse 2");
        REQUIRE_FALSE(rs1->next());

        // Second query — reuse the same prepared statement
        sel->setInt(1, 4);
        auto rs2 = sel->executeQuery();
        REQUIRE(rs2 != nullptr);
        REQUIRE(rs2->next());
        REQUIRE(rs2->getString("name") == "Reuse 4");
        REQUIRE_FALSE(rs2->next());

        // Third query — non-existent row
        sel->setInt(1, 99);
        auto rs3 = sel->executeQuery();
        REQUIRE(rs3 != nullptr);
        REQUIRE_FALSE(rs3->next());
    }

    SECTION("executeQuery then executeUpdate on same statement")
    {
        conn->executeUpdate("INSERT INTO test_pstmt_eq (id, name) VALUES (1, 'Original')");

        // First: executeQuery
        auto stmt = conn->prepareStatement("SELECT id, name FROM test_pstmt_eq WHERE id = ?");
        stmt->setInt(1, 1);
        auto rs = stmt->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Original");

        // Now use a different statement for update (same connection)
        auto upd = conn->prepareStatement("UPDATE test_pstmt_eq SET name = ? WHERE id = ?");
        upd->setString(1, "Updated");
        upd->setInt(2, 1);
        REQUIRE(upd->executeUpdate() == 1);

        // Re-query with original statement
        stmt->setInt(1, 1);
        auto rs2 = stmt->executeQuery();
        REQUIRE(rs2 != nullptr);
        REQUIRE(rs2->next());
        REQUIRE(rs2->getString("name") == "Updated");
    }

    SECTION("Column metadata from materialized result set")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, value) VALUES (?, ?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Meta test");
        ins->setDouble(3, 42.0);
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT id, name, value FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);

        auto colNames = rs->getColumnNames();
        REQUIRE(colNames.size() == 3);
        REQUIRE(colNames[0] == "id");
        REQUIRE(colNames[1] == "name");
        REQUIRE(colNames[2] == "value");
        REQUIRE(rs->getColumnCount() == 3);
    }

    SECTION("ResultSet navigation on materialized result set")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name) VALUES (?, ?)");
        for (int i = 1; i <= 3; ++i)
        {
            ins->setInt(1, i);
            ins->setString(2, "Nav " + std::to_string(i));
            ins->executeUpdate();
        }

        auto sel = conn->prepareStatement(
            "SELECT id, name FROM test_pstmt_eq ORDER BY id");
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);

        REQUIRE(rs->isBeforeFirst());
        REQUIRE_FALSE(rs->isAfterLast());

        REQUIRE(rs->next());
        REQUIRE(rs->getRow() == 1);
        REQUIRE(rs->next());
        REQUIRE(rs->getRow() == 2);
        REQUIRE(rs->next());
        REQUIRE(rs->getRow() == 3);
        REQUIRE_FALSE(rs->next());
    }

    SECTION("Special characters in string parameters")
    {
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name) VALUES (?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "O'Reilly \"quoted\" \\ backslash % percent _ underscore");
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT name FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "O'Reilly \"quoted\" \\ backslash % percent _ underscore");
    }

    SECTION("Large TEXT column via prepared executeQuery")
    {
        // Generate a large string (bigger than typical buffer)
        std::string largeText(50000, 'X');
        for (size_t i = 0; i < largeText.size(); i += 100)
        {
            largeText[i] = static_cast<char>('A' + (i / 100) % 26);
        }

        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, notes) VALUES (?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, largeText);
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT notes FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getString("notes") == largeText);
    }

    SECTION("String parameter containing question mark — proves mysql_stmt API is used")
    {
        // If the old string-concatenation path were still in use, the '?' inside
        // the data would be mistaken for a placeholder by finalQuery.find('?')
        // and replaced, corrupting the query or causing a parameter count mismatch.
        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name) VALUES (?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Is this a question? Yes it is? Really?");
        ins->executeUpdate();

        auto sel = conn->prepareStatement(
            "SELECT name FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Is this a question? Yes it is? Really?");
        REQUIRE_FALSE(rs->next());
    }

    SECTION("SQL injection attempt — proves parameterized queries prevent injection")
    {
        // Insert a legitimate row
        conn->executeUpdate("INSERT INTO test_pstmt_eq (id, name) VALUES (1, 'Legit')");

        // Attempt SQL injection via a parameter — the old string-concatenation
        // path would execute the DROP TABLE as a second statement
        auto sel = conn->prepareStatement(
            "SELECT id, name FROM test_pstmt_eq WHERE name = ?");
        sel->setString(1, "'; DROP TABLE test_pstmt_eq; --");
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        // Should find no rows (the injection string is not a real name)
        REQUIRE_FALSE(rs->next());

        // Verify the table still exists and the original row is intact
        auto verify = conn->executeQuery("SELECT COUNT(*) AS cnt FROM test_pstmt_eq");
        REQUIRE(verify->next());
        REQUIRE(verify->getInt("cnt") == 1);
    }

    SECTION("Binary data with explicit null bytes — proves no C-string truncation")
    {
        // Build a 1000-byte buffer with \0 at known positions
        std::vector<uint8_t> blobData(1000, 0x41); // fill with 'A'
        blobData[0]   = 0x00; // null at start
        blobData[100] = 0x00;
        blobData[500] = 0x00;
        blobData[999] = 0x00; // null at end

        auto ins = conn->prepareStatement(
            "INSERT INTO test_pstmt_eq (id, name, bin_data) VALUES (?, ?, ?)");
        ins->setInt(1, 1);
        ins->setString(2, "Null bytes");
        ins->setBytes(3, blobData);
        ins->executeUpdate();

        // Retrieve via PreparedStatement::executeQuery (materialized path)
        auto sel = conn->prepareStatement(
            "SELECT bin_data FROM test_pstmt_eq WHERE id = ?");
        sel->setInt(1, 1);
        auto rs = sel->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());

        auto retrieved = rs->getBytes("bin_data");
        REQUIRE(retrieved.size() == 1000);
        REQUIRE(common_test_helpers::compareBinaryData(blobData, retrieved));
        REQUIRE_FALSE(rs->next());
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS test_pstmt_eq");
    conn->close();
}

#else
TEST_CASE("MySQL PreparedStatement::executeQuery() (skipped)", "[20_035_01_mysql_prepared_executequery]")
{
    SKIP("MySQL support is not enabled");
}
#endif
