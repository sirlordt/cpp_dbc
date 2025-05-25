#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// Test case using Catch2 framework
TEST_CASE("JSON operations with Catch2", "[json]")
{
    SECTION("Basic JSON creation and access")
    {
        // Create a JSON object
        json j = {
            {"name", "CPP_DBC"},
            {"version", "0.1.0"},
            {"supported_databases", {{"mysql", true}, {"postgresql", true}}}};

        // Verify values using Catch2 assertions
        REQUIRE(j["name"] == "CPP_DBC");
        REQUIRE(j["version"] == "0.1.0");
        REQUIRE(j["supported_databases"]["mysql"] == true);
        REQUIRE(j["supported_databases"]["postgresql"] == true);
    }

    SECTION("JSON serialization and deserialization")
    {
        json j = {
            {"name", "CPP_DBC"},
            {"version", "0.1.0"}};

        // Serialize to string
        std::string serialized = j.dump();

        // Deserialize from string
        json parsed = json::parse(serialized);

        // Verify that data is preserved
        REQUIRE(parsed["name"] == "CPP_DBC");
        REQUIRE(parsed["version"] == "0.1.0");
    }

    SECTION("JSON array operations")
    {
        // Create a JSON array
        json j = json::array();

        // Add elements
        j.push_back("mysql");
        j.push_back("postgresql");
        j.push_back("sqlite");

        // Verify array properties
        REQUIRE(j.size() == 3);
        REQUIRE(j[0] == "mysql");
        REQUIRE(j[1] == "postgresql");
        REQUIRE(j[2] == "sqlite");
    }

    SECTION("JSON object manipulation")
    {
        // Create an empty JSON object
        json j = json::object();

        // Add key-value pairs
        j["database"] = "mysql";
        j["host"] = "localhost";
        j["port"] = 3306;
        j["enabled"] = true;

        // Verify object properties
        REQUIRE(j.size() == 4);
        REQUIRE(j["database"] == "mysql");
        REQUIRE(j["host"] == "localhost");
        REQUIRE(j["port"] == 3306);
        REQUIRE(j["enabled"] == true);

        // Modify values
        j["port"] = 3307;
        REQUIRE(j["port"] == 3307);

        // Remove a key
        j.erase("enabled");
        REQUIRE(j.size() == 3);
        REQUIRE(j.contains("enabled") == false);
    }
}