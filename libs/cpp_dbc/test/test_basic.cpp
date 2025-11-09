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

 @file test_basic.cpp
 @brief Implementation for the cpp_dbc library

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <string>

// Test case to verify that the library headers can be included correctly
TEST_CASE("Basic cpp_dbc header inclusion", "[basic]")
{
    SECTION("Headers can be included without errors")
    {
        // This test simply verifies that the cpp_dbc headers can be included
        // without compilation errors
        REQUIRE(true);
    }
}

// Test case to verify basic string operations (simple test to ensure Catch2 works)
TEST_CASE("Basic string operations", "[basic]")
{
    SECTION("String comparison")
    {
        std::string test = "cpp_dbc";
        REQUIRE(test == "cpp_dbc");
        REQUIRE(test.length() == 7);
    }
}

// Test case to verify basic cpp_dbc constants and types
TEST_CASE("Basic cpp_dbc types and constants", "[basic]")
{
    SECTION("Check driver manager is defined")
    {
        // This just verifies that the DriverManager type exists
        // We don't actually use it yet to avoid runtime dependencies
        REQUIRE(std::is_class<cpp_dbc::DriverManager>::value);
    }
}