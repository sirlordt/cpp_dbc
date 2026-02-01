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
 * @file driver_mongodb.hpp
 * @brief MongoDB database driver implementation using libmongoc (MongoDB C Driver)
 *
 * This driver provides a safe, thread-safe interface to MongoDB using:
 * - Smart pointers (shared_ptr, weak_ptr, unique_ptr) for memory management
 * - RAII patterns for resource cleanup
 * - Mutex-based thread safety
 * - Exception-safe operations
 *
 * Dependencies:
 * - libmongoc-1.0 (MongoDB C Driver)
 * - libbson-1.0 (BSON library)
 */

#ifndef CPP_DBC_DRIVER_MONGODB_HPP
#define CPP_DBC_DRIVER_MONGODB_HPP

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
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cpp_dbc
{
    namespace MongoDB
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

        // ============================================================================
        // MongoDBDocument - Implements DocumentDBData
        // ============================================================================

        /**
         * @brief MongoDB document implementation
         *
         * This class wraps a BSON document and provides a safe interface for
         * accessing and manipulating document data. It uses smart pointers
         * internally and provides thread-safe operations.
         *
         * Key safety features:
         * - All BSON memory is managed via RAII
         * - Deep copies are made when necessary to prevent dangling references
         * - Thread-safe when DB_DRIVER_THREAD_SAFE is enabled
         */
        class MongoDBDocument final : public DocumentDBData
        {
        private:
            /**
             * @brief The underlying BSON document
             *
             * This is an OWNING pointer that manages the lifecycle of the BSON document.
             * When this pointer is reset or destroyed, bson_destroy() is called automatically.
             */
            BsonHandle m_bson;

            /**
             * @brief Cached document ID for quick access
             */
            mutable std::string m_cachedId;

            /**
             * @brief Flag indicating if the cached ID is valid
             */
            mutable bool m_idCached{false};

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            /**
             * @brief Helper to navigate to a nested field using dot notation
             * @param fieldPath The field path (e.g., "address.city")
             * @param outIter Output iterator positioned at the field
             * @return true if the field was found
             */
            bool navigateToField(const std::string &fieldPath, bson_iter_t &outIter) const;

            /**
             * @brief Helper to set a value at a nested path
             * @param fieldPath The field path
             * @param appendFunc Function to append the value to a BSON builder
             */
            template <typename AppendFunc>
            void setFieldValue(const std::string &fieldPath, AppendFunc appendFunc);

            /**
             * @brief Validates that the BSON document is valid
             * @throws DBException if m_bson is nullptr
             */
            void validateDocument() const;

        public:
            /**
             * @brief Construct an empty document
             */
            MongoDBDocument();

            /**
             * @brief Construct a document from an existing BSON document
             * @param bson The BSON document (ownership is transferred)
             * @note The bson pointer will be managed by this object
             */
            explicit MongoDBDocument(bson_t *bson);

            /**
             * @brief Construct a document from a JSON string
             * @param json The JSON string to parse
             * @throws DBException if the JSON is invalid
             */
            explicit MongoDBDocument(const std::string &json);

            /**
             * @brief Copy constructor - creates a deep copy
             */
            MongoDBDocument(const MongoDBDocument &other);

            /**
             * @brief Move constructor
             */
            MongoDBDocument(MongoDBDocument &&other) noexcept;

            /**
             * @brief Copy assignment - creates a deep copy
             */
            MongoDBDocument &operator=(const MongoDBDocument &other);

            /**
             * @brief Move assignment
             */
            MongoDBDocument &operator=(MongoDBDocument &&other) noexcept;

            ~MongoDBDocument() override = default;

            // DocumentDBData interface implementation
            std::string getId() const override;
            void setId(const std::string &id) override;

            std::string toJson() const override;
            std::string toJsonPretty() const override;
            void fromJson(const std::string &json) override;

            std::string getString(const std::string &fieldPath) const override;
            int64_t getInt(const std::string &fieldPath) const override;
            double getDouble(const std::string &fieldPath) const override;
            bool getBool(const std::string &fieldPath) const override;
            std::vector<uint8_t> getBinary(const std::string &fieldPath) const override;

            std::shared_ptr<DocumentDBData> getDocument(const std::string &fieldPath) const override;
            std::vector<std::shared_ptr<DocumentDBData>> getDocumentArray(const std::string &fieldPath) const override;
            std::vector<std::string> getStringArray(const std::string &fieldPath) const override;

            void setString(const std::string &fieldPath, const std::string &value) override;
            void setInt(const std::string &fieldPath, int64_t value) override;
            void setDouble(const std::string &fieldPath, double value) override;
            void setBool(const std::string &fieldPath, bool value) override;
            void setBinary(const std::string &fieldPath, const std::vector<uint8_t> &value) override;
            void setDocument(const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc) override;
            void setNull(const std::string &fieldPath) override;

            bool hasField(const std::string &fieldPath) const override;
            bool isNull(const std::string &fieldPath) const override;
            bool removeField(const std::string &fieldPath) override;
            std::vector<std::string> getFieldNames() const override;

            std::shared_ptr<DocumentDBData> clone() const override;
            void clear() override;
            bool isEmpty() const override;

            // MongoDB-specific methods

            /**
             * @brief Get the underlying BSON document (const)
             * @return Const pointer to the BSON document
             * @note The returned pointer is valid only while this object exists
             */
            const bson_t *getBson() const;

            /**
             * @brief Get the underlying BSON document (non-const)
             * @return Pointer to the BSON document
             * @note Use with caution - modifications may invalidate cached data
             */
            bson_t *getBsonMutable();

            /**
             * @brief Create a document from a BSON document (takes ownership)
             * @param bson The BSON document
             * @return A shared pointer to the new document
             */
            static std::shared_ptr<MongoDBDocument> fromBson(bson_t *bson);

            /**
             * @brief Create a document from a const BSON document (makes a copy)
             * @param bson The BSON document
             * @return A shared pointer to the new document
             */
            static std::shared_ptr<MongoDBDocument> fromBsonCopy(const bson_t *bson);

            // Nothrow versions
            expected<std::string, DBException> getString(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<int64_t, DBException> getInt(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<double, DBException> getDouble(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<bool, DBException> getBool(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<std::vector<uint8_t>, DBException> getBinary(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> getDocument(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getDocumentArray(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<std::vector<std::string>, DBException> getStringArray(std::nothrow_t, const std::string &fieldPath) const noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> clone(std::nothrow_t) const noexcept override;
        };

        // ============================================================================
        // MongoDBCursor - Implements DocumentDBCursor
        // ============================================================================

        /**
         * @brief MongoDB cursor implementation
         *
         * This class wraps a MongoDB cursor and provides safe iteration over
         * query results. It uses weak_ptr to detect when the connection is closed.
         *
         * Key safety features:
         * - Uses weak_ptr to connection to detect disconnection
         * - Thread-safe iteration when DB_DRIVER_THREAD_SAFE is enabled
         * - Automatic resource cleanup via RAII
         */
        class MongoDBCursor final : public DocumentDBCursor
        {
        private:
            /**
             * @brief Weak reference to the MongoDB client
             *
             * This allows us to detect when the connection has been closed
             * and prevent use-after-free errors.
             */
            std::weak_ptr<mongoc_client_t> m_client;

            /**
             * @brief Weak pointer to the connection for registration/unregistration
             *
             * Using weak_ptr prevents circular references and allows safe
             * detection of connection closure.
             */
            std::weak_ptr<MongoDBConnection> m_connection;

            /**
             * @brief The underlying MongoDB cursor
             */
            MongoCursorHandle m_cursor;

            /**
             * @brief The current document (cached)
             */
            std::shared_ptr<MongoDBDocument> m_currentDoc;

            /**
             * @brief Current position in the cursor (0-based)
             */
            uint64_t m_position{0};

            /**
             * @brief Flag indicating if iteration has started
             */
            bool m_iterationStarted{false};

            /**
             * @brief Flag indicating if the cursor is exhausted
             */
            bool m_exhausted{false};

            /**
             * @brief Skip count (for cursor modifiers)
             */
            uint64_t m_skipCount{0};

            /**
             * @brief Limit count (for cursor modifiers)
             */
            uint64_t m_limitCount{0};

            /**
             * @brief Sort specification (JSON string)
             */
            std::string m_sortSpec;

            /**
             * @brief Flag indicating if modifiers have been applied
             */
            bool m_modifiersApplied{false};

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            /**
             * @brief Validates that the connection is still valid
             * @throws DBException if the connection has been closed
             */
            void validateConnection() const;

            /**
             * @brief Validates that the cursor is valid
             * @throws DBException if the cursor is nullptr
             */
            void validateCursor() const;

            /**
             * @brief Helper to get the client pointer safely
             * @return The client pointer
             * @throws DBException if the connection has been closed
             */
            mongoc_client_t *getClient() const;

        public:
            /**
             * @brief Construct a cursor from a MongoDB cursor
             * @param client Weak reference to the MongoDB client
             * @param cursor The MongoDB cursor (ownership is transferred)
             * @param connection Weak pointer to the connection for registration (optional)
             */
            MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor, std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>());

            ~MongoDBCursor() override;

            // Prevent copying (cursors are not copyable)
            MongoDBCursor(const MongoDBCursor &) = delete;
            MongoDBCursor &operator=(const MongoDBCursor &) = delete;

            // Allow moving
            MongoDBCursor(MongoDBCursor &&other) noexcept;
            MongoDBCursor &operator=(MongoDBCursor &&other) noexcept;

            // DBResultSet interface
            void close() override;
            bool isEmpty() override;

            // DocumentDBCursor interface
            bool next() override;
            bool hasNext() override;
            std::shared_ptr<DocumentDBData> current() override;
            std::shared_ptr<DocumentDBData> nextDocument() override;

            std::vector<std::shared_ptr<DocumentDBData>> toVector() override;
            std::vector<std::shared_ptr<DocumentDBData>> getBatch(size_t batchSize) override;

            int64_t count() override;
            uint64_t getPosition() override;

            DocumentDBCursor &skip(uint64_t n) override;
            DocumentDBCursor &limit(uint64_t n) override;
            DocumentDBCursor &sort(const std::string &fieldPath, bool ascending = true) override;

            bool isExhausted() override;
            [[noreturn]] void rewind() override;

            // MongoDB-specific methods

            /**
             * @brief Check if the connection is still valid
             * @return true if the connection is valid
             */
            bool isConnectionValid() const;

            /**
             * @brief Get any error from the cursor
             * @return The error message, or empty string if no error
             */
            std::string getError() const;

            // Nothrow versions
            expected<std::shared_ptr<DocumentDBData>, DBException> current(std::nothrow_t) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> nextDocument(std::nothrow_t) noexcept override;
            expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> toVector(std::nothrow_t) noexcept override;
            expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getBatch(
                std::nothrow_t, size_t batchSize) noexcept override;
        };

        // ============================================================================
        // MongoDBCollection - Implements DocumentDBCollection
        // ============================================================================

        /**
         * @brief MongoDB collection implementation
         *
         * This class wraps a MongoDB collection and provides safe CRUD operations.
         * It uses weak_ptr to detect when the connection is closed.
         *
         * Key safety features:
         * - Uses weak_ptr to connection to detect disconnection
         * - Thread-safe operations when DB_DRIVER_THREAD_SAFE is enabled
         * - All operations validate connection state before proceeding
         */
        class MongoDBCollection final : public DocumentDBCollection
        {
        private:
            /**
             * @brief Weak reference to the MongoDB client
             */
            std::weak_ptr<mongoc_client_t> m_client;

            /**
             * @brief Weak pointer to the connection for cursor registration
             *
             * Using weak_ptr prevents circular references and allows safe
             * detection of connection closure.
             */
            std::weak_ptr<MongoDBConnection> m_connection;

            /**
             * @brief The underlying MongoDB collection
             */
            MongoCollectionHandle m_collection;

            /**
             * @brief The collection name
             */
            std::string m_name;

            /**
             * @brief The database name
             */
            std::string m_databaseName;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            /**
             * @brief Validates that the connection is still valid
             * @throws DBException if the connection has been closed
             */
            void validateConnection() const;

            /**
             * @brief Helper to get the client pointer safely
             * @return The client pointer
             * @throws DBException if the connection has been closed
             */
            mongoc_client_t *getClient() const;

            /**
             * @brief Helper to parse a JSON filter string to BSON
             * @param filter The JSON filter string
             * @return A BsonHandle containing the parsed filter
             * @throws DBException if the JSON is invalid
             */
            BsonHandle parseFilter(const std::string &filter) const;

            /**
             * @brief Helper to handle MongoDB errors
             * @param error The bson_error_t from MongoDB
             * @param operation The operation that failed
             * @throws DBException with the error details
             */
            [[noreturn]] void throwMongoError(const bson_error_t &error, const std::string &operation) const;

        public:
            /**
             * @brief Construct a collection wrapper
             * @param client Weak reference to the MongoDB client
             * @param collection The MongoDB collection (ownership is transferred)
             * @param name The collection name
             * @param databaseName The database name
             * @param connection Weak pointer to the connection for cursor registration (optional)
             */
            MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                              mongoc_collection_t *collection,
                              const std::string &name,
                              const std::string &databaseName,
                              std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>());

            ~MongoDBCollection() override = default;

            // Prevent copying
            MongoDBCollection(const MongoDBCollection &) = delete;
            MongoDBCollection &operator=(const MongoDBCollection &) = delete;

            // Allow moving
            MongoDBCollection(MongoDBCollection &&other) noexcept;
            MongoDBCollection &operator=(MongoDBCollection &&other) noexcept;

            // DocumentDBCollection interface
            std::string getName() const override;
            std::string getNamespace() const override;
            uint64_t estimatedDocumentCount() override;
            uint64_t countDocuments(const std::string &filter = "") override;

            // ====================================================================
            // INSERT OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
            // ====================================================================

            expected<DocumentInsertResult, DBException> insertOne(
                std::nothrow_t,
                std::shared_ptr<DocumentDBData> document,
                const DocumentWriteOptions &options = DocumentWriteOptions()) noexcept override;

            expected<DocumentInsertResult, DBException> insertOne(
                std::nothrow_t,
                const std::string &jsonDocument,
                const DocumentWriteOptions &options = DocumentWriteOptions()) noexcept override;

            expected<DocumentInsertResult, DBException> insertMany(
                std::nothrow_t,
                const std::vector<std::shared_ptr<DocumentDBData>> &documents,
                const DocumentWriteOptions &options = DocumentWriteOptions()) noexcept override;

            // ====================================================================
            // INSERT OPERATIONS - exception versions (WRAPPERS)
            // ====================================================================

            DocumentInsertResult insertOne(
                std::shared_ptr<DocumentDBData> document,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            DocumentInsertResult insertOne(
                const std::string &jsonDocument,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            DocumentInsertResult insertMany(
                const std::vector<std::shared_ptr<DocumentDBData>> &documents,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            // ====================================================================
            // FIND OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
            // ====================================================================

            expected<std::shared_ptr<DocumentDBData>, DBException>
            findOne(std::nothrow_t, const std::string &filter = "") noexcept override;

            expected<std::shared_ptr<DocumentDBData>, DBException>
            findById(std::nothrow_t, const std::string &id) noexcept override;

            expected<std::shared_ptr<DocumentDBCursor>, DBException>
            find(std::nothrow_t, const std::string &filter = "") noexcept override;

            expected<std::shared_ptr<DocumentDBCursor>, DBException>
            find(std::nothrow_t, const std::string &filter, const std::string &projection) noexcept override;

            // ====================================================================
            // FIND OPERATIONS - exception versions (WRAPPERS)
            // ====================================================================

            std::shared_ptr<DocumentDBData> findOne(const std::string &filter = "") override;
            std::shared_ptr<DocumentDBData> findById(const std::string &id) override;
            std::shared_ptr<DocumentDBCursor> find(const std::string &filter = "") override;
            std::shared_ptr<DocumentDBCursor> find(
                const std::string &filter,
                const std::string &projection) override;

            // ====================================================================
            // UPDATE OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
            // ====================================================================

            expected<DocumentUpdateResult, DBException> updateOne(
                std::nothrow_t,
                const std::string &filter,
                const std::string &update,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) noexcept override;

            expected<DocumentUpdateResult, DBException> updateMany(
                std::nothrow_t,
                const std::string &filter,
                const std::string &update,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) noexcept override;

            expected<DocumentUpdateResult, DBException> replaceOne(
                std::nothrow_t,
                const std::string &filter,
                std::shared_ptr<DocumentDBData> replacement,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) noexcept override;

            // ====================================================================
            // UPDATE OPERATIONS - exception versions (WRAPPERS)
            // ====================================================================

            DocumentUpdateResult updateOne(
                const std::string &filter,
                const std::string &update,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) override;

            DocumentUpdateResult updateMany(
                const std::string &filter,
                const std::string &update,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) override;

            DocumentUpdateResult replaceOne(
                const std::string &filter,
                std::shared_ptr<DocumentDBData> replacement,
                const DocumentUpdateOptions &options = DocumentUpdateOptions()) override;

            // ====================================================================
            // DELETE OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
            // ====================================================================

            expected<DocumentDeleteResult, DBException> deleteOne(
                std::nothrow_t,
                const std::string &filter) noexcept override;

            expected<DocumentDeleteResult, DBException> deleteMany(
                std::nothrow_t,
                const std::string &filter) noexcept override;

            expected<DocumentDeleteResult, DBException> deleteById(
                std::nothrow_t,
                const std::string &id) noexcept override;

            // ====================================================================
            // DELETE OPERATIONS - exception versions (WRAPPERS)
            // ====================================================================

            DocumentDeleteResult deleteOne(const std::string &filter) override;
            DocumentDeleteResult deleteMany(const std::string &filter) override;
            DocumentDeleteResult deleteById(const std::string &id) override;

            // ====================================================================
            // INDEX & COLLECTION OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
            // ====================================================================

            expected<std::string, DBException> createIndex(
                std::nothrow_t,
                const std::string &keys,
                const std::string &options = "") noexcept override;

            expected<void, DBException> dropIndex(
                std::nothrow_t,
                const std::string &indexName) noexcept override;

            expected<void, DBException> dropAllIndexes(
                std::nothrow_t) noexcept override;

            expected<std::vector<std::string>, DBException> listIndexes(
                std::nothrow_t) noexcept override;

            expected<void, DBException> drop(
                std::nothrow_t) noexcept override;

            expected<void, DBException> rename(
                std::nothrow_t,
                const std::string &newName,
                bool dropTarget = false) noexcept override;

            expected<std::shared_ptr<DocumentDBCursor>, DBException> aggregate(
                std::nothrow_t,
                const std::string &pipeline) noexcept override;

            expected<std::vector<std::string>, DBException> distinct(
                std::nothrow_t,
                const std::string &fieldPath,
                const std::string &filter = "") noexcept override;

            // ====================================================================
            // INDEX & COLLECTION OPERATIONS - exception versions (WRAPPERS)
            // ====================================================================

            std::string createIndex(
                const std::string &keys,
                const std::string &options = "") override;

            void dropIndex(const std::string &indexName) override;
            void dropAllIndexes() override;
            std::vector<std::string> listIndexes() override;

            void drop() override;
            void rename(const std::string &newName, bool dropTarget = false) override;

            std::shared_ptr<DocumentDBCursor> aggregate(const std::string &pipeline) override;
            std::vector<std::string> distinct(
                const std::string &fieldPath,
                const std::string &filter = "") override;

            // MongoDB-specific methods

            /**
             * @brief Check if the connection is still valid
             * @return true if the connection is valid
             */
            bool isConnectionValid() const;
        };

        // ============================================================================
        // MongoDBConnection - Implements DocumentDBConnection
        // ============================================================================

        /**
         * @brief MongoDB connection implementation
         *
         * This class manages a connection to a MongoDB server and provides
         * access to databases and collections.
         *
         * Key safety features:
         * - Uses shared_ptr for client to allow weak_ptr references
         * - Inherits from enable_shared_from_this to allow weak_ptr to connection
         * - Thread-safe operations when DB_DRIVER_THREAD_SAFE is enabled
         * - Proper cleanup of all resources on close
         * - Session management for transactions
         */
        class MongoDBConnection final : public DocumentDBConnection, public std::enable_shared_from_this<MongoDBConnection>
        {
        private:
            /**
             * @brief The MongoDB client (shared_ptr for weak_ptr support)
             */
            MongoClientHandle m_client;

            /**
             * @brief The current database name
             */
            std::string m_databaseName;

            /**
             * @brief The connection URL
             */
            std::string m_url;

            /**
             * @brief Flag indicating if the connection is closed
             */
            std::atomic<bool> m_closed{true};

            /**
             * @brief Flag indicating if the connection is pooled
             */
            bool m_pooled{false};

            /**
             * @brief Active sessions (for transaction support)
             */
            std::map<std::string, MongoSessionHandle> m_sessions;

            /**
             * @brief Mutex for session management
             */
            std::mutex m_sessionsMutex;

            /**
             * @brief Counter for generating unique session IDs
             */
            std::atomic<uint64_t> m_sessionCounter{0};

            /**
             * @brief Active collections (weak references for cleanup tracking)
             */
            std::set<std::weak_ptr<MongoDBCollection>, std::owner_less<std::weak_ptr<MongoDBCollection>>> m_activeCollections;

            /**
             * @brief Mutex for collection tracking
             */
            std::mutex m_collectionsMutex;

            /**
             * @brief Active cursors (raw pointers for cleanup tracking)
             * We use raw pointers because cursors hold weak_ptr to client
             */
            std::set<MongoDBCursor *> m_activeCursors;

            /**
             * @brief Mutex for cursor tracking
             */
            std::mutex m_cursorsMutex;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_connMutex;
#endif

            /**
             * @brief Validates that the connection is open
             * @throws DBException if the connection is closed
             */
            void validateConnection() const;

            /**
             * @brief Generate a unique session ID
             * @return A unique session ID string
             */
            std::string generateSessionId();

        public:
            /**
             * @brief Register a cursor for cleanup tracking
             * @param cursor Pointer to the cursor to register
             */
            void registerCursor(MongoDBCursor *cursor);

            /**
             * @brief Unregister a cursor from cleanup tracking
             * @param cursor Pointer to the cursor to unregister
             */
            void unregisterCursor(MongoDBCursor *cursor);

            /**
             * @brief Construct a MongoDB connection
             * @param uri The MongoDB connection URI
             * @param user The username (may be empty if auth is in URI)
             * @param password The password (may be empty if auth is in URI)
             * @param options Additional connection options
             * @throws DBException if the connection fails
             */
            MongoDBConnection(const std::string &uri,
                              const std::string &user,
                              const std::string &password,
                              const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

            ~MongoDBConnection() override;

            // Prevent copying
            MongoDBConnection(const MongoDBConnection &) = delete;
            MongoDBConnection &operator=(const MongoDBConnection &) = delete;

            // Allow moving
            MongoDBConnection(MongoDBConnection &&other) noexcept;
            MongoDBConnection &operator=(MongoDBConnection &&other) noexcept;

            // DBConnection interface
            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;
            std::string getURL() const override;

            // DocumentDBConnection interface
            std::string getDatabaseName() const override;
            std::vector<std::string> listDatabases() override;
            bool databaseExists(const std::string &databaseName) override;
            void useDatabase(const std::string &databaseName) override;
            void dropDatabase(const std::string &databaseName) override;

            std::shared_ptr<DocumentDBCollection> getCollection(const std::string &collectionName) override;
            std::vector<std::string> listCollections() override;
            bool collectionExists(const std::string &collectionName) override;
            std::shared_ptr<DocumentDBCollection> createCollection(
                const std::string &collectionName,
                const std::string &options = "") override;
            void dropCollection(const std::string &collectionName) override;

            std::shared_ptr<DocumentDBData> createDocument() override;
            std::shared_ptr<DocumentDBData> createDocument(const std::string &json) override;

            std::shared_ptr<DocumentDBData> runCommand(const std::string &command) override;

            std::shared_ptr<DocumentDBData> getServerInfo() override;
            std::shared_ptr<DocumentDBData> getServerStatus() override;
            bool ping() override;

            std::string startSession() override;
            void endSession(const std::string &sessionId) override;
            void startTransaction(const std::string &sessionId) override;
            void commitTransaction(const std::string &sessionId) override;
            void abortTransaction(const std::string &sessionId) override;
            bool supportsTransactions() override;

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            expected<std::vector<std::string>, DBException> listDatabases(std::nothrow_t) noexcept override;
            expected<std::shared_ptr<DocumentDBCollection>, DBException> getCollection(
                std::nothrow_t, const std::string &collectionName) noexcept override;
            expected<std::vector<std::string>, DBException> listCollections(std::nothrow_t) noexcept override;
            expected<std::shared_ptr<DocumentDBCollection>, DBException> createCollection(
                std::nothrow_t,
                const std::string &collectionName,
                const std::string &options = "") noexcept override;
            expected<void, DBException> dropCollection(
                std::nothrow_t, const std::string &collectionName) noexcept override;
            expected<void, DBException> dropDatabase(
                std::nothrow_t, const std::string &databaseName) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(std::nothrow_t) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(
                std::nothrow_t, const std::string &json) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> runCommand(
                std::nothrow_t, const std::string &command) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> getServerInfo(std::nothrow_t) noexcept override;
            expected<std::shared_ptr<DocumentDBData>, DBException> getServerStatus(std::nothrow_t) noexcept override;

            // MongoDB-specific methods

            /**
             * @brief Get the underlying MongoDB client
             * @return Weak pointer to the client
             * @note Use this to pass to child objects (collections, cursors)
             */
            std::weak_ptr<mongoc_client_t> getClientWeak() const;

            /**
             * @brief Get the MongoDB client (shared_ptr)
             * @return Shared pointer to the client
             * @note Use with caution - prefer getClientWeak() for child objects
             */
            MongoClientHandle getClient() const;

            /**
             * @brief Set whether this connection is pooled
             * @param pooled true if the connection is managed by a pool
             */
            void setPooled(bool pooled);
        };

        // ============================================================================
        // MongoDBDriver - Implements DocumentDBDriver
        // ============================================================================

        /**
         * @brief MongoDB driver implementation
         *
         * This class provides the entry point for creating MongoDB connections.
         * It handles MongoDB library initialization and cleanup.
         *
         * Key safety features:
         * - Thread-safe initialization using call_once
         * - Proper library cleanup on destruction
         * - URI parsing and validation
         */
        class MongoDBDriver final : public DocumentDBDriver
        {
        private:
            /**
             * @brief Flag indicating if mongoc has been initialized
             */
            static std::once_flag s_initFlag;

            /**
             * @brief Flag indicating if mongoc should be cleaned up
             */
            static std::atomic<bool> s_initialized;

            /**
             * @brief Initialize the MongoDB C driver library
             */
            static void initializeMongoc();

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

        public:
            /**
             * @brief Construct a MongoDB driver
             *
             * This will initialize the MongoDB C driver library if not already done.
             */
            MongoDBDriver();

            /**
             * @brief Destructor
             *
             * Note: mongoc_cleanup() is NOT called here because it should only
             * be called once when the application exits. Use cleanup() explicitly
             * if needed.
             */
            ~MongoDBDriver() override;

            // Prevent copying
            MongoDBDriver(const MongoDBDriver &) = delete;
            MongoDBDriver &operator=(const MongoDBDriver &) = delete;

            // DBDriver interface
            std::shared_ptr<DBConnection> connect(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // DocumentDBDriver interface
            std::shared_ptr<DocumentDBConnection> connectDocument(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            int getDefaultPort() const override;
            std::string getURIScheme() const override;
            std::map<std::string, std::string> parseURI(const std::string &uri) override;
            std::string buildURI(
                const std::string &host,
                int port,
                const std::string &database,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool supportsReplicaSets() const override;
            bool supportsSharding() const override;
            std::string getDriverVersion() const override;

            // MongoDB-specific methods

            /**
             * @brief Explicitly cleanup the MongoDB C driver library
             *
             * This should only be called once when the application is exiting.
             * After calling this, no more MongoDB operations should be performed.
             */
            static void cleanup();

            /**
             * @brief Check if the MongoDB library has been initialized
             * @return true if initialized
             */
            static bool isInitialized();

            /**
             * @brief Validate a MongoDB URI
             * @param uri The URI to validate
             * @return true if the URI is valid
             */
            static bool validateURI(const std::string &uri);

            // Nothrow versions
            expected<std::shared_ptr<DocumentDBConnection>, DBException> connectDocument(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            expected<std::map<std::string, std::string>, DBException> parseURI(
                std::nothrow_t, const std::string &uri) noexcept override;

            std::string getName() const noexcept override;
        };

    } // namespace MongoDB
} // namespace cpp_dbc

#else // USE_MONGODB

// Stub implementations when MongoDB is disabled
namespace cpp_dbc
{
    namespace MongoDB
    {
        /**
         * @brief Stub MongoDB driver when MongoDB support is disabled
         * This class throws an exception on any operation, indicating that
         * MongoDB support was not compiled into the library.*
         */
        class MongoDBDriver final : public DocumentDBDriver
        {
        public:
            MongoDBDriver()
            {
                throw DBException("F3E813E10E8C", "MongoDB support is not enabled in this build");
            }

            ~MongoDBDriver() override = default;

            MongoDBDriver(const MongoDBDriver &) = delete;
            MongoDBDriver &operator=(const MongoDBDriver &) = delete;
            MongoDBDriver(MongoDBDriver &&) = delete;
            MongoDBDriver &operator=(MongoDBDriver &&) = delete;

            std::shared_ptr<DBConnection> connect(
                const std::string &,
                const std::string &,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("AC208113FF23", "MongoDB support is not enabled in this build");
            }

            bool acceptsURL(const std::string &) override
            {
                return false;
            }

            std::shared_ptr<DocumentDBConnection> connectDocument(
                const std::string &,
                const std::string &,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("2CC107C18A39", "MongoDB support is not enabled in this build");
            }

            int getDefaultPort() const override
            {
                return 27017;
            }

            std::string getURIScheme() const override
            {
                return "mongodb";
            }

            std::map<std::string, std::string> parseURI(const std::string &) override
            {
                throw DBException("1BB61E9DD031", "MongoDB support is not enabled in this build");
            }

            std::string buildURI(
                const std::string &,
                int,
                const std::string &,
                const std::map<std::string, std::string> & = {}) override
            {
                throw DBException("11202995FCE7", "MongoDB support is not enabled in this build");
            }

            bool supportsReplicaSets() const override
            {
                return false;
            }

            bool supportsSharding() const override
            {
                return false;
            }

            std::string getDriverVersion() const override
            {
                return "disabled";
            }

            std::string getName() const noexcept override;
        };

    } // namespace MongoDB

} // namespace cpp_dbc

#endif // USE_MONGODB

#endif // CPP_DBC_DRIVER_MONGODB_HPP
