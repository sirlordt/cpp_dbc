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

 @file test_yaml.cpp
 @brief Tests for YAML configuration loading

*/

#include <catch2/catch_test_macros.hpp>
#include <string>

// Only include yaml-cpp if USE_CPP_YAML is defined
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
#include <yaml-cpp/yaml.h>

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
#else
// Test case to verify that yaml-cpp can be used
TEST_CASE("Basic YAML operations", "[yaml]")
{
    SKIP("YAML support is disabled");
}
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1