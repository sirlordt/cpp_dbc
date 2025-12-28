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
 * @file document_db_collection.hpp
 * @brief Abstract class for document database collections
 */

#ifndef CPP_DBC_DOCUMENT_DB_COLLECTION_HPP
#define CPP_DBC_DOCUMENT_DB_COLLECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cpp_dbc
{

    // Forward declarations
    class DocumentDBData;
    class DocumentDBCursor;

    /**
     * @brief Options for write operations
     */
    struct DocumentWriteOptions
    {
        bool ordered = true;           ///< If true, stop on first error; if false, continue with remaining operations
        bool bypassValidation = false; ///< If true, bypass document validation
    };

    /**
     * @brief Options for update operations
     */
    struct DocumentUpdateOptions
    {
        bool upsert = false; ///< If true, insert a new document if no match is found
        bool multi = false;  ///< If true, update all matching documents; if false, update only the first match
    };

    /**
     * @brief Result of an insert operation
     */
    struct DocumentInsertResult
    {
        bool acknowledged = true;             ///< Whether the write was acknowledged
        std::string insertedId;               ///< The ID of the inserted document
        std::vector<std::string> insertedIds; ///< IDs of inserted documents (for bulk insert)
        uint64_t insertedCount = 0;           ///< Number of documents inserted
    };

    /**
     * @brief Result of an update operation
     */
    struct DocumentUpdateResult
    {
        bool acknowledged = true;   ///< Whether the write was acknowledged
        uint64_t matchedCount = 0;  ///< Number of documents matched
        uint64_t modifiedCount = 0; ///< Number of documents modified
        std::string upsertedId;     ///< ID of upserted document (if upsert occurred)
    };

    /**
     * @brief Result of a delete operation
     */
    struct DocumentDeleteResult
    {
        bool acknowledged = true;  ///< Whether the write was acknowledged
        uint64_t deletedCount = 0; ///< Number of documents deleted
    };

    /**
     * @brief Abstract class for document database collections
     *
     * A collection is the document database equivalent of a table in relational
     * databases. It contains documents and provides CRUD operations.
     *
     * This class provides the interface for:
     * - Inserting documents (single and bulk)
     * - Finding/querying documents
     * - Updating documents
     * - Deleting documents
     * - Index management
     *
     * Implementations: MongoDBCollection, CouchDBCollection, etc.
     */
    class DocumentDBCollection
    {
    public:
        virtual ~DocumentDBCollection() = default;

        // Collection information
        /**
         * @brief Get the name of the collection
         * @return The collection name
         */
        virtual std::string getName() const = 0;

        /**
         * @brief Get the full namespace (database.collection)
         * @return The full namespace
         */
        virtual std::string getNamespace() const = 0;

        /**
         * @brief Get the estimated document count
         * @return The estimated number of documents
         */
        virtual uint64_t estimatedDocumentCount() = 0;

        /**
         * @brief Get the exact document count matching a filter
         * @param filter The filter document (JSON string), empty for all documents
         * @return The exact count of matching documents
         */
        virtual uint64_t countDocuments(const std::string &filter = "") = 0;

        // Insert operations
        /**
         * @brief Insert a single document
         * @param document The document to insert
         * @param options Write options
         * @return The result of the insert operation
         * @throws DBException if the insert fails
         */
        virtual DocumentInsertResult insertOne(
            std::shared_ptr<DocumentDBData> document,
            const DocumentWriteOptions &options = DocumentWriteOptions()) = 0;

        /**
         * @brief Insert a single document from JSON
         * @param jsonDocument The document as a JSON string
         * @param options Write options
         * @return The result of the insert operation
         * @throws DBException if the insert fails
         */
        virtual DocumentInsertResult insertOne(
            const std::string &jsonDocument,
            const DocumentWriteOptions &options = DocumentWriteOptions()) = 0;

        /**
         * @brief Insert multiple documents
         * @param documents The documents to insert
         * @param options Write options
         * @return The result of the insert operation
         * @throws DBException if the insert fails
         */
        virtual DocumentInsertResult insertMany(
            const std::vector<std::shared_ptr<DocumentDBData>> &documents,
            const DocumentWriteOptions &options = DocumentWriteOptions()) = 0;

        // Find operations
        /**
         * @brief Find a single document matching the filter
         * @param filter The filter document (JSON string)
         * @return The matching document, or nullptr if not found
         */
        virtual std::shared_ptr<DocumentDBData> findOne(const std::string &filter = "") = 0;

        /**
         * @brief Find a document by its ID
         * @param id The document ID
         * @return The matching document, or nullptr if not found
         */
        virtual std::shared_ptr<DocumentDBData> findById(const std::string &id) = 0;

        /**
         * @brief Find all documents matching the filter
         * @param filter The filter document (JSON string), empty for all documents
         * @return A cursor for iterating over the results
         */
        virtual std::shared_ptr<DocumentDBCursor> find(const std::string &filter = "") = 0;

        /**
         * @brief Find documents with projection (field selection)
         * @param filter The filter document (JSON string)
         * @param projection The projection document (JSON string) specifying fields to include/exclude
         * @return A cursor for iterating over the results
         */
        virtual std::shared_ptr<DocumentDBCursor> find(
            const std::string &filter,
            const std::string &projection) = 0;

        // Update operations
        /**
         * @brief Update a single document matching the filter
         * @param filter The filter document (JSON string)
         * @param update The update document (JSON string)
         * @param options Update options
         * @return The result of the update operation
         * @throws DBException if the update fails
         */
        virtual DocumentUpdateResult updateOne(
            const std::string &filter,
            const std::string &update,
            const DocumentUpdateOptions &options = DocumentUpdateOptions()) = 0;

        /**
         * @brief Update all documents matching the filter
         * @param filter The filter document (JSON string)
         * @param update The update document (JSON string)
         * @param options Update options
         * @return The result of the update operation
         * @throws DBException if the update fails
         */
        virtual DocumentUpdateResult updateMany(
            const std::string &filter,
            const std::string &update,
            const DocumentUpdateOptions &options = DocumentUpdateOptions()) = 0;

        /**
         * @brief Replace a single document matching the filter
         * @param filter The filter document (JSON string)
         * @param replacement The replacement document
         * @param options Update options
         * @return The result of the replace operation
         * @throws DBException if the replace fails
         */
        virtual DocumentUpdateResult replaceOne(
            const std::string &filter,
            std::shared_ptr<DocumentDBData> replacement,
            const DocumentUpdateOptions &options = DocumentUpdateOptions()) = 0;

        // Delete operations
        /**
         * @brief Delete a single document matching the filter
         * @param filter The filter document (JSON string)
         * @return The result of the delete operation
         * @throws DBException if the delete fails
         */
        virtual DocumentDeleteResult deleteOne(const std::string &filter) = 0;

        /**
         * @brief Delete all documents matching the filter
         * @param filter The filter document (JSON string)
         * @return The result of the delete operation
         * @throws DBException if the delete fails
         */
        virtual DocumentDeleteResult deleteMany(const std::string &filter) = 0;

        /**
         * @brief Delete a document by its ID
         * @param id The document ID
         * @return The result of the delete operation
         * @throws DBException if the delete fails
         */
        virtual DocumentDeleteResult deleteById(const std::string &id) = 0;

        // Index operations
        /**
         * @brief Create an index on the collection
         * @param keys The index keys (JSON string specifying fields and order)
         * @param options Index options (JSON string)
         * @return The name of the created index
         * @throws DBException if index creation fails
         */
        virtual std::string createIndex(
            const std::string &keys,
            const std::string &options = "") = 0;

        /**
         * @brief Drop an index by name
         * @param indexName The name of the index to drop
         * @throws DBException if the index doesn't exist or drop fails
         */
        virtual void dropIndex(const std::string &indexName) = 0;

        /**
         * @brief Drop all indexes on the collection (except _id)
         */
        virtual void dropAllIndexes() = 0;

        /**
         * @brief List all indexes on the collection
         * @return A vector of index specifications as JSON strings
         */
        virtual std::vector<std::string> listIndexes() = 0;

        // Collection operations
        /**
         * @brief Drop (delete) the entire collection
         * @throws DBException if the drop fails
         */
        virtual void drop() = 0;

        /**
         * @brief Rename the collection
         * @param newName The new collection name
         * @param dropTarget If true, drop the target collection if it exists
         * @throws DBException if the rename fails
         */
        virtual void rename(const std::string &newName, bool dropTarget = false) = 0;

        // Aggregation
        /**
         * @brief Execute an aggregation pipeline
         * @param pipeline The aggregation pipeline (JSON array string)
         * @return A cursor for iterating over the results
         * @throws DBException if the aggregation fails
         */
        virtual std::shared_ptr<DocumentDBCursor> aggregate(const std::string &pipeline) = 0;

        /**
         * @brief Get distinct values for a field
         * @param fieldPath The field path
         * @param filter Optional filter document (JSON string)
         * @return A vector of distinct values as JSON strings
         */
        virtual std::vector<std::string> distinct(
            const std::string &fieldPath,
            const std::string &filter = "") = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_COLLECTION_HPP