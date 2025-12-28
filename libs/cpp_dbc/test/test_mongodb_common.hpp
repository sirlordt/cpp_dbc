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

 @file test_mongodb_common.hpp
 @brief Tests for MongoDB database operations

*/

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <tuple>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_main.hpp"

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

namespace mongodb_test_helpers
{

#if USE_MONGODB

    /**
     * @brief Get MongoDB database configuration
     *
     * Gets a DatabaseConfig object with MongoDB connection parameters either from:
     * - YAML config file (when USE_CPP_YAML is defined)
     * - Hardcoded default values (when USE_CPP_YAML is not defined)
     *
     * The returned DatabaseConfig object also includes test collection names stored as options:
     * - "collection__test" - Test collection name
     *
     * @param databaseName The name to use for the configuration
     * @param useEmptyDatabase If true, returns configuration with empty database name
     * @return cpp_dbc::config::DatabaseConfig with MongoDB connection parameters
     */
    cpp_dbc::config::DatabaseConfig getMongoDBConfig(const std::string &databaseName = "dev_mongodb",
                                                     bool useEmptyDatabase = false);

    /**
     * @brief Helper function to check if we can connect to MongoDB
     */
    bool canConnectToMongoDB();

    /**
     * @brief Build a MongoDB connection string from a DatabaseConfig
     *
     * Builds a properly formatted MongoDB connection string including:
     * - Host and port
     * - Username and password (if provided)
     * - Database name
     * - Query parameters from options (authSource, directConnection, timeouts)
     *
     * @param dbConfig The database configuration
     * @return std::string The MongoDB connection string
     */
    std::string buildMongoDBConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig);

    /**
     * @brief Helper function to get a MongoDB driver instance
     */
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBDriver> getMongoDBDriver();

    /**
     * @brief Helper function to get a MongoDB connection
     */
    std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> getMongoDBConnection();

    /**
     * @brief Helper function to generate a random document for testing
     */
    std::string generateTestDocument(int id, const std::string &name, double value);

    /**
     * @brief Helper function to generate a random collection name
     */
    std::string generateRandomCollectionName();

#endif

} // namespace mongodb_test_helpers