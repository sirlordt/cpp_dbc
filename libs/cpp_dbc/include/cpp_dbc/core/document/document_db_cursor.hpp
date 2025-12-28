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
 * @file document_db_cursor.hpp
 * @brief Abstract class for document database cursors (result iteration)
 */

#ifndef CPP_DBC_DOCUMENT_DB_CURSOR_HPP
#define CPP_DBC_DOCUMENT_DB_CURSOR_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include "cpp_dbc/core/db_result_set.hpp"

namespace cpp_dbc
{

    // Forward declaration
    class DocumentDBData;

    /**
     * @brief Abstract class for document database cursors
     *
     * This class extends DBResultSet with methods specific to document databases.
     * A cursor provides iteration over query results, similar to how
     * RelationalDBResultSet provides row-by-row access in relational databases.
     *
     * Cursors in document databases typically support:
     * - Forward iteration through documents
     * - Batch retrieval for efficiency
     * - Cursor-level operations like skip, limit, sort
     *
     * Implementations: MongoDBCursor, CouchDBCursor, etc.
     */
    class DocumentDBCursor : public DBResultSet
    {
    public:
        virtual ~DocumentDBCursor() = default;

        // Navigation
        /**
         * @brief Move to the next document in the cursor
         * @return true if there is a next document, false if at end
         */
        virtual bool next() = 0;

        /**
         * @brief Check if the cursor has more documents
         * @return true if there are more documents to iterate
         */
        virtual bool hasNext() = 0;

        /**
         * @brief Get the current document
         * @return A shared pointer to the current document
         * @throws DBException if cursor is not positioned on a valid document
         */
        virtual std::shared_ptr<DocumentDBData> current() = 0;

        /**
         * @brief Get the next document and advance the cursor
         * @return A shared pointer to the next document
         * @throws DBException if there are no more documents
         */
        virtual std::shared_ptr<DocumentDBData> nextDocument() = 0;

        // Batch operations
        /**
         * @brief Get all remaining documents as a vector
         * @return A vector of all remaining documents
         * @note This consumes the cursor - after calling, hasNext() will return false
         */
        virtual std::vector<std::shared_ptr<DocumentDBData>> toVector() = 0;

        /**
         * @brief Get the next batch of documents
         * @param batchSize The maximum number of documents to retrieve
         * @return A vector of documents (may be less than batchSize if fewer remain)
         */
        virtual std::vector<std::shared_ptr<DocumentDBData>> getBatch(size_t batchSize) = 0;

        // Cursor information
        /**
         * @brief Get the number of documents in the cursor (if known)
         * @return The count of documents, or -1 if unknown
         * @note Some databases may not support this without consuming the cursor
         */
        virtual int64_t count() = 0;

        /**
         * @brief Get the current position in the cursor (0-based)
         * @return The current position
         */
        virtual uint64_t getPosition() = 0;

        // Cursor modifiers (must be called before iteration begins)
        /**
         * @brief Skip a number of documents
         * @param n The number of documents to skip
         * @return Reference to this cursor for method chaining
         * @throws DBException if iteration has already begun
         */
        virtual DocumentDBCursor &skip(uint64_t n) = 0;

        /**
         * @brief Limit the number of documents returned
         * @param n The maximum number of documents to return
         * @return Reference to this cursor for method chaining
         * @throws DBException if iteration has already begun
         */
        virtual DocumentDBCursor &limit(uint64_t n) = 0;

        /**
         * @brief Sort the results by a field
         * @param fieldPath The field to sort by
         * @param ascending true for ascending order, false for descending
         * @return Reference to this cursor for method chaining
         * @throws DBException if iteration has already begun
         */
        virtual DocumentDBCursor &sort(const std::string &fieldPath, bool ascending = true) = 0;

        // Cursor state
        /**
         * @brief Check if the cursor is exhausted (no more documents)
         * @return true if all documents have been consumed
         */
        virtual bool isExhausted() = 0;

        /**
         * @brief Rewind the cursor to the beginning
         * @throws DBException if the cursor does not support rewinding
         * @note Not all cursor implementations support rewinding
         */
        virtual void rewind() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_CURSOR_HPP