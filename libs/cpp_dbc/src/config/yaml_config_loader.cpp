// yaml_config_loader.cpp
// Implementation of YAML configuration loader for cpp_dbc
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

// This file should only be compiled if USE_CPP_YAML is defined
#ifdef USE_CPP_YAML

#include <cpp_dbc/config/yaml_config_loader.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace cpp_dbc
{
    namespace config
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

                        // Load basic properties
                        dbConfig.setName(dbNode["name"].as<std::string>());
                        dbConfig.setType(dbNode["type"].as<std::string>());
                        dbConfig.setHost(dbNode["host"].as<std::string>());
                        dbConfig.setPort(dbNode["port"].as<int>());
                        dbConfig.setDatabase(dbNode["database"].as<std::string>());
                        dbConfig.setUsername(dbNode["username"].as<std::string>());
                        dbConfig.setPassword(dbNode["password"].as<std::string>());

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

                        ConnectionPoolConfig poolCfg;
                        poolCfg.setName(name);
                        poolCfg.setInitialSize(poolConfig["initial_size"].as<int>());
                        poolCfg.setMaxSize(poolConfig["max_size"].as<int>());
                        poolCfg.setConnectionTimeout(poolConfig["connection_timeout"].as<int>());
                        poolCfg.setIdleTimeout(poolConfig["idle_timeout"].as<int>());
                        poolCfg.setValidationInterval(poolConfig["validation_interval"].as<int>());

                        // Load transaction isolation level if present
                        if (poolConfig["transaction_isolation"])
                        {
                            std::string isolationStr = poolConfig["transaction_isolation"].as<std::string>();

                            // Convert to lowercase for case-insensitive comparison
                            std::string isolationLower = isolationStr;
                            std::transform(isolationLower.begin(), isolationLower.end(), isolationLower.begin(),
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
                                throw cpp_dbc::DBException("1W2X3Y4Z5A6B: Transaction isolation unknown: " + isolationStr);
                            }
                        }

                        // Add pool config to manager
                        configManager.addConnectionPoolConfig(poolCfg);
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

    } // namespace config
} // namespace cpp_dbc

#endif // USE_CPP_YAML