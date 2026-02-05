#pragma once

#include "handles.hpp"

#if USE_MONGODB

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cpp_dbc::MongoDB
{
        class MongoDBCollection; // Forward declaration
        class MongoDBCursor;     // Forward declaration

        // ============================================================================
        // MongoDBConnection - Implements DocumentDBConnection
        // ============================================================================

        /**
         * @brief MongoDB connection implementation
         *
         * Concrete DocumentDBConnection for MongoDB databases.
         * Manages a mongoc_client_t and provides access to databases, collections,
         * and session-based transactions.
         *
         * ```cpp
         * auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
         *     cpp_dbc::DriverManager::getDBConnection(
         *         "cpp_dbc:mongodb://localhost:27017/mydb", "", ""));
         * auto coll = conn->getCollection("users");
         * coll->insertOne("{\"name\": \"Alice\", \"age\": 30}");
         * auto cursor = coll->find("{\"age\": {\"$gte\": 18}}");
         * while (cursor->next()) {
         *     std::cout << cursor->current()->toJson() << std::endl;
         * }
         * conn->close();
         * ```
         *
         * @see MongoDBDriver, MongoDBCollection, MongoDBCursor
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
             * @brief Active cursors (weak references for cleanup tracking)
             */
            std::set<std::weak_ptr<MongoDBCursor>, std::owner_less<std::weak_ptr<MongoDBCursor>>> m_activeCursors;

            /**
             * @brief Mutex for cursor tracking
             */
            std::mutex m_cursorsMutex;

#if DB_DRIVER_THREAD_SAFE
            /**
             * @brief Shared connection mutex for thread-safe operations
             *
             * This mutex is shared with all child objects (MongoDBCollection,
             * MongoDBCursor) to ensure all operations on the same mongoc_client_t
             * are properly serialized, regardless of which object initiates them.
             */
            SharedConnMutex m_connMutex;
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
             * @brief Register a collection for cleanup tracking
             * @param collection Weak pointer to the collection to register
             */
            void registerCollection(std::weak_ptr<MongoDBCollection> collection);

            /**
             * @brief Unregister a collection from cleanup tracking
             * @param collection Weak pointer to the collection to unregister
             */
            void unregisterCollection(std::weak_ptr<MongoDBCollection> collection);

            /**
             * @brief Register a cursor for cleanup tracking
             * @param cursor Weak pointer to the cursor to register
             */
            void registerCursor(std::weak_ptr<MongoDBCursor> cursor);

            /**
             * @brief Unregister a cursor from cleanup tracking
             * @param cursor Weak pointer to the cursor to unregister
             */
            void unregisterCursor(std::weak_ptr<MongoDBCursor> cursor);

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
            bool isClosed() const override;
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
            void prepareForPoolReturn() override;

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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
