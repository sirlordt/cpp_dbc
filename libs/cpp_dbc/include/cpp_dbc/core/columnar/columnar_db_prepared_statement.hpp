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
 * @file columnar_db_prepared_statement.hpp
 * @brief Abstract class for columnar database prepared statements
 */

#ifndef CPP_DBC_COLUMNAR_DB_PREPARED_STATEMENT_HPP
#define CPP_DBC_COLUMNAR_DB_PREPARED_STATEMENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "columnar_db_result_set.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class InputStream;

    /**
     * @brief Abstract class for columnar database prepared statements
     *
     * This class provides the interface for prepared SQL/CQL statements with
     * parameter binding and execution capabilities. It specifically adds
     * support for batch operations which are critical for columnar database performance.
     *
     * Implementations: ClickHousePreparedStatement, ScyllaDBPreparedStatement
     */
    class ColumnarDBPreparedStatement
    {
    public:
        /**
 * @brief Virtual default destructor.
 *
 * Ensures derived class destructors are invoked when an instance is deleted
 * through a pointer to ColumnarDBPreparedStatement.
 */
virtual ~ColumnarDBPreparedStatement() = default;

        // Parameter binding (1-based index)
        virtual void setInt(int parameterIndex, int value) = 0;
        virtual void setLong(int parameterIndex, long value) = 0;
        virtual void setDouble(int parameterIndex, double value) = 0;
        virtual void setString(int parameterIndex, const std::string &value) = 0;
        virtual void setBoolean(int parameterIndex, bool value) = 0;
        virtual void setNull(int parameterIndex, Types type) = 0;
        virtual void setDate(int parameterIndex, const std::string &value) = 0;
        virtual void setTimestamp(int parameterIndex, const std::string &value) = 0;
        virtual void setUUID(int parameterIndex, const std::string &value) = 0;

        // Binary/Blob support
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) = 0;
        virtual void setBytes(int parameterIndex, const std::vector<uint8_t> &x) = 0;
        virtual void setBytes(int parameterIndex, const uint8_t *x, size_t length) = 0;

        // Execution
        /**
         * @brief Execute a query that returns results (SELECT)
         * @return A result set containing the query results
         */
        virtual std::shared_ptr<ColumnarDBResultSet> executeQuery() = 0;

        /**
         * @brief Execute a statement that modifies data (INSERT, UPDATE, DELETE, DDL)
         * @return The number of affected rows (if available)
         */
        virtual uint64_t executeUpdate() = 0;

        /**
         * @brief Execute any statement
         * @return true if the result is a result set, false if it's an update count
         */
        virtual bool execute() = 0;

        // Batch Processing
        /**
         * @brief Add the current set of parameters to the batch
         */
        virtual void addBatch() = 0;

        /**
         * @brief Clear the current batch of parameters
         */
        virtual void clearBatch() = 0;

        /**
         * @brief Execute the batch of commands
         * @return A vector of update counts for each command in the batch
         */
        virtual std::vector<uint64_t> executeBatch() = 0;

        /**
         * @brief Close the prepared statement and release resources
         */
        virtual void close() = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        virtual cpp_dbc::expected<void, DBException> setInt(std::nothrow_t, int parameterIndex, int value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setLong(std::nothrow_t, int parameterIndex, long value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setDouble(std::nothrow_t, int parameterIndex, double value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setNull(std::nothrow_t, int parameterIndex, Types type) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setUUID(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept = 0;

        virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException> addBatch(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<void, DBException> clearBatch(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<std::vector<uint64_t>, DBException> executeBatch(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_PREPARED_STATEMENT_HPP