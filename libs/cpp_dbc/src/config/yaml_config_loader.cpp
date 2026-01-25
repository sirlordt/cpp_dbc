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

 @file yaml_config_loader.cpp
 @brief Implementation of YAML configuration loading

*/

// This file should only be compiled if USE_CPP_YAML is defined
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1

#include <cpp_dbc/config/yaml_config_loader.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace cpp_dbc::config
{

        DatabaseConfigManager YamlConfigLoader::loadFromFile(const std::string &filePath)
        {
            try
            {
                // Load YAML file
                YAML::Node config = YAML::LoadFile(filePath);

                // Create config manager
                DatabaseConfigManager configManager;

                // Load databases
                if (config["databases"])
                {
                    for (const auto &dbNode : config["databases"])
                    {
                        DatabaseConfig dbConfig;

                        // Check and load required properties
                        if (!dbNode["name"] || !dbNode["name"].IsDefined())
                        {
                            throw cpp_dbc::DBException("E625EF2DF318", "Missing required field 'name' in database configuration",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        dbConfig.setName(dbNode["name"].as<std::string>());

                        if (!dbNode["type"] || !dbNode["type"].IsDefined())
                        {
                            throw cpp_dbc::DBException("4A7390BC7EF7", "Missing required field 'type' in database configuration",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        dbConfig.setType(dbNode["type"].as<std::string>());

                        if (!dbNode["database"] || !dbNode["database"].IsDefined())
                        {
                            throw cpp_dbc::DBException("77398F244B5D", "Missing required field 'database' in database configuration",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        dbConfig.setDatabase(dbNode["database"].as<std::string>());

                        // optional fields host, port, username, password
                        // Check host
                        if (dbNode["host"].IsDefined())
                        {
                            dbConfig.setHost(dbNode["host"].as<std::string>());
                        }

                        // Check port
                        if (dbNode["port"].IsDefined())
                        {
                            dbConfig.setPort(dbNode["port"].as<int>());
                        }

                        // Check username
                        if (dbNode["username"].IsDefined())
                        {
                            dbConfig.setUsername(dbNode["username"].as<std::string>());
                        }

                        // Check password
                        if (dbNode["password"].IsDefined())
                        {
                            dbConfig.setPassword(dbNode["password"].as<std::string>());
                        }

                        // Load options
                        if (dbNode["options"])
                        {
                            for (const auto &option : dbNode["options"])
                            {
                                std::string key = option.first.as<std::string>();
                                // Convert all option values to strings
                                std::string value;

                                if (option.second.IsScalar())
                                {
                                    if (option.second.IsNull())
                                    {
                                        value = "";
                                    }
                                    else
                                    {
                                        value = option.second.as<std::string>();
                                    }
                                }
                                else if (option.second.IsSequence())
                                {
                                    // Convert sequence to comma-separated string
                                    value = "";
                                    for (const auto &item : option.second)
                                    {
                                        if (!value.empty())
                                        {
                                            value += ",";
                                        }
                                        value += item.as<std::string>();
                                    }
                                }
                                else if (option.second.IsMap())
                                {
                                    // Convert map to JSON-like string
                                    value = "{";
                                    bool first = true;
                                    for (const auto &item : option.second)
                                    {
                                        if (!first)
                                        {
                                            value += ",";
                                        }
                                        value += "\"" + item.first.as<std::string>() + "\":\"" +
                                                 item.second.as<std::string>() + "\"";
                                        first = false;
                                    }
                                    value += "}";
                                }

                                dbConfig.setOption(key, value);
                            }
                        }

                        // Add database config to manager
                        configManager.addDatabaseConfig(dbConfig);
                    }
                }

                // Load connection pools
                if (config["connection_pool"])
                {
                    for (const auto &poolNode : config["connection_pool"])
                    {
                        std::string name = poolNode.first.as<std::string>();
                        YAML::Node poolConfig = poolNode.second;

                        DBConnectionPoolConfig poolCfg;
                        poolCfg.setName(name);

                        // Validate required connection pool fields
                        if (!poolConfig["initial_size"] || !poolConfig["initial_size"].IsDefined())
                        {
                            throw cpp_dbc::DBException("A1B2C3D4E5F6", "Missing required field 'initial_size' in connection pool '" + name + "'",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        poolCfg.setInitialSize(poolConfig["initial_size"].as<int>());

                        if (!poolConfig["max_size"] || !poolConfig["max_size"].IsDefined())
                        {
                            throw cpp_dbc::DBException("B2C3D4E5F6A1", "Missing required field 'max_size' in connection pool '" + name + "'",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        poolCfg.setMaxSize(poolConfig["max_size"].as<int>());

                        if (!poolConfig["connection_timeout"] || !poolConfig["connection_timeout"].IsDefined())
                        {
                            throw cpp_dbc::DBException("C3D4E5F6A1B2", "Missing required field 'connection_timeout' in connection pool '" + name + "'",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        poolCfg.setConnectionTimeout(poolConfig["connection_timeout"].as<int>());

                        if (!poolConfig["idle_timeout"] || !poolConfig["idle_timeout"].IsDefined())
                        {
                            throw cpp_dbc::DBException("D4E5F6A1B2C3", "Missing required field 'idle_timeout' in connection pool '" + name + "'",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        poolCfg.setIdleTimeout(poolConfig["idle_timeout"].as<int>());

                        if (!poolConfig["validation_interval"] || !poolConfig["validation_interval"].IsDefined())
                        {
                            throw cpp_dbc::DBException("E5F6A1B2C3D4", "Missing required field 'validation_interval' in connection pool '" + name + "'",
                                                       cpp_dbc::system_utils::captureCallStack());
                        }
                        poolCfg.setValidationInterval(poolConfig["validation_interval"].as<int>());

                        // Load transaction isolation level if present
                        if (poolConfig["transaction_isolation"])
                        {
                            std::string isolationStr = poolConfig["transaction_isolation"].as<std::string>();

                            // Convert to lowercase for case-insensitive comparison
                            std::string isolationLower = isolationStr;
                            std::ranges::transform(isolationLower, isolationLower.begin(),
                                           [](unsigned char c)
                                           { return std::tolower(c); });

                            // Convert string to TransactionIsolationLevel enum
                            if (isolationLower == "none")
                            {
                                poolCfg.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_NONE);
                            }
                            else if (isolationLower == "read_uncommitted")
                            {
                                poolCfg.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED);
                            }
                            else if (isolationLower == "read_committed")
                            {
                                poolCfg.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);
                            }
                            else if (isolationLower == "repeatable_read")
                            {
                                poolCfg.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ);
                            }
                            else if (isolationLower == "serializable")
                            {
                                poolCfg.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
                            }
                            else
                            {
                                throw cpp_dbc::DBException("1W2X3Y4Z5A6B", " Transaction isolation unknown: " + isolationStr, cpp_dbc::system_utils::captureCallStack());
                            }
                        }

                        // Add pool config to manager
                        configManager.addDBConnectionPoolConfig(poolCfg);
                    }
                }

                // Load test queries
                if (config["test_queries"])
                {
                    TestQueries queries;

                    // Load connection test query
                    if (config["test_queries"]["connection_test"])
                    {
                        queries.setConnectionTest(config["test_queries"]["connection_test"].as<std::string>());
                    }

                    // Load database-specific queries
                    for (const auto &dbTypeNode : config["test_queries"])
                    {
                        std::string dbType = dbTypeNode.first.as<std::string>();

                        // Skip connection_test, it's not a database type
                        if (dbType == "connection_test")
                        {
                            continue;
                        }

                        // Load queries for this database type
                        for (const auto &queryNode : dbTypeNode.second)
                        {
                            std::string queryName = queryNode.first.as<std::string>();
                            std::string query = queryNode.second.as<std::string>();

                            queries.setQuery(dbType, queryName, query);
                        }
                    }

                    // Set test queries in manager
                    configManager.setTestQueries(queries);
                }

                return configManager;
            }
            catch (const YAML::Exception &e)
            {
                throw std::runtime_error("7C8D9E0F1G2H: Error loading YAML configuration: " + std::string(e.what()));
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error("3I4J5K6L7M8N: Error loading configuration: " + std::string(e.what()));
            }
        }

} // namespace cpp_dbc::config

#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1