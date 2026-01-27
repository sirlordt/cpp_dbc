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

 @file test_scylladb_db_config.cpp
 @brief Tests for ScyllaDB database configuration handling

*/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_SCYLLADB

// Test case to verify specific ScyllaDB database configuration
TEST_CASE("Specific ScyllaDB database configuration", "[26_011_01_scylladb_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify dev_scylla configuration")
    {
        auto devScyllaOpt = configManager.getDatabaseByName("dev_scylla");

        // Check that the database was found
        REQUIRE(devScyllaOpt.has_value());

        // Get the reference to the database config
        const auto &devScylla = devScyllaOpt->get();

        // Check connection parameters
        REQUIRE(devScylla.getType() == "scylladb");
        REQUIRE(devScylla.getHost() == "localhost");
        REQUIRE(devScylla.getPort() == 9042);
        REQUIRE(devScylla.getDatabase() == "dev_keyspace");
        REQUIRE(devScylla.getUsername() == "cassandra");
        REQUIRE(devScylla.getPassword() == "dsystems");

        // Check options
        REQUIRE(devScylla.getOption("connect_timeout") == "5000");
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_SCYLLADB
