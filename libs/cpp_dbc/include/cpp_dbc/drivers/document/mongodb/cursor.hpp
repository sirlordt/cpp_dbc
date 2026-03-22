#pragma once

#include "handles.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
    class MongoDBConnection;     // Forward declaration
    class MongoDBConnectionLock; // Forward declaration
    class MongoDBDocument;       // Forward declaration

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
         * @brief Weak pointer to the connection for RAII lock helper
         *
         * Using weak_ptr prevents circular references and allows safe
         * detection of connection closure. The MongoDBConnectionLock
         * RAII helper acquires the connection mutex through this pointer.
         */
        std::weak_ptr<MongoDBConnection> m_connection;

        /**
         * @brief Closed flag for double-checked locking in RAII helper
         *
         * Checked by MongoDBConnectionLock before and after acquiring the
         * connection mutex to detect concurrent close() calls.
         */
        mutable std::atomic<bool> m_closed{false};

        /**
         * @brief The underlying MongoDB cursor
         */
        MongoCursorHandle m_cursor;

        /**
         * @brief The current document (cached)
         */
        std::shared_ptr<MongoDBDocument> m_currentDoc;

        /**
         * @brief Peek-ahead buffer for hasNext() reliability
         *
         * mongoc_cursor_more() is unreliable and may return true even when
         * no more documents exist. To make hasNext() reliable, we read ahead
         * when hasNext() is called and store the result here.
         */
        std::shared_ptr<MongoDBDocument> m_peekedDoc;

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

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Validates that the cursor is valid
         * @return unexpected(DBException) if the cursor is nullptr
         */
        expected<void, DBException> validateCursor(std::nothrow_t) const noexcept;

    public:
        // Nothrow constructor: contains all initialization logic.
        // Public for std::make_shared access, but effectively private via PrivateCtorTag.
        // Errors are stored in m_initFailed/m_initError for the factory to inspect.
        MongoDBCursor(PrivateCtorTag,
                      std::nothrow_t,
                      mongoc_cursor_t *cursor,
                      std::weak_ptr<MongoDBConnection> connection) noexcept;

        ~MongoDBCursor() override;

        // Non-copyable and non-movable: always managed via shared_ptr from create().
        // Moving is incompatible with enable_shared_from_this — the internal weak_ptr
        // control block tracks the original address, so shared_from_this() would be
        // broken in the move destination.
        MongoDBCursor(const MongoDBCursor &) = delete;
        MongoDBCursor &operator=(const MongoDBCursor &) = delete;
        MongoDBCursor(MongoDBCursor &&) = delete;
        MongoDBCursor &operator=(MongoDBCursor &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<MongoDBCursor>
        create(mongoc_cursor_t *cursor,
               std::weak_ptr<MongoDBConnection> connection)
        {
            auto r = create(std::nothrow, cursor, std::move(connection));
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void close() override;
        bool isEmpty() override;

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

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<MongoDBCursor>, DBException>
        create(std::nothrow_t,
               mongoc_cursor_t *cursor,
               std::weak_ptr<MongoDBConnection> connection) noexcept
        {
            // The nothrow constructor stores init errors in m_initFailed/m_initError
            // rather than throwing, so no try/catch is needed here.
            auto obj = std::make_shared<MongoDBCursor>(
                PrivateCtorTag{}, std::nothrow, cursor, std::move(connection));
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        expected<void, DBException> close(std::nothrow_t) noexcept override;
        expected<bool, DBException> isEmpty(std::nothrow_t) noexcept override;

        expected<bool, DBException> next(std::nothrow_t) noexcept override;
        expected<bool, DBException> hasNext(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> current(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> nextDocument(std::nothrow_t) noexcept override;
        expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> toVector(std::nothrow_t) noexcept override;
        expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getBatch(
            std::nothrow_t, size_t batchSize) noexcept override;
        expected<int64_t, DBException> count(std::nothrow_t) noexcept override;
        expected<uint64_t, DBException> getPosition(std::nothrow_t) noexcept override;
        expected<std::reference_wrapper<DocumentDBCursor>, DBException> skip(
            std::nothrow_t, uint64_t n) noexcept override;
        expected<std::reference_wrapper<DocumentDBCursor>, DBException> limit(
            std::nothrow_t, uint64_t n) noexcept override;
        expected<std::reference_wrapper<DocumentDBCursor>, DBException> sort(
            std::nothrow_t, const std::string &fieldPath, bool ascending = true) noexcept override;
        expected<bool, DBException> isExhausted(std::nothrow_t) noexcept override;
        expected<void, DBException> rewind(std::nothrow_t) noexcept override;

        // MongoDB-specific methods
        bool isConnectionValid(std::nothrow_t) const noexcept;
        expected<std::string, DBException> getError(std::nothrow_t) const noexcept;
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
