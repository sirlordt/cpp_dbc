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
 * @file kv_db_connection.hpp
 * @brief Abstract class for key-value database connections
 */

#ifndef CPP_DBC_KV_DB_CONNECTION_HPP
#define CPP_DBC_KV_DB_CONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "cpp_dbc/core/db_connection.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class KVDBData;

    /**
     * @brief Abstract class for key-value database connections
     *
     * This class extends DBConnection with methods specific to key-value stores.
     * It provides CRUD operations for keys and values, as well as methods for
     * working with different data types and structures supported by the underlying
     * key-value store.
     *
     * Implementations: RedisConnection, etc.
     */
    class KVDBConnection : public DBConnection
    {
    public:
        virtual ~KVDBConnection() = default;

        // Basic key-value operations
        /**
         * @brief Set a key to a string value
         * @param key The key
         * @param value The string value
         * @param expirySeconds Optional expiration time in seconds
         * @return true if the operation was successful
         */
        virtual bool setString(const std::string &key, const std::string &value,
                               std::optional<int64_t> expirySeconds = std::nullopt) = 0;

        /**
         * @brief Get the string value of a key
         * @param key The key
         * @return The value as a string, or empty string if the key doesn't exist
         */
        virtual std::string getString(const std::string &key) = 0;

        /**
         * @brief Check if a key exists
         * @param key The key
         * @return true if the key exists
         */
        virtual bool exists(const std::string &key) = 0;

        /**
         * @brief Delete a key
         * @param key The key
         * @return true if the key was deleted, false if it didn't exist
         */
        virtual bool deleteKey(const std::string &key) = 0;

        /**
         * @brief Delete multiple keys
         * @param keys Vector of keys to delete
         * @return Number of keys that were deleted
         */
        virtual int64_t deleteKeys(const std::vector<std::string> &keys) = 0;

        /**
         * @brief Set expiration time on a key
         * @param key The key
         * @param seconds Time to live in seconds
         * @return true if the operation was successful
         */
        virtual bool expire(const std::string &key, int64_t seconds) = 0;

        /**
         * @brief Get the time to live for a key
         * @param key The key
         * @return Time to live in seconds, or -1 if the key has no expiry, -2 if key doesn't exist
         */
        virtual int64_t getTTL(const std::string &key) = 0;

        // Counter operations
        /**
         * @brief Increment the integer value of a key
         * @param key The key
         * @param by The increment value (default: 1)
         * @return The new value after increment
         */
        virtual int64_t increment(const std::string &key, int64_t by = 1) = 0;

        /**
         * @brief Decrement the integer value of a key
         * @param key The key
         * @param by The decrement value (default: 1)
         * @return The new value after decrement
         */
        virtual int64_t decrement(const std::string &key, int64_t by = 1) = 0;

        // List operations
        /**
         * @brief Push an element to the left of a list
         * @param key The key
         * @param value The value to push
         * @return The length of the list after the push
         */
        virtual int64_t listPushLeft(const std::string &key, const std::string &value) = 0;

        /**
         * @brief Push an element to the right of a list
         * @param key The key
         * @param value The value to push
         * @return The length of the list after the push
         */
        virtual int64_t listPushRight(const std::string &key, const std::string &value) = 0;

        /**
         * @brief Pop an element from the left of a list
         * @param key The key
         * @return The popped element, or empty string if the list is empty
         */
        virtual std::string listPopLeft(const std::string &key) = 0;

        /**
         * @brief Pop an element from the right of a list
         * @param key The key
         * @return The popped element, or empty string if the list is empty
         */
        virtual std::string listPopRight(const std::string &key) = 0;

        /**
         * @brief Get a range of elements from a list
         * @param key The key
         * @param start The start index (0-based)
         * @param stop The stop index (inclusive)
         * @return Vector of elements in the specified range
         */
        virtual std::vector<std::string> listRange(const std::string &key, int64_t start, int64_t stop) = 0;

        /**
         * @brief Get the length of a list
         * @param key The key
         * @return The length of the list
         */
        virtual int64_t listLength(const std::string &key) = 0;

        // Hash operations
        /**
         * @brief Set a field in a hash
         * @param key The key of the hash
         * @param field The field to set
         * @param value The value to set
         * @return true if field is a new field in the hash and value was set
         */
        virtual bool hashSet(const std::string &key, const std::string &field, const std::string &value) = 0;

        /**
         * @brief Get a field from a hash
         * @param key The key of the hash
         * @param field The field to get
         * @return The value of the field, or empty string if it doesn't exist
         */
        virtual std::string hashGet(const std::string &key, const std::string &field) = 0;

        /**
         * @brief Delete a field from a hash
         * @param key The key of the hash
         * @param field The field to delete
         * @return true if the field existed and was deleted
         */
        virtual bool hashDelete(const std::string &key, const std::string &field) = 0;

        /**
         * @brief Check if a field exists in a hash
         * @param key The key of the hash
         * @param field The field to check
         * @return true if the field exists
         */
        virtual bool hashExists(const std::string &key, const std::string &field) = 0;

        /**
         * @brief Get all fields and values from a hash
         * @param key The key of the hash
         * @return Map of field-value pairs
         */
        virtual std::map<std::string, std::string> hashGetAll(const std::string &key) = 0;

        /**
         * @brief Get the number of fields in a hash
         * @param key The key of the hash
         * @return The number of fields
         */
        virtual int64_t hashLength(const std::string &key) = 0;

        // Set operations
        /**
         * @brief Add a member to a set
         * @param key The key of the set
         * @param member The member to add
         * @return true if the member was added (didn't already exist)
         */
        virtual bool setAdd(const std::string &key, const std::string &member) = 0;

        /**
         * @brief Remove a member from a set
         * @param key The key of the set
         * @param member The member to remove
         * @return true if the member was removed (existed)
         */
        virtual bool setRemove(const std::string &key, const std::string &member) = 0;

        /**
         * @brief Check if a member exists in a set
         * @param key The key of the set
         * @param member The member to check
         * @return true if the member exists
         */
        virtual bool setIsMember(const std::string &key, const std::string &member) = 0;

        /**
         * @brief Get all members of a set
         * @param key The key of the set
         * @return Vector of all members
         */
        virtual std::vector<std::string> setMembers(const std::string &key) = 0;

        /**
         * @brief Get the number of members in a set
         * @param key The key of the set
         * @return The number of members
         */
        virtual int64_t setSize(const std::string &key) = 0;

        // Sorted set operations
        /**
         * @brief Add a member with score to a sorted set
         * @param key The key of the sorted set
         * @param score The score
         * @param member The member
         * @return true if the member was added or its score was updated
         */
        virtual bool sortedSetAdd(const std::string &key, double score, const std::string &member) = 0;

        /**
         * @brief Remove a member from a sorted set
         * @param key The key of the sorted set
         * @param member The member to remove
         * @return true if the member was removed (existed)
         */
        virtual bool sortedSetRemove(const std::string &key, const std::string &member) = 0;

        /**
         * @brief Get the score of a member in a sorted set
         * @param key The key of the sorted set
         * @param member The member
         * @return The score, or nullopt if the member doesn't exist
         */
        virtual std::optional<double> sortedSetScore(const std::string &key, const std::string &member) = 0;

        /**
         * @brief Get a range of members from a sorted set by rank (ordered by score)
         * @param key The key of the sorted set
         * @param start The start rank (0-based)
         * @param stop The stop rank (inclusive)
         * @return Vector of members in the specified range
         */
        virtual std::vector<std::string> sortedSetRange(const std::string &key, int64_t start, int64_t stop) = 0;

        /**
         * @brief Get the number of members in a sorted set
         * @param key The key of the sorted set
         * @return The number of members
         */
        virtual int64_t sortedSetSize(const std::string &key) = 0;

        // Key scan operations
        /**
         * @brief Scan keys matching a pattern
         * @param pattern The pattern to match (e.g., "user:*")
         * @param count The hint for number of keys to scan per iteration
         * @return Vector of keys matching the pattern
         */
        virtual std::vector<std::string> scanKeys(const std::string &pattern, int64_t count = 10) = 0;

        // Server operations
        /**
         * @brief Execute a server command
         * @param command The command name
         * @param args The command arguments
         * @return The command result as a string
         */
        virtual std::string executeCommand(const std::string &command,
                                           const std::vector<std::string> &args = {}) = 0;

        /**
         * @brief Flush the database (delete all keys)
         * @param async If true, flush asynchronously
         * @return true if the operation was successful
         */
        virtual bool flushDB(bool async = false) = 0;

        /**
         * @brief Ping the server
         * @return Server response ("PONG" for Redis)
         */
        virtual std::string ping() = 0;

        /**
         * @brief Get server information
         * @return Map of server information
         */
        virtual std::map<std::string, std::string> getServerInfo() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_KV_DB_CONNECTION_HPP