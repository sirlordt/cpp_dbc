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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file 21_001_example_postgresql_basic.cpp
 * @brief PostgreSQL-specific example demonstrating basic CRUD operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - PostgreSQL connection
 * - Creating tables
 * - Inserting data with prepared statements
 * - Querying and displaying results
 * - Transaction management (commit/rollback)
 *
 * Usage:
 *   ./21_001_example_postgresql_basic [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - PostgreSQL support not enabled at compile time
 */

#include "../example_relational_common.hpp"

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
#if !USE_POSTGRESQL
    logError("PostgreSQL support is not enabled");
    logInfo("Build with -DUSE_POSTGRESQL=ON to enable PostgreSQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,postgres");
    return EXIT_DRIVER_NOT_ENABLED_;
#else
    // Run the example with PostgreSQL-specific operations
    return relational::runRelationalExample(
        argc, argv,
        "postgresql",
        "PostgreSQL Basic Example",
        relational::performCrudOperations);
#endif
}
