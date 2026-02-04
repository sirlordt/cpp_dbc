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

 @file yaml_config_loader.hpp
 @brief Class for loading database configuration from YAML files

*/

#ifndef CPP_DBC_YAML_CONFIG_LOADER_HPP
#define CPP_DBC_YAML_CONFIG_LOADER_HPP

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1

#include <cpp_dbc/config/database_config.hpp>
#include <string>

namespace cpp_dbc::config
{

    /**
     * @brief Class for loading database configuration from YAML files.
     *
     * Parses a YAML configuration file and produces a fully populated
     * DatabaseConfigManager containing database definitions, connection pool
     * settings, and test queries. Requires the project to be built with
     * `USE_CPP_YAML=1`.
     *
     * ### Example
     * ```cpp
     * auto configMgr = cpp_dbc::config::YamlConfigLoader::loadFromFile("database.yml");
     * auto conn = configMgr.createDBConnection("mydb");
     * auto pool = configMgr.createDBConnectionPool("mydb");
     * ```
     *
     * @see DatabaseConfigManager
     * @see DatabaseConfig
     * @see DBConnectionPoolConfig
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

} // namespace cpp_dbc::config

#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1

#endif // CPP_DBC_YAML_CONFIG_LOADER_HPP