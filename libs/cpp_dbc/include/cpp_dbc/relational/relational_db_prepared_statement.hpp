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
 * @file relational_db_prepared_statement.hpp
 * @brief Abstract class for relational database prepared statements
 */

#ifndef CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP
#define CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "cpp_dbc/core/db_types.hpp"
#include "relational_db_result_set.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class Blob;
    class InputStream;

    /**
     * @brief Abstract class for relational database prepared statements
     *
     * This class provides the interface for prepared SQL statements with
     * parameter binding and execution capabilities.
     *
     * Implementations: MySQLDBPreparedStatement, PostgreSQLDBPreparedStatement,
     *                  SQLiteDBPreparedStatement, FirebirdDBPreparedStatement
     */
    class RelationalDBPreparedStatement
    {
    public:
        virtual ~RelationalDBPreparedStatement() = default;

        // Parameter binding (1-based index)
        virtual void setInt(int parameterIndex, int value) = 0;
        virtual void setLong(int parameterIndex, long value) = 0;
        virtual void setDouble(int parameterIndex, double value) = 0;
        virtual void setString(int parameterIndex, const std::string &value) = 0;
        virtual void setBoolean(int parameterIndex, bool value) = 0;
        virtual void setNull(int parameterIndex, Types type) = 0;
        virtual void setDate(int parameterIndex, const std::string &value) = 0;
        virtual void setTimestamp(int parameterIndex, const std::string &value) = 0;

        // BLOB support methods
        virtual void setBlob(int parameterIndex, std::shared_ptr<Blob> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) = 0;
        virtual void setBytes(int parameterIndex, const std::vector<uint8_t> &x) = 0;
        virtual void setBytes(int parameterIndex, const uint8_t *x, size_t length) = 0;

        // Execution
        /**
         * @brief Execute a SELECT query
         * @return A result set containing the query results
         */
        virtual std::shared_ptr<RelationalDBResultSet> executeQuery() = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement
         * @return The number of affected rows
         */
        virtual uint64_t executeUpdate() = 0;

        /**
         * @brief Execute any SQL statement
         * @return true if the result is a result set, false if it's an update count
         */
        virtual bool execute() = 0;

        /**
         * @brief Close the prepared statement and release resources
         */
        virtual void close() = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP