#pragma once

#include "handles.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
        class MongoDBConnection; // Forward declaration
        class MongoDBCursor;     // Forward declaration

        // ============================================================================
        // MongoDBCollection - Implements DocumentDBCollection
        // ============================================================================

        /**
         * @brief MongoDB collection implementation
         *
         * Concrete DocumentDBCollection for MongoDB. Provides CRUD, index, and
         * aggregation operations. Uses weak_ptr to detect connection closure.
         *
         * ```cpp
         * auto coll = conn->getCollection("users");
         * coll->insertOne("{\"name\": \"Alice\"}");
         * auto doc = coll->findOne("{\"name\": \"Alice\"}");
         * coll->updateOne("{\"name\": \"Alice\"}", "{\"$set\": {\"age\": 30}}");
         * coll->deleteOne("{\"name\": \"Alice\"}");
         * ```
         *
         * @see MongoDBConnection, MongoDBCursor, MongoDBDocument
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
            /**
             * @brief Shared mutex from the parent connection
             *
             * This mutex is shared with MongoDBConnection and MongoDBCursor
             * to synchronize all operations that access the same mongoc_client_t.
             * This prevents race conditions when multiple threads use different
             * objects (connection, collection, cursor) that all route through
             * the same underlying client handle.
             */
            SharedConnMutex m_connMutex;
#endif

            /**
             * @brief Validates that the connection is still valid
             * @return unexpected(DBException) if the connection has been closed
             */
            expected<void, DBException> validateConnection(std::nothrow_t) const noexcept;

            /**
             * @brief Helper to get the client pointer safely
             * @return The client pointer, or unexpected(DBException) if the connection has been closed
             */
            expected<mongoc_client_t *, DBException> getClient(std::nothrow_t) const noexcept;

            /**
             * @brief Helper to parse a JSON filter string to BSON
             * @param filter The JSON filter string
             * @return expected containing the BsonHandle, or DBException if the JSON is invalid
             */
            expected<BsonHandle, DBException> parseFilter(std::nothrow_t, const std::string &filter) const noexcept;

            /**
             * @brief Helper to handle MongoDB errors
             * @param error The bson_error_t from MongoDB
             * @param operation The operation that failed
             * @return unexpected(DBException) with the error details
             */
            expected<void, DBException> throwMongoError(std::nothrow_t, const bson_error_t &error, const std::string &operation) const noexcept;

        public:
            /**
             * @brief Construct a collection wrapper
             * @param client Weak reference to the MongoDB client
             * @param collection The MongoDB collection (ownership is transferred)
             * @param name The collection name
             * @param databaseName The database name
             * @param connection Weak pointer to the connection for cursor registration (optional)
             * @param connMutex Shared mutex from the parent connection for thread safety
             */
#if DB_DRIVER_THREAD_SAFE
            MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                              mongoc_collection_t *collection,
                              const std::string &name,
                              const std::string &databaseName,
                              std::weak_ptr<MongoDBConnection> connection,
                              SharedConnMutex connMutex);
#else
            MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                              mongoc_collection_t *collection,
                              const std::string &name,
                              const std::string &databaseName,
                              std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>());
#endif

            ~MongoDBCollection() override = default;

#if DB_DRIVER_THREAD_SAFE
            static cpp_dbc::expected<std::shared_ptr<MongoDBCollection>, DBException>
            create(std::nothrow_t,
                   std::weak_ptr<mongoc_client_t> client,
                   mongoc_collection_t *collection,
                   const std::string &name,
                   const std::string &databaseName,
                   std::weak_ptr<MongoDBConnection> connection,
                   SharedConnMutex connMutex) noexcept
            {
                try
                {
                    return std::make_shared<MongoDBCollection>(std::move(client), collection, name, databaseName, std::move(connection), std::move(connMutex));
                }
                catch (const DBException &ex)
                {
                    return cpp_dbc::unexpected(ex);
                }
                catch (const std::exception &ex)
                {
                    return cpp_dbc::unexpected(DBException("IEAGMJKJZ33G", ex.what(), system_utils::captureCallStack()));
                }
                catch (...)
                {
                    return cpp_dbc::unexpected(DBException("AAIU13LTY75U", "Unknown error creating MongoDBCollection", system_utils::captureCallStack()));
                }
            }

            static std::shared_ptr<MongoDBCollection>
            create(std::weak_ptr<mongoc_client_t> client,
                   mongoc_collection_t *collection,
                   const std::string &name,
                   const std::string &databaseName,
                   std::weak_ptr<MongoDBConnection> connection,
                   SharedConnMutex connMutex)
            {
                auto r = create(std::nothrow, std::move(client), collection, name, databaseName, std::move(connection), std::move(connMutex));
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }
#else
            static cpp_dbc::expected<std::shared_ptr<MongoDBCollection>, DBException>
            create(std::nothrow_t,
                   std::weak_ptr<mongoc_client_t> client,
                   mongoc_collection_t *collection,
                   const std::string &name,
                   const std::string &databaseName,
                   std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>()) noexcept
            {
                try
                {
                    return std::make_shared<MongoDBCollection>(std::move(client), collection, name, databaseName, std::move(connection));
                }
                catch (const DBException &ex)
                {
                    return cpp_dbc::unexpected(ex);
                }
                catch (const std::exception &ex)
                {
                    return cpp_dbc::unexpected(DBException("IEAGMJKJZ33G", ex.what(), system_utils::captureCallStack()));
                }
                catch (...)
                {
                    return cpp_dbc::unexpected(DBException("AAIU13LTY75U", "Unknown error creating MongoDBCollection", system_utils::captureCallStack()));
                }
            }

            static std::shared_ptr<MongoDBCollection>
            create(std::weak_ptr<mongoc_client_t> client,
                   mongoc_collection_t *collection,
                   const std::string &name,
                   const std::string &databaseName,
                   std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>())
            {
                auto r = create(std::nothrow, std::move(client), collection, name, databaseName, std::move(connection));
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }
#endif

            // Prevent copying
            MongoDBCollection(const MongoDBCollection &) = delete;
            MongoDBCollection &operator=(const MongoDBCollection &) = delete;

            // Allow moving
            MongoDBCollection(MongoDBCollection &&other) noexcept;
            MongoDBCollection &operator=(MongoDBCollection &&other) noexcept;

            // DocumentDBCollection interface - throwing API (wrappers)

            #ifdef __cpp_exceptions
            std::string getName() const override;
            std::string getNamespace() const override;
            uint64_t estimatedDocumentCount() override;
            uint64_t countDocuments(const std::string &filter = "") override;

            DocumentInsertResult insertOne(
                std::shared_ptr<DocumentDBData> document,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            DocumentInsertResult insertOne(
                const std::string &jsonDocument,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            DocumentInsertResult insertMany(
                const std::vector<std::shared_ptr<DocumentDBData>> &documents,
                const DocumentWriteOptions &options = DocumentWriteOptions()) override;

            std::shared_ptr<DocumentDBData> findOne(const std::string &filter = "") override;
            std::shared_ptr<DocumentDBData> findById(const std::string &id) override;
            std::shared_ptr<DocumentDBCursor> find(const std::string &filter = "") override;
            std::shared_ptr<DocumentDBCursor> find(
                const std::string &filter,
                const std::string &projection) override;

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

            DocumentDeleteResult deleteOne(const std::string &filter) override;
            DocumentDeleteResult deleteMany(const std::string &filter) override;
            DocumentDeleteResult deleteById(const std::string &id) override;

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

            #endif // __cpp_exceptions
            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            expected<std::string, DBException> getName(std::nothrow_t) const noexcept override;
            expected<std::string, DBException> getNamespace(std::nothrow_t) const noexcept override;
            expected<uint64_t, DBException> estimatedDocumentCount(std::nothrow_t) noexcept override;
            expected<uint64_t, DBException> countDocuments(
                std::nothrow_t, const std::string &filter = "") noexcept override;

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

            expected<std::shared_ptr<DocumentDBData>, DBException>
            findOne(std::nothrow_t, const std::string &filter = "") noexcept override;

            expected<std::shared_ptr<DocumentDBData>, DBException>
            findById(std::nothrow_t, const std::string &id) noexcept override;

            expected<std::shared_ptr<DocumentDBCursor>, DBException>
            find(std::nothrow_t, const std::string &filter = "") noexcept override;

            expected<std::shared_ptr<DocumentDBCursor>, DBException>
            find(std::nothrow_t, const std::string &filter, const std::string &projection) noexcept override;

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

            expected<DocumentDeleteResult, DBException> deleteOne(
                std::nothrow_t,
                const std::string &filter) noexcept override;

            expected<DocumentDeleteResult, DBException> deleteMany(
                std::nothrow_t,
                const std::string &filter) noexcept override;

            expected<DocumentDeleteResult, DBException> deleteById(
                std::nothrow_t,
                const std::string &id) noexcept override;

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
        };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
