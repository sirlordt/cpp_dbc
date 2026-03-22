#pragma once

#include "handles.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
    class MongoDBConnection;     // Forward declaration
    class MongoDBConnectionLock; // Forward declaration
    class MongoDBCursor;         // Forward declaration

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
        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        friend class MongoDBConnectionLock;

        /**
         * @brief Weak pointer to the connection for cursor registration
         *
         * Using weak_ptr prevents circular references and allows safe
         * detection of connection closure. Also used by MongoDBConnectionLock
         * RAII helper to acquire the connection mutex.
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

        /**
         * @brief Tracks whether this collection has been closed
         *
         * Used by MongoDBConnectionLock for double-checked locking.
         */
        mutable std::atomic<bool> m_closed{false};

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

        /**
         * @brief Builds a BSON filter for _id lookup, handling both OID and string IDs
         * @param id The document ID (ObjectId hex string or plain string)
         * @return JSON string representing {"_id": ...} filter, or DBException on failure
         */
        expected<std::string, DBException> buildIdFilter(std::nothrow_t, const std::string &id) const noexcept;

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the private nothrow constructor when the collection pointer is null.
         * Inspected by the delegating public throwing constructor and by create(nothrow_t).
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         * Only allocated on the failure path (~256 bytes saved per successful instance).
         */
        std::unique_ptr<DBException> m_initError{nullptr};

    public:
        // Nothrow constructor: contains all initialization logic.
        // Public for std::make_shared access, but effectively private via PrivateCtorTag.
        // Errors are stored in m_initFailed/m_initError for the factory to inspect.
        MongoDBCollection(PrivateCtorTag,
                          std::nothrow_t,
                          mongoc_collection_t *collection,
                          const std::string &name,
                          const std::string &databaseName,
                          std::weak_ptr<MongoDBConnection> connection);

        ~MongoDBCollection() override = default;

        // Non-copyable, non-movable: always owned by shared_ptr, never moved by value
        MongoDBCollection(const MongoDBCollection &) = delete;
        MongoDBCollection &operator=(const MongoDBCollection &) = delete;
        MongoDBCollection(MongoDBCollection &&) = delete;
        MongoDBCollection &operator=(MongoDBCollection &&) = delete;

        // ====================================================================
        // THROWING API — only available when exceptions are enabled
        // ====================================================================

#ifdef __cpp_exceptions

        static std::shared_ptr<MongoDBCollection>
        create(mongoc_collection_t *collection,
               const std::string &name,
               const std::string &databaseName,
               std::weak_ptr<MongoDBConnection> connection)
        {
            auto r = create(std::nothrow, collection, name, databaseName, std::move(connection));
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

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

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — always available, compiles with -fno-exceptions
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<MongoDBCollection>, DBException>
        create(std::nothrow_t,
               mongoc_collection_t *collection,
               const std::string &name,
               const std::string &databaseName,
               std::weak_ptr<MongoDBConnection> connection) noexcept
        {
            // The nothrow constructor stores init errors in m_initFailed/m_initError
            // rather than throwing, so no try/catch is needed here.
            auto obj = std::make_shared<MongoDBCollection>(
                PrivateCtorTag{}, std::nothrow, collection, name, databaseName, std::move(connection));
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

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

        bool isConnectionValid(std::nothrow_t) const noexcept;
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
