#pragma once

#include "handles.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
        class MongoDBConnection; // Forward declaration
        class MongoDBDocument;   // Forward declaration

        // ============================================================================
        // MongoDBCursor - Implements DocumentDBCursor
        // ============================================================================

        /**
         * @brief MongoDB cursor implementation for iterating query results
         *
         * Wraps a mongoc_cursor_t and provides safe iteration over query results.
         * Supports chaining skip/limit/sort modifiers.
         *
         * ```cpp
         * auto cursor = coll->find("{\"active\": true}");
         * cursor->sort("name", true).skip(10).limit(5);
         * while (cursor->next()) {
         *     auto doc = cursor->current();
         *     std::cout << doc->getString("name") << std::endl;
         * }
         * ```
         *
         * @see MongoDBCollection, MongoDBDocument
         */
        class MongoDBCursor final : public DocumentDBCursor, public std::enable_shared_from_this<MongoDBCursor>
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
            /**
             * @brief Shared mutex from the parent connection
             *
             * This mutex is shared with MongoDBConnection and other objects
             * (MongoDBCollection) that access the same mongoc_client_t.
             * All operations that access the client are synchronized through
             * this shared mutex to prevent race conditions.
             */
            SharedConnMutex m_connMutex;
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
             * @param connMutex Shared mutex from the parent connection for thread safety
             */
#if DB_DRIVER_THREAD_SAFE
            MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor,
                          std::weak_ptr<MongoDBConnection> connection, SharedConnMutex connMutex);
#else
            MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor,
                          std::weak_ptr<MongoDBConnection> connection = std::weak_ptr<MongoDBConnection>());
#endif

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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
