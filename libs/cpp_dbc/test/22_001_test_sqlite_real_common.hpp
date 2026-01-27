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

 @file 22_001_test_sqlite_real_common.hpp
 @brief Tests for SQLite database operations

*/

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <tuple>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#include <cpp_dbc/drivers/relational/sqlite_blob.hpp>
#endif

namespace sqlite_test_helpers
{

#if USE_SQLITE

    /**
     * @brief Get SQLite database configuration with test queries
     *
     * Gets a DatabaseConfig object with SQLite connection parameters either from:
     * - YAML config file (when USE_CPP_YAML is defined)
     * - Hardcoded default values (when USE_CPP_YAML is not defined)
     *
     * @param databaseName The name to use for the configuration
     * @param useInMemory If true, returns configuration with in-memory database
     * @return cpp_dbc::config::DatabaseConfig with SQLite connection parameters
     */
    cpp_dbc::config::DatabaseConfig getSQLiteConfig(const std::string &databaseName = "dev_sqlite",
                                                    bool useInMemory = false);

    /**
     * @brief Helper function to check if we can connect to SQLite
     */
    bool canConnectToSQLite();

#endif

} // namespace sqlite_test_helpers