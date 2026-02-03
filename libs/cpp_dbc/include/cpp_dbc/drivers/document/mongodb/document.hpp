#pragma once

#include "handles.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
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

            /**
             * @brief Get an array of documents with optional strict type checking
             * @param fieldPath The path to the field
             * @param strict If true, fails if any element is not a document; if false, skips non-document elements
             * @return Vector of documents or error
             */
            expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getDocumentArray(
                std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept;

            /**
             * @brief Get an array of strings with optional strict type checking
             * @param fieldPath The path to the field
             * @param strict If true, fails if any element is not a string; if false, skips non-string elements
             * @return Vector of strings or error
             */
            expected<std::vector<std::string>, DBException> getStringArray(
                std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept;

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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
