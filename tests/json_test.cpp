#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

TEST_CASE("JSON basic operations", "[json]")
{
    SECTION("Creating and accessing JSON objects")
    {
        // Create a JSON object
        json j = {
            {"name", "CPP_DBC"},
            {"version", "0.1.0"},
            {"supported_databases", {{"mysql", true}, {"postgresql", true}}}};

        // Verify values
        REQUIRE(j["name"] == "CPP_DBC");
        REQUIRE(j["version"] == "0.1.0");
        REQUIRE(j["supported_databases"]["mysql"] == true);
        REQUIRE(j["supported_databases"]["postgresql"] == true);
    }

    SECTION("Serialization and deserialization")
    {
        // Create a JSON object
        json j = {
            {"name", "CPP_DBC"},
            {"version", "0.1.0"}};

        // Serialize to string
        std::string serialized = j.dump(4); // With 4-space indentation

        // Output for demonstration
        INFO("Serialized JSON: " << serialized);

        // Deserialize from string
        json parsed = json::parse(serialized);

        // Verify that data is preserved
        REQUIRE(parsed["name"] == "CPP_DBC");
        REQUIRE(parsed["version"] == "0.1.0");
    }

    SECTION("Database connection configuration")
    {
        // Example of a database connection configuration
        json config = {
            {"connections", {{{"name", "mysql_local"}, {"type", "mysql"}, {"host", "localhost"}, {"port", 3306}, {"user", "root"}, {"database", "test_db"}}, {{"name", "postgres_dev"}, {"type", "postgresql"}, {"host", "db.example.com"}, {"port", 5432}, {"user", "dev_user"}, {"database", "dev_db"}}}}};

        // Verify configuration values
        REQUIRE(config["connections"].size() == 2);
        REQUIRE(config["connections"][0]["name"] == "mysql_local");
        REQUIRE(config["connections"][1]["type"] == "postgresql");
        REQUIRE(config["connections"][1]["port"] == 5432);

        // Output for demonstration
        INFO("Database connection configuration: " << config.dump(4));
    }
}