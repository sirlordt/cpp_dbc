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

 @file 20_001_test_mysql_real_common.hpp
 @brief Tests for MySQL database operations

*/

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <tuple>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

namespace mysql_test_helpers
{

#if USE_MYSQL

    /**
     * @brief Get MySQL database configuration with test queries
     *
     * Gets a DatabaseConfig object with MySQL connection parameters either from:
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
     * @return cpp_dbc::config::DatabaseConfig with MySQL connection parameters and test queries
     */
    cpp_dbc::config::DatabaseConfig getMySQLConfig(const std::string &databaseName = "dev_mysql",
                                                   bool useEmptyDatabase = false);

    /**
     * @brief Helper function to try to create the database if it doesn't exist
     */
    bool tryCreateDatabase();

    /**
     * @brief Helper function to check if we can connect to MySQL
     */
    bool canConnectToMySQL();

#endif

} // namespace mysql_test_helpers