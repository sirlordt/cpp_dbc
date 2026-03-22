#pragma once

#include "handles.hpp"

#if USE_MONGODB

#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace cpp_dbc::MongoDB
{
    // ============================================================================
    // MongoDBDocument - Implements DocumentDBData
    // ============================================================================

    /**
     * @brief MongoDB document implementation backed by BSON
     *
     * Wraps a bson_t document with a safe interface for accessing and
     * manipulating document data. Supports dot notation for nested fields.
     *
     * ```cpp
     * auto doc = conn->createDocument("{\"name\": \"Alice\", \"age\": 30}");
     * std::string name = doc->getString("name");
     * doc->setInt("age", 31);
     * doc->setString("address.city", "NYC");
     * std::string json = doc->toJson();
     * ```
     *
     * @see MongoDBCollection, DocumentDBData
     */
    class MongoDBDocument final : public DocumentDBData
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
         * @brief Flag indicating constructor initialization failed
         *
         * Set by private nothrow constructors when initialization fails.
         * Inspected by create(nothrow_t) to convert to unexpected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Only allocated on the failure path (~256 bytes saved per successful instance).
         */
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Helper to navigate to a nested field using dot notation (nothrow)
         * @param fieldPath The field path (e.g., "address.city")
         * @param outIter Output iterator positioned at the field
         * @return expected containing true if found, or DBException on error
         */
        expected<bool, DBException> navigateToField(std::nothrow_t, const std::string &fieldPath, bson_iter_t &outIter) const noexcept;

        /**
         * @brief Validates that the BSON document is valid (nothrow)
         * @return expected<void> or DBException if m_bson is nullptr
         */
        expected<void, DBException> validateDocument(std::nothrow_t) const noexcept;

    public:
        // Nothrow constructors: contain all initialization logic.
        // Public for std::make_shared access, but effectively private via PrivateCtorTag.
        // Errors are stored in m_initFailed/m_initError for the factory to inspect.

        /**
         * @brief Nothrow default constructor — creates an empty (null) document.
         *
         * Used exclusively by create(std::nothrow_t) to build a MongoDBDocument.
         * The resulting object has m_bson == nullptr until the factory assigns a handle.
         */
        explicit MongoDBDocument(PrivateCtorTag, std::nothrow_t) noexcept;

        /**
         * @brief Nothrow constructor — takes ownership of an existing BSON pointer.
         *
         * Sets m_initFailed if bson is null, so the factory can detect failure.
         */
        explicit MongoDBDocument(PrivateCtorTag, std::nothrow_t, bson_t *bson) noexcept;

        /**
         * @brief Nothrow constructor — parses a JSON string into a BSON document.
         *
         * Sets m_initFailed/m_initError if parsing fails, so the factory can detect failure.
         */
        explicit MongoDBDocument(PrivateCtorTag, std::nothrow_t, const std::string &json) noexcept;

        ~MongoDBDocument() override = default;

        // Non-copyable and non-movable: always managed via shared_ptr from create().
        // Copy semantics are covered by clone(). Move is not needed since the object
        // lives exclusively on the heap and is never transferred by value.
        MongoDBDocument(const MongoDBDocument &) = delete;
        MongoDBDocument &operator=(const MongoDBDocument &) = delete;
        MongoDBDocument(MongoDBDocument &&) = delete;
        MongoDBDocument &operator=(MongoDBDocument &&) = delete;

        // ====================================================================
        // THROWING API — requires exceptions enabled (-fno-exceptions excludes this block)
        // ====================================================================

#ifdef __cpp_exceptions

        static std::shared_ptr<MongoDBDocument> create(bson_t *bson)
        {
            auto r = create(std::nothrow, bson);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<MongoDBDocument> create(const std::string &json)
        {
            auto r = create(std::nothrow, json);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<MongoDBDocument> create()
        {
            auto r = create(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

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

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Nothrow factory — creates an empty BSON document.
         *
         * @return expected containing the new document, or DBException on allocation failure.
         */
        static expected<std::shared_ptr<MongoDBDocument>, DBException> create(std::nothrow_t) noexcept;

        /**
         * @brief Nothrow factory — takes ownership of an existing BSON pointer.
         *
         * Use this whenever a document must be built from a raw bson_t* without exceptions
         * (e.g. under -fno-exceptions or inside a noexcept method).
         *
         * Precondition: bson must not be null (callers must check before invoking).
         *
         * @param bson Raw BSON pointer whose ownership is transferred to the new document.
         * @return expected containing the new document, or DBException on allocation failure.
         */
        static expected<std::shared_ptr<MongoDBDocument>, DBException> create(std::nothrow_t, bson_t *bson) noexcept;

        /**
         * @brief Nothrow factory — parses a JSON string into a new document.
         *
         * @param json The JSON string to parse.
         * @return expected containing the new document, or DBException if parsing fails.
         */
        static expected<std::shared_ptr<MongoDBDocument>, DBException> create(std::nothrow_t, const std::string &json) noexcept;

        /**
         * @brief Create a deep copy of a const BSON document (nothrow)
         * @param bson The source BSON document to copy from
         * @return expected containing the new document, or DBException if bson is null or copy fails
         */
        static expected<std::shared_ptr<MongoDBDocument>, DBException> copyFrom(std::nothrow_t, const bson_t *bson) noexcept;

        expected<std::string, DBException> getId(std::nothrow_t) const noexcept override;
        expected<void, DBException> setId(std::nothrow_t, const std::string &id) noexcept override;

        expected<std::string, DBException> toJson(std::nothrow_t) const noexcept override;
        expected<std::string, DBException> toJsonPretty(std::nothrow_t) const noexcept override;
        expected<void, DBException> fromJson(std::nothrow_t, const std::string &json) noexcept override;

        expected<std::string, DBException> getString(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<int64_t, DBException> getInt(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<double, DBException> getDouble(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<bool, DBException> getBool(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<std::vector<uint8_t>, DBException> getBinary(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getDocument(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getDocumentArray(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<std::vector<std::string>, DBException> getStringArray(std::nothrow_t, const std::string &fieldPath) const noexcept override;

        /**
         * @brief Get an array of documents with optional strict type checking
         * @param strict If true, fails if any element is not a document; if false, skips non-document elements
         */
        expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getDocumentArray(
            std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept;

        /**
         * @brief Get an array of strings with optional strict type checking
         * @param strict If true, fails if any element is not a string; if false, skips non-string elements
         */
        expected<std::vector<std::string>, DBException> getStringArray(
            std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept;

        expected<void, DBException> setString(std::nothrow_t, const std::string &fieldPath, const std::string &value) noexcept override;
        expected<void, DBException> setInt(std::nothrow_t, const std::string &fieldPath, int64_t value) noexcept override;
        expected<void, DBException> setDouble(std::nothrow_t, const std::string &fieldPath, double value) noexcept override;
        expected<void, DBException> setBool(std::nothrow_t, const std::string &fieldPath, bool value) noexcept override;
        expected<void, DBException> setBinary(std::nothrow_t, const std::string &fieldPath, const std::vector<uint8_t> &value) noexcept override;
        expected<void, DBException> setDocument(std::nothrow_t, const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc) noexcept override;
        expected<void, DBException> setNull(std::nothrow_t, const std::string &fieldPath) noexcept override;

        expected<bool, DBException> hasField(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<bool, DBException> isNull(std::nothrow_t, const std::string &fieldPath) const noexcept override;
        expected<bool, DBException> removeField(std::nothrow_t, const std::string &fieldPath) noexcept override;
        expected<std::vector<std::string>, DBException> getFieldNames(std::nothrow_t) const noexcept override;

        expected<std::shared_ptr<DocumentDBData>, DBException> clone(std::nothrow_t) const noexcept override;
        expected<void, DBException> clear(std::nothrow_t) noexcept override;
        expected<bool, DBException> isEmpty(std::nothrow_t) const noexcept override;

        // MongoDB-specific methods — nothrow variants

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
    };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
