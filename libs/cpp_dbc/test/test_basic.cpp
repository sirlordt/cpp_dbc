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