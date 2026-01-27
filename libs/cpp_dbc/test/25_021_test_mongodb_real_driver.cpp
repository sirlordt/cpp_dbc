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

 @file test_mongodb_driver.cpp
 @brief Tests for MongoDB driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_MONGODB
// Test case for MongoDB driver
TEST_CASE("MongoDB driver tests", "[25_021_01_mongodb_real_driver]")
{
    SECTION("MongoDB driver URL acceptance")
    {
        // Create a MongoDB driver
        cpp_dbc::MongoDB::MongoDBDriver driver;

        // Check that it accepts MongoDB URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:mongodb://localhost:27017/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mongodb://127.0.0.1:27017/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mongodb://db.example.com:27017/testdb"));

        // Check that it rejects non-MongoDB URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("mongodb://localhost:27017/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:mongodb://localhost:27017/testdb"));
    }

    SECTION("MongoDB driver connection string parsing")
    {
        // Create a MongoDB driver
        cpp_dbc::MongoDB::MongoDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:mongodb://localhost:27017/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }
}
#endif
