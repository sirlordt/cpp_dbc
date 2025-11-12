/*

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

 @file database_config.cpp
 @brief Tests for configuration handling

*/

#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/connection_pool.hpp"

namespace cpp_dbc
{
    namespace config
    {
        // DatabaseConfig implementation
        std::shared_ptr<Connection> DatabaseConfig::createConnection() const
        {
            return DriverManager::getConnection(
                createConnectionString(),
                getUsername(),
                getPassword(),
                getOptions());
        }

        // DatabaseConfigManager implementation
        std::shared_ptr<Connection> DatabaseConfigManager::createConnection(const std::string &configName) const
        {
            auto dbConfigOpt = getDatabaseByName(configName);
            if (!dbConfigOpt.has_value())
            {
                return nullptr;
            }

            const auto &dbConfig = dbConfigOpt->get();
            return dbConfig.createConnection();
        }

        std::shared_ptr<ConnectionPool> DatabaseConfigManager::createConnectionPool(
            const std::string &dbConfigName,
            const std::string &poolConfigName) const
        {
            auto dbConfigOpt = getDatabaseByName(dbConfigName);
            if (!dbConfigOpt.has_value())
            {
                return nullptr;
            }

            auto poolConfigOpt = getConnectionPoolConfig(poolConfigName);
            if (!poolConfigOpt.has_value())
            {
                return nullptr;
            }

            // Create a ConnectionPoolConfig with database connection details
            const auto &dbConfig = dbConfigOpt->get();
            const auto &poolConfig = poolConfigOpt->get();
            cpp_dbc::config::ConnectionPoolConfig config = poolConfig;
            config.setUrl(dbConfig.createConnectionString());
            config.setUsername(dbConfig.getUsername());
            config.setPassword(dbConfig.getPassword());

            // Create the connection pool
            return cpp_dbc::ConnectionPool::create(config);
        }
    }
}