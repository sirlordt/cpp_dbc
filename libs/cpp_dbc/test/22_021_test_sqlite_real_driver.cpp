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

 @file 22_021_test_sqlite_real_driver.cpp
 @brief Tests for SQLite driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_SQLITE
// Test case for SQLite driver
TEST_CASE("SQLite driver tests", "[22_021_01_sqlite_real_driver]")
{
    SECTION("SQLite driver URL acceptance")
    {
        // Create a SQLite driver
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // Check that it accepts SQLite URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:sqlite://:memory:"));
        REQUIRE(driver.acceptsURL("cpp_dbc:sqlite://test.db"));
        REQUIRE(driver.acceptsURL("cpp_dbc:sqlite:///path/to/database.db"));

        // Check that it rejects non-SQLite URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:sqlite://test.db"));
        REQUIRE_FALSE(driver.acceptsURL("sqlite://test.db"));
    }

    SECTION("SQLite driver connection to in-memory database")
    {
        // Create a SQLite driver
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // SQLite can connect to in-memory databases without external dependencies
        auto conn = driver.connect("cpp_dbc:sqlite://:memory:", "", "");
        REQUIRE(conn != nullptr);

        // Verify the connection works
        auto relConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
        REQUIRE(relConn != nullptr);
        REQUIRE(relConn->isClosed() == false);

        // Close the connection
        relConn->close();
        REQUIRE(relConn->isClosed() == true);
    }
}
#endif
