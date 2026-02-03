#pragma once

#include "cpp_dbc/core/document/document_db_collection.hpp"
#include "cpp_dbc/core/document/document_db_connection.hpp"
#include "cpp_dbc/core/document/document_db_cursor.hpp"
#include "cpp_dbc/core/document/document_db_data.hpp"
#include "cpp_dbc/core/document/document_db_driver.hpp"
#include "cpp_dbc/core/db_exception.hpp"

#if USE_MONGODB

// Suppress warnings from external MongoDB C driver headers
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cpp_dbc::MongoDB
{
        // Forward declarations
        class MongoDBConnection;
        class MongoDBCollection;
        class MongoDBCursor;
        class MongoDBDocument;

        // ============================================================================
        // Custom Deleters for RAII Resource Management
        // ============================================================================

        /**
         * @brief Custom deleter for bson_t* to use with unique_ptr
         *
         * Ensures bson_destroy() is called automatically when the unique_ptr
         * goes out of scope, preventing memory leaks.
         */
        struct BsonDeleter
        {
            void operator()(bson_t *bson) const noexcept
            {
                if (bson)
                {
                    bson_destroy(bson);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing bson_t
         *
         * Note: We use void* as the template parameter to avoid -Wignored-attributes
         * warnings caused by bson_t's alignment attributes. The BsonDeleter handles
         * the proper casting and destruction.
         */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
        using BsonHandle = std::unique_ptr<bson_t, BsonDeleter>;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

        /**
         * @brief Helper function to create a BsonHandle from a new bson_t
         * @return A BsonHandle owning a new empty BSON document
         */
        inline BsonHandle makeBsonHandle()
        {
            return BsonHandle(bson_new());
        }

        /**
         * @brief Helper function to create a BsonHandle from JSON
         * @param json The JSON string to parse
         * @return A BsonHandle owning the parsed BSON document
         * @throws DBException if the JSON is invalid
         */
        inline BsonHandle makeBsonHandleFromJson(const std::string &json)
        {
            bson_error_t error;
            bson_t *bson = bson_new_from_json(
                reinterpret_cast<const uint8_t *>(json.c_str()),
                static_cast<ssize_t>(json.length()),
                &error);

            if (!bson)
            {
                throw DBException("MongoDB", std::string("Failed to parse JSON: ") + error.message);
            }

            return BsonHandle(bson);
        }

        /**
         * @brief Custom deleter for mongoc_client_t* to use with shared_ptr
         *
         * Ensures mongoc_client_destroy() is called automatically.
         * Uses shared_ptr to allow weak_ptr references from child objects.
         */
        struct MongoClientDeleter
        {
            void operator()(mongoc_client_t *client) const noexcept
            {
                if (client)
                {
                    mongoc_client_destroy(client);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_client_t
         */
        using MongoClientHandle = std::shared_ptr<mongoc_client_t>;

        /**
         * @brief Custom deleter for mongoc_collection_t* to use with unique_ptr
         */
        struct MongoCollectionDeleter
        {
            void operator()(mongoc_collection_t *collection) const noexcept
            {
                if (collection)
                {
                    mongoc_collection_destroy(collection);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_collection_t
         */
        using MongoCollectionHandle = std::unique_ptr<mongoc_collection_t, MongoCollectionDeleter>;

        /**
         * @brief Custom deleter for mongoc_cursor_t* to use with unique_ptr
         */
        struct MongoCursorDeleter
        {
            void operator()(mongoc_cursor_t *cursor) const noexcept
            {
                if (cursor)
                {
                    mongoc_cursor_destroy(cursor);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_cursor_t
         */
        using MongoCursorHandle = std::unique_ptr<mongoc_cursor_t, MongoCursorDeleter>;

        /**
         * @brief Custom deleter for mongoc_database_t* to use with unique_ptr
         */
        struct MongoDatabaseDeleter
        {
            void operator()(mongoc_database_t *database) const noexcept
            {
                if (database)
                {
                    mongoc_database_destroy(database);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_database_t
         */
        using MongoDatabaseHandle = std::unique_ptr<mongoc_database_t, MongoDatabaseDeleter>;

        /**
         * @brief Custom deleter for mongoc_client_session_t* to use with unique_ptr
         */
        struct MongoSessionDeleter
        {
            void operator()(mongoc_client_session_t *session) const noexcept
            {
                if (session)
                {
                    mongoc_client_session_destroy(session);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_client_session_t
         */
        using MongoSessionHandle = std::unique_ptr<mongoc_client_session_t, MongoSessionDeleter>;

        /**
         * @brief Custom deleter for mongoc_uri_t* to use with unique_ptr
         */
        struct MongoUriDeleter
        {
            void operator()(mongoc_uri_t *uri) const noexcept
            {
                if (uri)
                {
                    mongoc_uri_destroy(uri);
                }
            }
        };

        /**
         * @brief Type alias for smart pointer managing mongoc_uri_t
         */
        using MongoUriHandle = std::unique_ptr<mongoc_uri_t, MongoUriDeleter>;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared connection mutex type for thread-safe MongoDB operations
         *
         * CRITICAL: MongoDB's libmongoc explicitly states that mongoc_client_t is NOT
         * thread-safe and "You must only use a mongoc_client_t from one thread at a time."
         *
         * This creates a race condition when:
         * - Thread A: cursor->next() uses mongoc_client_t
         * - Thread B: collection->insertOne() uses SAME mongoc_client_t
         * - Thread C: connection->ping() uses SAME mongoc_client_t
         *
         * Without a shared mutex, each object locks independently, but all operations
         * go through the same underlying mongoc_client_t handle.
         *
         * The SharedConnMutex is:
         * - Created by MongoDBConnection
         * - Shared with MongoDBCollection when collections are obtained
         * - Shared with MongoDBCursor when cursors are created
         *
         * This ensures ALL operations on the same mongoc_client_t are serialized,
         * preventing data corruption and crashes from concurrent access.
         */
        using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;
#endif

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
