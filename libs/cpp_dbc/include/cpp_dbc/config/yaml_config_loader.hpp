// yaml_config_loader.hpp
// YAML configuration loader for cpp_dbc
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#ifndef CPP_DBC_YAML_CONFIG_LOADER_HPP
#define CPP_DBC_YAML_CONFIG_LOADER_HPP

#ifdef USE_CPP_YAML

#include <cpp_dbc/config/database_config.hpp>
#include <string>

namespace cpp_dbc
{
    namespace config
    {

        /**
         * @brief Class for loading database configuration from YAML files
         */
        class YamlConfigLoader
        {
        public:
            /**
             * @brief Load database configuration from a YAML file
             * @param filePath Path to the YAML file
             * @return DatabaseConfigManager with loaded configuration
             * @throws std::runtime_error if file cannot be loaded or has invalid format
             */
            static DatabaseConfigManager loadFromFile(const std::string &filePath);
        };

    } // namespace config
} // namespace cpp_dbc

#endif // USE_CPP_YAML

#endif // CPP_DBC_YAML_CONFIG_LOADER_HPP