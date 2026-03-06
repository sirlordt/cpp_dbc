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
 * @file document_db_connection.hpp
 * @brief Abstract class for document database connections
 */

#ifndef CPP_DBC_DOCUMENT_DB_CONNECTION_HPP
#define CPP_DBC_DOCUMENT_DB_CONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class DocumentDBCollection;
    class DocumentDBData;
    class DocumentDBCursor;

    /**
     * @brief Abstract class for document database connections (MongoDB, etc.)
     *
     * Provides access to collections, documents, and database-level operations.
     * Data is organized as Database > Collection > Document (JSON/BSON).
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("mongodb://localhost:27017/mydb", "", ""));
     * auto coll = conn->getCollection("users");
     * auto doc = conn->createDocument(R"({"name": "Alice", "age": 30})");
     * coll->insertOne(doc);
     * auto cursor = coll->find(R"({"age": {"$gt": 25}})");
     * while (cursor->next()) {
     *     std::cout << cursor->current()->toJson() << std::endl;
     * }
     * conn->close();
     * ```
     *
     * Implementations: MongoDBConnection
     *
     * @see DocumentDBCollection, DocumentDBData, DocumentDBCursor
     */
    class DocumentDBConnection : public DBConnection
    {
        friend class DocumentDBConnectionPool;
        friend class DocumentPooledDBConnection;
        template <typename, typename, typename> friend class PooledDBConnectionBase;

    public:
        ~DocumentDBConnection() override = default;

        // ====================================================================
        // Database information
        // ====================================================================

        /**
         * @brief Get the name of the current database
         * @return The database name
         */
        #ifdef __cpp_exceptions
        virtual std::string getDatabaseName() const = 0;

        /**
         * @brief List all databases on the server
         * @return A vector of database names
         */
        virtual std::vector<std::string> listDatabases() = 0;

        /**
         * @brief Check if a database exists
         * @param databaseName The name of the database
         * @return true if the database exists
         */
        virtual bool databaseExists(const std::string &databaseName) = 0;

        /**
         * @brief Switch to a different database
         * @param databaseName The name of the database to use
         * @throws DBException if the database doesn't exist (depending on implementation)
         */
        virtual void useDatabase(const std::string &databaseName) = 0;

        /**
         * @brief Drop (delete) a database
         * @param databaseName The name of the database to drop
         * @throws DBException if the drop fails
         */
        virtual void dropDatabase(const std::string &databaseName) = 0;

        // ====================================================================
        // Collection access
        // ====================================================================

        /**
         * @brief Get a collection by name
         *
         * @param collectionName The name of the collection
         * @return A shared pointer to the collection
         * @note May create the collection implicitly on first write (MongoDB behavior)
         *
         * ```cpp
         * auto users = conn->getCollection("users");
         * auto count = users->estimatedDocumentCount();
         * ```
         */
        virtual std::shared_ptr<DocumentDBCollection> getCollection(const std::string &collectionName) = 0;

        /**
         * @brief List all collections in the current database
         * @return A vector of collection names
         */
        virtual std::vector<std::string> listCollections() = 0;

        /**
         * @brief Check if a collection exists
         * @param collectionName The name of the collection
         * @return true if the collection exists
         */
        virtual bool collectionExists(const std::string &collectionName) = 0;

        /**
         * @brief Create a new collection explicitly
         *
         * @param collectionName The name of the collection to create
         * @param options Collection options (JSON string, e.g., capped collection settings)
         * @return A shared pointer to the created collection
         * @throws DBException if the collection already exists or creation fails
         */
        virtual std::shared_ptr<DocumentDBCollection> createCollection(
            const std::string &collectionName,
            const std::string &options = "") = 0;

        /**
         * @brief Drop (delete) a collection
         * @param collectionName The name of the collection to drop
         * @throws DBException if the drop fails
         */
        virtual void dropCollection(const std::string &collectionName) = 0;

        // ====================================================================
        // Document factory methods
        // ====================================================================

        /**
         * @brief Create a new empty document
         * @return A shared pointer to a new document
         */
        virtual std::shared_ptr<DocumentDBData> createDocument() = 0;

        /**
         * @brief Create a document from a JSON string
         *
         * @param json The JSON string to parse
         * @return A shared pointer to the created document
         * @throws DBException if the JSON is invalid
         *
         * ```cpp
         * auto doc = conn->createDocument(R"({"name": "Bob", "email": "bob@test.com"})");
         * ```
         */
        virtual std::shared_ptr<DocumentDBData> createDocument(const std::string &json) = 0;

        // Command execution
        /**
         * @brief Execute a database command
         * @param command The command document (JSON string)
         * @return The command result as a document
         * @throws DBException if the command fails
         */
        virtual std::shared_ptr<DocumentDBData> runCommand(const std::string &command) = 0;

        // Server information
        /**
         * @brief Get server information
         * @return Server information as a document
         */
        virtual std::shared_ptr<DocumentDBData> getServerInfo() = 0;

        /**
         * @brief Get server status
         * @return Server status as a document
         */
        virtual std::shared_ptr<DocumentDBData> getServerStatus() = 0;

        // Session/Transaction support (optional - not all document DBs support this)
        /**
         * @brief Start a session for multi-document transactions
         * @return A session identifier
         * @throws DBException if sessions are not supported
         */
        virtual std::string startSession() = 0;

        /**
         * @brief End a session
         * @param sessionId The session identifier
         */
        virtual void endSession(const std::string &sessionId) = 0;

        /**
         * @brief Start a transaction within a session
         * @param sessionId The session identifier
         * @throws DBException if transactions are not supported
         */
        virtual void startTransaction(const std::string &sessionId) = 0;

        /**
         * @brief Commit a transaction
         * @param sessionId The session identifier
         * @throws DBException if the commit fails
         */
        virtual void commitTransaction(const std::string &sessionId) = 0;

        /**
         * @brief Abort a transaction
         * @param sessionId The session identifier
         */
        virtual void abortTransaction(const std::string &sessionId) = 0;

        /**
         * @brief Check if the database supports multi-document transactions
         * @return true if transactions are supported
         */
        virtual bool supportsTransactions() = 0;

        /**
         * @brief Set the transaction isolation level
         * @param level The desired isolation level
         * @note For document stores, this stores the level in-memory (no database command).
         */
        virtual void setTransactionIsolation(TransactionIsolationLevel level) = 0;

        /**
         * @brief Get the current transaction isolation level
         * @return The current isolation level
         */
        virtual TransactionIsolationLevel getTransactionIsolation() = 0;

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        virtual expected<std::string, DBException> getDatabaseName(std::nothrow_t) const noexcept = 0;

        virtual expected<std::vector<std::string>, DBException> listDatabases(std::nothrow_t) noexcept = 0;

        virtual expected<bool, DBException> databaseExists(
            std::nothrow_t, const std::string &databaseName) noexcept = 0;

        virtual expected<void, DBException> useDatabase(
            std::nothrow_t, const std::string &databaseName) noexcept = 0;

        virtual expected<void, DBException> dropDatabase(
            std::nothrow_t, const std::string &databaseName) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBCollection>, DBException> getCollection(
            std::nothrow_t, const std::string &collectionName) noexcept = 0;

        virtual expected<std::vector<std::string>, DBException> listCollections(std::nothrow_t) noexcept = 0;

        virtual expected<bool, DBException> collectionExists(
            std::nothrow_t, const std::string &collectionName) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBCollection>, DBException> createCollection(
            std::nothrow_t,
            const std::string &collectionName,
            const std::string &options = "") noexcept = 0;

        virtual expected<void, DBException> dropCollection(
            std::nothrow_t, const std::string &collectionName) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(std::nothrow_t) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(
            std::nothrow_t, const std::string &json) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBData>, DBException> runCommand(
            std::nothrow_t, const std::string &command) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBData>, DBException> getServerInfo(std::nothrow_t) noexcept = 0;

        virtual expected<std::shared_ptr<DocumentDBData>, DBException> getServerStatus(std::nothrow_t) noexcept = 0;

        virtual expected<std::string, DBException> startSession(std::nothrow_t) noexcept = 0;

        virtual expected<void, DBException> endSession(
            std::nothrow_t, const std::string &sessionId) noexcept = 0;

        virtual expected<void, DBException> startTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept = 0;

        virtual expected<void, DBException> commitTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept = 0;

        virtual expected<void, DBException> abortTransaction(
            std::nothrow_t, const std::string &sessionId) noexcept = 0;

        virtual expected<bool, DBException> supportsTransactions(std::nothrow_t) noexcept = 0;

        /**
         * @brief Set the transaction isolation level (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param level The desired isolation level
         * @return expected containing void on success, or DBException on failure
         */
        virtual expected<void, DBException>
            setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept = 0;

        /**
         * @brief Get the current transaction isolation level (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing the current isolation level, or DBException on failure
         */
        virtual expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept = 0;

    protected:
        /**
         * @brief Prepare the connection for return to pool
         *
         * Called when a connection is returned to the pool. Implementations should:
         * - Close all active cursors
         * - Abort any active transactions
         * - Clean up any session state
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param isolationLevel The transaction isolation level to restore.
         *        TRANSACTION_NONE (default) means "do not restore isolation level".
         * @return expected containing void on success, or DBException on failure
         */
        expected<void, DBException>
            prepareForPoolReturn(std::nothrow_t,
                TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override = 0;

        /**
         * @brief Prepare the connection for borrowing from pool
         *
         * Called when a connection is borrowed from the pool.
         * Implementations can override this to refresh internal state.
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing void on success, or DBException on failure
         */
        expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_CONNECTION_HPP
