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
 * @file relational_db_result_set.hpp
 * @brief Abstract class for relational database result sets
 */

#ifndef CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP
#define CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "cpp_dbc/core/db_result_set.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class Blob;
    class InputStream;

    /**
     * @brief Abstract class for relational database result sets
     *
     * This class extends DBResultSet with methods specific to relational databases,
     * including row-based navigation and typed column access.
     *
     * Implementations: MySQLDBResultSet, PostgreSQLDBResultSet, SQLiteDBResultSet, FirebirdDBResultSet
     */
    class RelationalDBResultSet : public DBResultSet
    {
    public:
        virtual ~RelationalDBResultSet() = default;

        // Row navigation
        /**
         * @brief Move to the next row in the result set
         * @return true if there is a next row, false if at end
         */
        virtual bool next() = 0;

        /**
         * @brief Check if cursor is before the first row
         * @return true if before first row
         */
        virtual bool isBeforeFirst() = 0;

        /**
         * @brief Check if cursor is after the last row
         * @return true if after last row
         */
        virtual bool isAfterLast() = 0;

        /**
         * @brief Get the current row number (1-based)
         * @return The current row number
         */
        virtual uint64_t getRow() = 0;

        // Typed column access by index (1-based)
        virtual int getInt(size_t columnIndex) = 0;
        virtual long getLong(size_t columnIndex) = 0;
        virtual double getDouble(size_t columnIndex) = 0;
        virtual std::string getString(size_t columnIndex) = 0;
        virtual bool getBoolean(size_t columnIndex) = 0;
        virtual bool isNull(size_t columnIndex) = 0;

        // Typed column access by name
        virtual int getInt(const std::string &columnName) = 0;
        virtual long getLong(const std::string &columnName) = 0;
        virtual double getDouble(const std::string &columnName) = 0;
        virtual std::string getString(const std::string &columnName) = 0;
        virtual bool getBoolean(const std::string &columnName) = 0;
        virtual bool isNull(const std::string &columnName) = 0;

        // Metadata
        /**
         * @brief Get the names of all columns in the result set
         * @return Vector of column names
         */
        virtual std::vector<std::string> getColumnNames() = 0;

        /**
         * @brief Get the number of columns in the result set
         * @return Number of columns
         */
        virtual size_t getColumnCount() = 0;

        // BLOB support methods
        virtual std::shared_ptr<Blob> getBlob(size_t columnIndex) = 0;
        virtual std::shared_ptr<Blob> getBlob(const std::string &columnName) = 0;

        virtual std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) = 0;
        virtual std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) = 0;

        virtual std::vector<uint8_t> getBytes(size_t columnIndex) = 0;
        virtual std::vector<uint8_t> getBytes(const std::string &columnName) = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP