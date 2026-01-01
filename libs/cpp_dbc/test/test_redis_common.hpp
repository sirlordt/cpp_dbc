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
 * @file test_redis_common.hpp
 * @brief Tests for Redis database operations
 *
 */

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <tuple>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_main.hpp"

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

namespace redis_test_helpers
{

#if USE_REDIS

    /**
     * @brief Get Redis database configuration
     *
     * Gets a DatabaseConfig object with Redis connection parameters either from:
     * - YAML config file (when USE_CPP_YAML is defined)
     * - Hardcoded default values (when USE_CPP_YAML is not defined)
     *
     * @param databaseName The name to use for the configuration
     * @return cpp_dbc::config::DatabaseConfig with Redis connection parameters
     */
    cpp_dbc::config::DatabaseConfig getRedisConfig(const std::string &databaseName = "test_redis");

    /**
     * @brief Helper function to check if we can connect to Redis
     */
    bool canConnectToRedis();

    /**
     * @brief Build a Redis connection string from a DatabaseConfig
     *
     * Builds a properly formatted Redis connection string including:
     * - Host and port
     * - Database number
     *
     * @param dbConfig The database configuration
     * @return std::string The Redis connection string
     */
    std::string buildRedisConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig);

    /**
     * @brief Helper function to get a Redis driver instance
     */
    std::shared_ptr<cpp_dbc::Redis::RedisDriver> getRedisDriver();

    /**
     * @brief Helper function to get a Redis connection
     */
    std::shared_ptr<cpp_dbc::KVDBConnection> getRedisConnection();

#endif

} // namespace redis_test_helpers