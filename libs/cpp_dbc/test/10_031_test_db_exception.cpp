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

 @file 10_031_test_db_exception.cpp
 @brief Tests for DBException class

*/

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

// Test case for DBException
TEST_CASE("DBException tests", "[10_031_01_exception]")
{
    SECTION("Create and throw DBException without mark")
    {
        // Create an exception without mark
        cpp_dbc::DBException ex("", "Test error message");

        // Check the error message
        REQUIRE(std::string(ex.what_s()) == "Test error message");

        // Check the mark is empty
        REQUIRE(ex.getMark().empty());

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            cpp_dbc::DBException);

        // Check that we can catch it as a std::exception
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            std::exception);
    }

    SECTION("Create and throw DBException with mark")
    {
        // Create an exception with mark
        cpp_dbc::DBException ex("9S0T1U2V3W4X", "Test error message");

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "9S0T1U2V3W4X: Test error message");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "9S0T1U2V3W4X");

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            cpp_dbc::DBException);

        // Verify the mark in a caught exception
        try
        {
            throw cpp_dbc::DBException("1M2N3O4P5Q6R", "Error message");
        }
        catch (const cpp_dbc::DBException &e)
        {
            REQUIRE(e.getMark() == "1M2N3O4P5Q6R");
            REQUIRE(std::string(e.what_s()) == "1M2N3O4P5Q6R: Error message");
        }
    }

    SECTION("Create and throw DBException with callstack")
    {
        // Create a simple callstack manually for testing
        std::vector<cpp_dbc::system_utils::StackFrame> test_callstack;
        cpp_dbc::system_utils::StackFrame frame;
        frame.file = "test_file.cpp";
        frame.line = 42;
        frame.function = "test_function";
        test_callstack.push_back(frame);

        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with callstack", test_callstack);

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK: Test error with callstack");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "CALLSTACK");

        // Check that the callstack is stored and can be retrieved
        REQUIRE(ex.getCallStack().size() == 1);
        REQUIRE(ex.getCallStack()[0].file == "test_file.cpp");
        REQUIRE(ex.getCallStack()[0].line == 42);
        REQUIRE(ex.getCallStack()[0].function == "test_function");

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
    }

    SECTION("Capture real callstack and throw DBException with callstack")
    {
        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with real callstack", cpp_dbc::system_utils::captureCallStack(true));

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK: Test error with real callstack");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "CALLSTACK");

        // Check that the callstack is stored and can be retrieved
        REQUIRE(ex.getCallStack().size() >= 1);

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
    }
}
