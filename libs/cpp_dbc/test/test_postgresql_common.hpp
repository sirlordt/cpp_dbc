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

 @file test_postgresql_common.hpp
 @brief Tests for PostgreSQL database operations

*/

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <optional>

#include <cpp_dbc/cpp_dbc.hpp>

#include "test_main.hpp"

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#include <cpp_dbc/drivers/relational/postgresql_blob.hpp>
#endif

namespace postgresql_test_helpers
{

#if USE_POSTGRESQL

    /**
     * @brief Get PostgreSQL database configuration with test queries
     *
     * Gets a DatabaseConfig object with PostgreSQL connection parameters either from:
     * - YAML config file (when USE_CPP_YAML is defined)
     * - Hardcoded default values (when USE_CPP_YAML is not defined)
     *
     * The returned DatabaseConfig object also includes SQL queries stored as options:
     * - "query__create_table" - CREATE TABLE query
     * - "query__insert_data" - INSERT query
     * - "query__select_data" - SELECT query
     * - "query__drop_table" - DROP TABLE query
     *
     * @param databaseName The name to use for the configuration
     * @param useEmptyDatabase If true, returns configuration with empty database name
     * @return cpp_dbc::config::DatabaseConfig with PostgreSQL connection parameters and test queries
     */
    cpp_dbc::config::DatabaseConfig getPostgreSQLConfig(const std::string &databaseName = "dev_postgresql",
                                                        bool useEmptyDatabase = false);

    // Helper function to try to create the database if it doesn't exist
    bool tryCreateDatabase();

    // Helper function to check if we can connect to PostgreSQL
    bool canConnectToPostgreSQL();

#endif

} // namespace postgresql_test_helpers