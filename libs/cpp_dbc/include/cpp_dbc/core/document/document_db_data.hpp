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
 * @file document_db_data.hpp
 * @brief Abstract class for document database data
 */

#ifndef CPP_DBC_DOCUMENT_DB_DATA_HPP
#define CPP_DBC_DOCUMENT_DB_DATA_HPP

#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "cpp_dbc/core/db_expected.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class representing a document in a document database
     *
     * Provides a generic interface for working with JSON/BSON-like documents.
     * Supports nested field access using dot notation (e.g., "address.city").
     *
     * ```cpp
     * auto doc = conn->createDocument(R"({"name": "Alice", "age": 30})");
     * doc->setString("email", "alice@test.com");
     * doc->setDocument("address", conn->createDocument(R"({"city": "NYC"})"));
     * std::string name = doc->getString("name");      // "Alice"
     * std::string city = doc->getString("address.city"); // "NYC"
     * std::cout << doc->toJsonPretty() << std::endl;
     * ```
     *
     * Implementations: MongoDBDocument
     *
     * @see DocumentDBCollection, DocumentDBConnection
     */
    class DocumentDBData
    {
    public:
        virtual ~DocumentDBData() = default;

        // Document identification
        /**
         * @brief Get the unique identifier of the document
         * @return The document ID as a string (e.g., MongoDB ObjectId as hex string)
         */
#ifdef __cpp_exceptions
        virtual std::string getId() const = 0;

        /**
         * @brief Set the document identifier
         * @param id The document ID
         */
        virtual void setId(const std::string &id) = 0;

        // JSON/BSON representation
        /**
         * @brief Get the document as a JSON string
         * @return The document serialized as JSON
         */
        virtual std::string toJson() const = 0;

        /**
         * @brief Get the document as a pretty-printed JSON string
         * @return The document serialized as formatted JSON
         */
        virtual std::string toJsonPretty() const = 0;

        /**
         * @brief Parse and set document content from a JSON string
         * @param json The JSON string to parse
         * @throws DBException if the JSON is invalid
         */
        virtual void fromJson(const std::string &json) = 0;

        // Field access - basic types
        /**
         * @brief Get a string field value
         * @param fieldPath The field path (e.g., "name" or "address.city")
         * @return The field value as a string
         * @throws DBException if the field doesn't exist or is not a string
         */
        virtual std::string getString(const std::string &fieldPath) const = 0;

        /**
         * @brief Get an integer field value
         * @param fieldPath The field path
         * @return The field value as an integer
         * @throws DBException if the field doesn't exist or is not an integer
         */
        virtual int64_t getInt(const std::string &fieldPath) const = 0;

        /**
         * @brief Get a double field value
         * @param fieldPath The field path
         * @return The field value as a double
         * @throws DBException if the field doesn't exist or is not a number
         */
        virtual double getDouble(const std::string &fieldPath) const = 0;

        /**
         * @brief Get a boolean field value
         * @param fieldPath The field path
         * @return The field value as a boolean
         * @throws DBException if the field doesn't exist or is not a boolean
         */
        virtual bool getBool(const std::string &fieldPath) const = 0;

        /**
         * @brief Get binary data from a field
         * @param fieldPath The field path
         * @return The binary data as a vector of bytes
         * @throws DBException if the field doesn't exist or is not binary
         */
        virtual std::vector<uint8_t> getBinary(const std::string &fieldPath) const = 0;

        // Field access - nested documents and arrays
        /**
         * @brief Get a nested document
         * @param fieldPath The field path
         * @return A shared pointer to the nested document
         * @throws DBException if the field doesn't exist or is not a document
         */
        virtual std::shared_ptr<DocumentDBData> getDocument(const std::string &fieldPath) const = 0;

        /**
         * @brief Get an array of documents
         * @param fieldPath The field path
         * @return A vector of shared pointers to documents
         * @throws DBException if the field doesn't exist or is not an array
         */
        virtual std::vector<std::shared_ptr<DocumentDBData>> getDocumentArray(const std::string &fieldPath) const = 0;

        /**
         * @brief Get an array of strings
         * @param fieldPath The field path
         * @return A vector of strings
         * @throws DBException if the field doesn't exist or is not a string array
         */
        virtual std::vector<std::string> getStringArray(const std::string &fieldPath) const = 0;

        // Field setters
        /**
         * @brief Set a string field value
         * @param fieldPath The field path
         * @param value The value to set
         */
        virtual void setString(const std::string &fieldPath, const std::string &value) = 0;

        /**
         * @brief Set an integer field value
         * @param fieldPath The field path
         * @param value The value to set
         */
        virtual void setInt(const std::string &fieldPath, int64_t value) = 0;

        /**
         * @brief Set a double field value
         * @param fieldPath The field path
         * @param value The value to set
         */
        virtual void setDouble(const std::string &fieldPath, double value) = 0;

        /**
         * @brief Set a boolean field value
         * @param fieldPath The field path
         * @param value The value to set
         */
        virtual void setBool(const std::string &fieldPath, bool value) = 0;

        /**
         * @brief Set binary data in a field
         * @param fieldPath The field path
         * @param value The binary data to set
         */
        virtual void setBinary(const std::string &fieldPath, const std::vector<uint8_t> &value) = 0;

        /**
         * @brief Set a nested document
         * @param fieldPath The field path
         * @param doc The document to set
         */
        virtual void setDocument(const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc) = 0;

        /**
         * @brief Set a null value for a field
         * @param fieldPath The field path
         */
        virtual void setNull(const std::string &fieldPath) = 0;

        // Field existence and type checking
        /**
         * @brief Check if a field exists in the document
         * @param fieldPath The field path
         * @return true if the field exists
         */
        virtual bool hasField(const std::string &fieldPath) const = 0;

        /**
         * @brief Check if a field is null
         * @param fieldPath The field path
         * @return true if the field is null or doesn't exist
         */
        virtual bool isNull(const std::string &fieldPath) const = 0;

        /**
         * @brief Remove a field from the document
         * @param fieldPath The field path
         * @return true if the field was removed, false if it didn't exist
         */
        virtual bool removeField(const std::string &fieldPath) = 0;

        /**
         * @brief Get all field names at the top level
         * @return A vector of field names
         */
        virtual std::vector<std::string> getFieldNames() const = 0;

        // Document operations
        /**
         * @brief Create a deep copy of this document
         * @return A new document with the same content
         */
        virtual std::shared_ptr<DocumentDBData> clone() const = 0;

        /**
         * @brief Clear all fields from the document
         */
        virtual void clear() = 0;

        /**
         * @brief Check if the document is empty (has no fields)
         * @return true if the document has no fields
         */
        virtual bool isEmpty() const = 0;

#endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        // Document identification (nothrow)
        /**
         * @brief Get the unique identifier of the document (nothrow version)
         * @return expected containing document ID string, or DBException on failure
         */
        virtual expected<std::string, DBException> getId(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Set the document identifier (nothrow version)
         * @param id The document ID
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setId(std::nothrow_t, const std::string &id) noexcept = 0;

        // JSON/BSON representation (nothrow)
        /**
         * @brief Get the document as a JSON string (nothrow version)
         * @return expected containing JSON string, or DBException on failure
         */
        virtual expected<std::string, DBException> toJson(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Get the document as a pretty-printed JSON string (nothrow version)
         * @return expected containing formatted JSON string, or DBException on failure
         */
        virtual expected<std::string, DBException> toJsonPretty(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Parse and set document content from a JSON string (nothrow version)
         * @param json The JSON string to parse
         * @return expected<void> or DBException if the JSON is invalid
         */
        virtual expected<void, DBException> fromJson(std::nothrow_t, const std::string &json) noexcept = 0;

        // Field getters (nothrow)
        /**
         * @brief Get a string field value (nothrow version)
         * @return expected containing field value, or DBException on failure
         */
        virtual expected<std::string, DBException> getString(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get an integer field value (nothrow version)
         * @return expected containing field value, or DBException on failure
         */
        virtual expected<int64_t, DBException> getInt(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get a double field value (nothrow version)
         * @return expected containing field value, or DBException on failure
         */
        virtual expected<double, DBException> getDouble(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get a boolean field value (nothrow version)
         * @return expected containing field value, or DBException on failure
         */
        virtual expected<bool, DBException> getBool(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get binary data from a field (nothrow version)
         * @return expected containing binary data, or DBException on failure
         */
        virtual expected<std::vector<uint8_t>, DBException> getBinary(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get a nested document (nothrow version)
         * @return expected containing shared pointer to nested document, or DBException on failure
         */
        virtual expected<std::shared_ptr<DocumentDBData>, DBException> getDocument(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get an array of documents (nothrow version)
         * @return expected containing vector of shared pointers to documents, or DBException on failure
         */
        virtual expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> getDocumentArray(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Get an array of strings (nothrow version)
         * @return expected containing vector of strings, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> getStringArray(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        // Field setters (nothrow)
        /**
         * @brief Set a string field value (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setString(std::nothrow_t, const std::string &fieldPath, const std::string &value) noexcept = 0;

        /**
         * @brief Set an integer field value (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setInt(std::nothrow_t, const std::string &fieldPath, int64_t value) noexcept = 0;

        /**
         * @brief Set a double field value (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setDouble(std::nothrow_t, const std::string &fieldPath, double value) noexcept = 0;

        /**
         * @brief Set a boolean field value (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setBool(std::nothrow_t, const std::string &fieldPath, bool value) noexcept = 0;

        /**
         * @brief Set binary data in a field (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setBinary(std::nothrow_t, const std::string &fieldPath, const std::vector<uint8_t> &value) noexcept = 0;

        /**
         * @brief Set a nested document (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setDocument(std::nothrow_t, const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc) noexcept = 0;

        /**
         * @brief Set a null value for a field (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> setNull(std::nothrow_t, const std::string &fieldPath) noexcept = 0;

        // Field existence and type checking (nothrow)
        /**
         * @brief Check if a field exists in the document (nothrow version)
         * @return expected containing true if the field exists, or DBException on failure
         */
        virtual expected<bool, DBException> hasField(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Check if a field is null (nothrow version)
         * @return expected containing true if the field is null or doesn't exist, or DBException on failure
         */
        virtual expected<bool, DBException> isNull(std::nothrow_t, const std::string &fieldPath) const noexcept = 0;

        /**
         * @brief Remove a field from the document (nothrow version)
         * @return expected containing true if removed, false if it didn't exist, or DBException on failure
         */
        virtual expected<bool, DBException> removeField(std::nothrow_t, const std::string &fieldPath) noexcept = 0;

        /**
         * @brief Get all field names at the top level (nothrow version)
         * @return expected containing vector of field names, or DBException on failure
         */
        virtual expected<std::vector<std::string>, DBException> getFieldNames(std::nothrow_t) const noexcept = 0;

        // Document operations (nothrow)
        /**
         * @brief Create a deep copy of this document (nothrow version)
         * @return expected containing new document with same content, or DBException on failure
         */
        virtual expected<std::shared_ptr<DocumentDBData>, DBException> clone(std::nothrow_t) const noexcept = 0;

        /**
         * @brief Clear all fields from the document (nothrow version)
         * @return expected<void> or DBException on failure
         */
        virtual expected<void, DBException> clear(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if the document is empty (nothrow version)
         * @return expected containing true if the document has no fields, or DBException on failure
         */
        virtual expected<bool, DBException> isEmpty(std::nothrow_t) const noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_DATA_HPP
