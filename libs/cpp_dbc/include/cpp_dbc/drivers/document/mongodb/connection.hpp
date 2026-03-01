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
         * @brief Counter for generating unique session IDs
         */
        std::atomic<uint64_t> m_sessionCounter{0};

        /**
         * @brief Active collections (weak references for cleanup tracking)
         */
        std::set<std::weak_ptr<MongoDBCollection>, std::owner_less<std::weak_ptr<MongoDBCollection>>> m_activeCollections;

        /**
         * @brief Active cursors (weak references for cleanup tracking)
         */
        std::set<std::weak_ptr<MongoDBCursor>, std::owner_less<std::weak_ptr<MongoDBCursor>>> m_activeCursors;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Single connection mutex — serializes all access to this connection's state.
         *
         * Shared with child objects (MongoDBCollection, MongoDBCursor) so that all
         * operations on the same mongoc_client_t are serialized regardless of which
         * object initiates them. Declared as recursive_mutex so that re-entrant calls
         * within the same thread (e.g. registerCollection called while holding the lock)
         * never deadlock.
         */
        SharedConnMutex m_connMutex;
#endif

        /**
         * @brief Validates that the connection is open
         * @return unexpected(DBException) if the connection is closed
         */
        expected<void, DBException> validateConnection(std::nothrow_t) const noexcept;

        /**
         * @brief Generate a unique session ID
         * @return A unique session ID string
         */
        std::string generateSessionId();

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the private nothrow constructor when connection setup fails.
         * Inspected by the delegating public throwing constructor and by create(nothrow_t).
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         */
        DBException m_initError{"31N7TQLDCNQT", "", {}};

        /**
         * @brief Private nothrow constructor — contains all connection logic
         *
         * All URI parsing, client creation, and ping logic lives here.
         * On failure, sets m_initFailed and m_initError instead of throwing.
         * Called by the public throwing constructor (via delegation) and by create(nothrow_t).
         *
         * @note create(nothrow_t) uses `new` (not std::make_shared) to access this private constructor.
         */
        MongoDBConnection(std::nothrow_t,
                          const std::string &uri,
                          const std::string &user,
                          const std::string &password,
                          const std::map<std::string, std::string> &options);

    public:
        ~MongoDBConnection() override;

        // Non-copyable, non-movable: owns mutexes and a live DB connection
        MongoDBConnection(const MongoDBConnection &) = delete;
        MongoDBConnection &operator=(const MongoDBConnection &) = delete;
        MongoDBConnection(MongoDBConnection &&) = delete;
        MongoDBConnection &operator=(MongoDBConnection &&) = delete;

        static cpp_dbc::expected<std::shared_ptr<MongoDBConnection>, DBException>
        create(std::nothrow_t,
               const std::string &uri,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // Use `new` instead of std::make_shared: std::make_shared cannot access private constructors,
            // but a static class member function can. The private nothrow constructor stores init
            // errors in m_initFailed/m_initError rather than throwing, so no try/catch is needed here.
            auto obj = std::shared_ptr<MongoDBConnection>(new MongoDBConnection(std::nothrow, uri, user, password, options));
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(obj->m_initError);
            }
            return obj;
        }

#ifdef __cpp_exceptions
        static std::shared_ptr<MongoDBConnection>
        create(const std::string &uri,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto r = create(std::nothrow, uri, user, password, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        // DocumentDBConnection interface - throwing API (wrappers)

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

        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;

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

#endif // __cpp_exceptions
       // ====================================================================
       // NOTHROW VERSIONS - Exception-free API
       // ====================================================================

        expected<void, DBException> close(std::nothrow_t) noexcept override;
        expected<void, DBException> reset(std::nothrow_t) noexcept override;
        expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;

        expected<std::string, DBException> getDatabaseName(std::nothrow_t) const noexcept override;
        expected<std::vector<std::string>, DBException> listDatabases(std::nothrow_t) noexcept override;
        expected<bool, DBException> databaseExists(
            std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<void, DBException> useDatabase(
            std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<void, DBException> dropDatabase(
            std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<std::shared_ptr<DocumentDBCollection>, DBException> getCollection(
            std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<std::vector<std::string>, DBException> listCollections(std::nothrow_t) noexcept override;
        expected<bool, DBException> collectionExists(
            std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<std::shared_ptr<DocumentDBCollection>, DBException> createCollection(
            std::nothrow_t,
            const std::string &collectionName,
            const std::string &options = "") noexcept override;
        expected<void, DBException> dropCollection(
            std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(
            std::nothrow_t, const std::string &json) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> runCommand(
            std::nothrow_t, const std::string &command) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerInfo(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerStatus(std::nothrow_t) noexcept override;
        expected<bool, DBException> ping(std::nothrow_t) noexcept override;
        expected<std::string, DBException> startSession(std::nothrow_t) noexcept override;
        expected<void, DBException> endSession(
            std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> startTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> commitTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> abortTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<bool, DBException> supportsTransactions(std::nothrow_t) noexcept override;
        expected<void, DBException> prepareForPoolReturn(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
