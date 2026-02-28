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

            // DocumentDBData interface - throwing API (wrappers)

            #ifdef __cpp_exceptions
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
             * @return expected containing the document, or DBException if bson is null
             */
            static expected<std::shared_ptr<MongoDBDocument>, DBException> fromBson(std::nothrow_t, bson_t *bson) noexcept;

            /**
             * @brief Create a document from a BSON document (takes ownership) — throwing variant
             * @param bson The BSON document
             * @return shared_ptr to the new document
             * @throws DBException if bson is null
             */
            static std::shared_ptr<MongoDBDocument> fromBson(bson_t *bson)
            {
                auto r = fromBson(std::nothrow, bson);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            /**
             * @brief Create a document from a const BSON document (makes a copy)
             * @param bson The BSON document
             * @return expected containing the document, or DBException if bson is null or copy fails
             */
            static expected<std::shared_ptr<MongoDBDocument>, DBException> fromBsonCopy(std::nothrow_t, const bson_t *bson) noexcept;

            /**
             * @brief Create a document from a const BSON document (makes a copy) — throwing variant
             * @param bson The BSON document
             * @return shared_ptr to the new document
             * @throws DBException if bson is null or copy fails
             */
            static std::shared_ptr<MongoDBDocument> fromBsonCopy(const bson_t *bson)
            {
                auto r = fromBsonCopy(std::nothrow, bson);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            #endif // __cpp_exceptions
            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

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
        };

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
