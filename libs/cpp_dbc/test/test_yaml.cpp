#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <string>

// Test case to verify that yaml-cpp can be used
TEST_CASE("Basic YAML operations", "[yaml]")
{
    SECTION("Parse simple YAML string")
    {
        // Create a simple YAML string
        std::string yaml_str = "key: value\nlist:\n  - item1\n  - item2";

        // Parse the YAML string
        YAML::Node node = YAML::Load(yaml_str);

        // Verify the parsed values
        REQUIRE(node["key"].as<std::string>() == "value");
        REQUIRE(node["list"].IsSequence());
        REQUIRE(node["list"].size() == 2);
        REQUIRE(node["list"][0].as<std::string>() == "item1");
        REQUIRE(node["list"][1].as<std::string>() == "item2");
    }

    SECTION("Create and emit YAML")
    {
        // Create a YAML node programmatically
        YAML::Node node;
        node["database"] = "mysql";
        node["host"] = "localhost";
        node["port"] = 3306;
        node["credentials"]["username"] = "user";
        node["credentials"]["password"] = "pass";

        // Convert to string
        std::string yaml_str = YAML::Dump(node);

        // Parse back to verify
        YAML::Node parsed = YAML::Load(yaml_str);

        // Verify the values
        REQUIRE(parsed["database"].as<std::string>() == "mysql");
        REQUIRE(parsed["host"].as<std::string>() == "localhost");
        REQUIRE(parsed["port"].as<int>() == 3306);
        REQUIRE(parsed["credentials"]["username"].as<std::string>() == "user");
        REQUIRE(parsed["credentials"]["password"].as<std::string>() == "pass");
    }
}