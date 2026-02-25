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
#include <optional>
#include <tuple>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
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

    /**
     * @brief Cleanup SQLite database files before test execution
     *
     * Removes all SQLite database files associated with the given path:
     * - Main database file (.db)
     * - Write-Ahead Log file (.db-wal)
     * - Shared memory file (.db-shm)
     * - Journal file (.db-journal)
     *
     * After deletion, waits for the specified number of seconds to ensure:
     * - Filesystem buffers are flushed to disk
     * - WAL (Write-Ahead Log) background processes complete
     * - Shared memory segments are properly released
     * - File locks are released by the kernel
     *
     * This is CRITICAL when running multiple test iterations under Helgrind/Valgrind
     * because WAL mode can leave database files in inconsistent state, causing
     * "attempt to write a readonly database" errors on subsequent runs.
     *
     * Call this function at the START of each SQLite test case before creating connections.
     *
     * @param dbPath Full path to the SQLite database file (e.g., "/tmp/test.db")
     * @param waitSeconds Number of seconds to wait after cleanup (default: 10)
     *
     * @note Uses std::filesystem::remove with error_code to avoid exceptions if files don't exist
     *
     * @example
     * ```cpp
     * // Cleanup with default 10 second wait
     * cleanupSQLiteTestFiles("/tmp/test_sqlite.db");
     *
     * // Cleanup with custom 5 second wait
     * cleanupSQLiteTestFiles("/tmp/my_test.db", 5);
     * ```
     */
    void cleanupSQLiteTestFiles(const std::string &dbPath, int waitSeconds = 3);

#endif

} // namespace sqlite_test_helpers