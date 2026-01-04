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
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/db_expected.hpp"

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

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Set a key to a string value (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param value The string value
         * @param expirySeconds Optional expiration time in seconds
         * @return expected containing true if successful, or DBException on failure
         */
        virtual expected<bool, DBException> setString(
            std::nothrow_t,
            const std::string &key,
            const std::string &value,
            std::optional<int64_t> expirySeconds = std::nullopt) noexcept = 0;

        /**
         * @brief Get the string value of a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing the value as a string, or DBException on failure
         */
        virtual expected<std::string, DBException> getString(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Check if a key exists (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing true if the key exists, or DBException on failure
         */
        virtual expected<bool, DBException> exists(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Delete a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing true if the key was deleted, or DBException on failure
         */
        virtual expected<bool, DBException> deleteKey(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Delete multiple keys (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param keys Vector of keys to delete
         * @return expected containing number of keys that were deleted, or DBException on failure
         */
        virtual expected<int64_t, DBException> deleteKeys(
            std::nothrow_t, const std::vector<std::string> &keys) noexcept = 0;

        /**
         * @brief Set expiration time on a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param seconds Time to live in seconds
         * @return expected containing true if successful, or DBException on failure
         */
        virtual expected<bool, DBException> expire(
            std::nothrow_t, const std::string &key, int64_t seconds) noexcept = 0;

        /**
         * @brief Get the time to live for a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing TTL in seconds, or DBException on failure
         */
        virtual expected<int64_t, DBException> getTTL(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Increment the integer value of a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param by The increment value
         * @return expected containing the new value after increment, or DBException on failure
         */
        virtual expected<int64_t, DBException> increment(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept = 0;

        /**
         * @brief Decrement the integer value of a key (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param by The decrement value
         * @return expected containing the new value after decrement, or DBException on failure
         */
        virtual expected<int64_t, DBException> decrement(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept = 0;

        /**
         * @brief Push an element to the left of a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param value The value to push
         * @return expected containing the length of the list after the push, or DBException on failure
         */
        virtual expected<int64_t, DBException> listPushLeft(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept = 0;

        /**
         * @brief Push an element to the right of a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param value The value to push
         * @return expected containing the length of the list after the push, or DBException on failure
         */
        virtual expected<int64_t, DBException> listPushRight(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept = 0;

        /**
         * @brief Pop an element from the left of a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing the popped element, or DBException on failure
         */
        virtual expected<std::string, DBException> listPopLeft(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Pop an element from the right of a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing the popped element, or DBException on failure
         */
        virtual expected<std::string, DBException> listPopRight(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Get a range of elements from a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @param start The start index
         * @param stop The stop index
         * @return expected containing vector of elements, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> listRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept = 0;

        /**
         * @brief Get the length of a list (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key
         * @return expected containing the length of the list, or DBException on failure
         */
        virtual expected<int64_t, DBException> listLength(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Set a field in a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @param field The field to set
         * @param value The value to set
         * @return expected containing true if field is a new field, or DBException on failure
         */
        virtual expected<bool, DBException> hashSet(
            std::nothrow_t,
            const std::string &key,
            const std::string &field,
            const std::string &value) noexcept = 0;

        /**
         * @brief Get a field from a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @param field The field to get
         * @return expected containing the value of the field, or DBException on failure
         */
        virtual expected<std::string, DBException> hashGet(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept = 0;

        /**
         * @brief Delete a field from a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @param field The field to delete
         * @return expected containing true if the field existed and was deleted, or DBException on failure
         */
        virtual expected<bool, DBException> hashDelete(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept = 0;

        /**
         * @brief Check if a field exists in a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @param field The field to check
         * @return expected containing true if the field exists, or DBException on failure
         */
        virtual expected<bool, DBException> hashExists(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept = 0;

        /**
         * @brief Get all fields and values from a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @return expected containing map of field-value pairs, or DBException on failure
         */
        virtual expected<std::map<std::string, std::string>, DBException> hashGetAll(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Get the number of fields in a hash (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the hash
         * @return expected containing the number of fields, or DBException on failure
         */
        virtual expected<int64_t, DBException> hashLength(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Add a member to a set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the set
         * @param member The member to add
         * @return expected containing true if the member was added, or DBException on failure
         */
        virtual expected<bool, DBException> setAdd(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept = 0;

        /**
         * @brief Remove a member from a set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the set
         * @param member The member to remove
         * @return expected containing true if the member was removed, or DBException on failure
         */
        virtual expected<bool, DBException> setRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept = 0;

        /**
         * @brief Check if a member exists in a set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the set
         * @param member The member to check
         * @return expected containing true if the member exists, or DBException on failure
         */
        virtual expected<bool, DBException> setIsMember(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept = 0;

        /**
         * @brief Get all members of a set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the set
         * @return expected containing vector of all members, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> setMembers(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Get the number of members in a set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the set
         * @return expected containing the number of members, or DBException on failure
         */
        virtual expected<int64_t, DBException> setSize(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Add a member with score to a sorted set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the sorted set
         * @param score The score
         * @param member The member
         * @return expected containing true if the member was added, or DBException on failure
         */
        virtual expected<bool, DBException> sortedSetAdd(
            std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept = 0;

        /**
         * @brief Remove a member from a sorted set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the sorted set
         * @param member The member to remove
         * @return expected containing true if the member was removed, or DBException on failure
         */
        virtual expected<bool, DBException> sortedSetRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept = 0;

        /**
         * @brief Get the score of a member in a sorted set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the sorted set
         * @param member The member
         * @return expected containing the score, or DBException on failure
         */
        virtual expected<std::optional<double>, DBException> sortedSetScore(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept = 0;

        /**
         * @brief Get a range of members from a sorted set by rank (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the sorted set
         * @param start The start rank
         * @param stop The stop rank
         * @return expected containing vector of members, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> sortedSetRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept = 0;

        /**
         * @brief Get the number of members in a sorted set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param key The key of the sorted set
         * @return expected containing the number of members, or DBException on failure
         */
        virtual expected<int64_t, DBException> sortedSetSize(
            std::nothrow_t, const std::string &key) noexcept = 0;

        /**
         * @brief Scan keys matching a pattern (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param pattern The pattern to match
         * @param count The hint for number of keys to scan per iteration
         * @return expected containing vector of keys matching the pattern, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> scanKeys(
            std::nothrow_t, const std::string &pattern, int64_t count = 10) noexcept = 0;

        /**
         * @brief Execute a server command (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param command The command name
         * @param args The command arguments
         * @return expected containing the command result as a string, or DBException on failure
         */
        virtual expected<std::string, DBException> executeCommand(
            std::nothrow_t,
            const std::string &command,
            const std::vector<std::string> &args = {}) noexcept = 0;

        /**
         * @brief Flush the database (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param async If true, flush asynchronously
         * @return expected containing true if successful, or DBException on failure
         */
        virtual expected<bool, DBException> flushDB(
            std::nothrow_t, bool async = false) noexcept = 0;

        /**
         * @brief Ping the server (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing server response, or DBException on failure
         */
        virtual expected<std::string, DBException> ping(std::nothrow_t) noexcept = 0;

        /**
         * @brief Get server information (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing map of server information, or DBException on failure
         */
        virtual expected<std::map<std::string, std::string>, DBException> getServerInfo(
            std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_KV_DB_CONNECTION_HPP
