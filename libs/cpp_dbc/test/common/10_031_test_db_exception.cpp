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

#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

// Test case for DBException
TEST_CASE("DBException tests", "[10_031_01_exception]")
{
    SECTION("Create and throw DBException without mark")
    {
        // Create an exception without mark — empty mark is replaced with "MARK_NOT_DEF"
        // (exactly 12 chars) so the single-buffer layout contract always holds.
        cpp_dbc::DBException ex("", "Test error message");

        // Empty mark produces "MARK_NOT_DEF: <message>" in the full message
        REQUIRE(std::string(ex.what_s()) == "MARK_NOT_DEF: Test error message");

        // getMark() always returns exactly 12 chars; empty mark → "MARK_NOT_DEF"
        REQUIRE(ex.getMark() == "MARK_NOT_DEF");

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("9CITHRJGOOR4", "Test throw"),
            cpp_dbc::DBException);

        // Check that we can catch it as a std::exception
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("9CITHRJGOOR4", "Test throw"),
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
            throw cpp_dbc::DBException("9CITHRJGOOR4", "Test throw"),
            cpp_dbc::DBException);

        // Verify the mark in a caught exception
        try
        {
            throw cpp_dbc::DBException("1M2N3O4P5Q6R", "Error message");
        }
        catch (const cpp_dbc::DBException &ex1)
        {
            REQUIRE(ex1.getMark() == "1M2N3O4P5Q6R");
            REQUIRE(std::string(ex1.what_s()) == "1M2N3O4P5Q6R: Error message");
        }
    }

    SECTION("Create and throw DBException with callstack")
    {
        // Build a CallStackCapture manually with one frame for testing.
        // char[] members must be written via strncpy — they are not assignable with =.
        // CallStackCapture is now heap-allocated and passed as shared_ptr to DBException.
        auto test_callstack = std::make_shared<cpp_dbc::system_utils::CallStackCapture>();
        auto &frame = test_callstack->frames[test_callstack->count++];
        std::strncpy(frame.file, "test_file.cpp",
                     cpp_dbc::system_utils::StackFrame::FILE_MAX - 1);
        frame.file[cpp_dbc::system_utils::StackFrame::FILE_MAX - 1] = '\0';
        frame.line = 42;
        std::strncpy(frame.function, "test_function",
                     cpp_dbc::system_utils::StackFrame::FUNCTION_MAX - 1);
        frame.function[cpp_dbc::system_utils::StackFrame::FUNCTION_MAX - 1] = '\0';

        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with callstack", test_callstack);

        // getMark() always returns exactly 12 chars; "CALLSTACK" (9 chars) is padded
        // to 12 with 3 trailing spaces by the %-12.12s format in the constructor.
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK   : Test error with callstack");

        // getMark() returns exactly 12 bytes — "CALLSTACK" padded with 3 spaces
        REQUIRE(ex.getMark() == "CALLSTACK   ");

        // Check that the callstack is stored and can be retrieved.
        // char[] members compare as string_view — == on char[] is a pointer comparison.
        REQUIRE(ex.getCallStack().size() == 1);
        REQUIRE(std::string_view(ex.getCallStack()[0].file) == "test_file.cpp");
        REQUIRE(ex.getCallStack()[0].line == 42);
        REQUIRE(std::string_view(ex.getCallStack()[0].function) == "test_function");

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
    }

    SECTION("Capture real callstack and throw DBException with callstack")
    {
        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with real callstack", cpp_dbc::system_utils::captureCallStack(true));

        // "CALLSTACK" (9 chars) is padded to 12 with 3 trailing spaces by %-12.12s
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK   : Test error with real callstack");

        // getMark() returns exactly 12 bytes — "CALLSTACK" padded with 3 spaces
        REQUIRE(ex.getMark() == "CALLSTACK   ");

        // Check that the callstack can be retrieved (may be empty in BACKWARD_HAS_UNWIND=0 builds)
        // Only assert non-empty if entries were captured
        if (!ex.getCallStack().empty())
        {
            REQUIRE(ex.getCallStack().size() >= 1);
        }

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
    }
}
