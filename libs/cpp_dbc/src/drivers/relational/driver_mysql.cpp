/*

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file driver_mysql.cpp
 @brief MySQL database driver implementation

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"
#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cpp_dbc/common/system_utils.hpp>

namespace cpp_dbc
{
    namespace MySQL
    {

        // MySQLDBResultSet implementation

        void MySQLDBResultSet::validateResultState() const
        {
#if DB_DRIVER_THREAD_SAFE
            // Note: This is called from other methods that already hold the lock
            // so we don't acquire the lock here
#endif
            if (!m_result)
            {
                throw DBException("E53694BC170E", "ResultSet has been closed or is invalid", system_utils::captureCallStack());
            }
        }

        void MySQLDBResultSet::validateCurrentRow() const
        {
#if DB_DRIVER_THREAD_SAFE
            // Note: This is called from other methods that already hold the lock
            // so we don't acquire the lock here
#endif
            validateResultState();
            if (!m_currentRow)
            {
                throw DBException("F200B1E69DA7", "No current row - call next() first", system_utils::captureCallStack());
            }
        }

        MySQLDBResultSet::MySQLDBResultSet(MYSQL_RES *res) : m_result(res), m_currentRow(nullptr), m_rowPosition(0)
        {
            if (m_result)
            {
                m_rowCount = mysql_num_rows(m_result.get());
                m_fieldCount = mysql_num_fields(m_result.get());

                // Store column names and create column name to index mapping
                MYSQL_FIELD *fields = mysql_fetch_fields(m_result.get());
                for (size_t i = 0; i < m_fieldCount; i++)
                {
                    std::string name = fields[i].name;
                    m_columnNames.push_back(name);
                    m_columnMap[name] = i;
                }
            }
            else
            {
                m_rowCount = 0;
                m_fieldCount = 0;
            }
        }

        MySQLDBResultSet::~MySQLDBResultSet()
        {
            close();
        }

        bool MySQLDBResultSet::isEmpty()
        {
#if DB_DRIVER_THREAD_SAFE
            DB_DRIVER_LOCK_GUARD(m_mutex);
#endif
            return m_rowCount == 0;
        }

        bool MySQLDBResultSet::next()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (!m_result || m_rowPosition >= m_rowCount)
            {
                m_currentRow = nullptr; // Ensure currentRow is invalidated
                return false;
            }

            m_currentRow = mysql_fetch_row(m_result.get());
            if (m_currentRow)
            {
                m_rowPosition++;
                return true;
            }

            return false;
        }

        bool MySQLDBResultSet::isBeforeFirst()
        {
            return m_rowPosition == 0;
        }

        bool MySQLDBResultSet::isAfterLast()
        {
            return m_result && m_rowPosition > m_rowCount;
        }

        uint64_t MySQLDBResultSet::getRow()
        {
            return m_rowPosition;
        }

        int MySQLDBResultSet::getInt(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("7O8P9Q0R1S2T", "Invalid column index", system_utils::captureCallStack());
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return std::stoi(m_currentRow[idx]);
        }

        int MySQLDBResultSet::getInt(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("3U4V5W6X7Y8Z", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long MySQLDBResultSet::getLong(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("9A0B1C2D3E4F", "Invalid column index", system_utils::captureCallStack());
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0;
            }

            return std::stol(m_currentRow[idx]);
        }

        long MySQLDBResultSet::getLong(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("5G6H7I8J9K0L", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getLong(it->second + 1);
        }

        double MySQLDBResultSet::getDouble(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("1M2N3O4P5Q6R", "Invalid column index", system_utils::captureCallStack());
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0.0;
            }

            return std::stod(m_currentRow[idx]);
        }

        double MySQLDBResultSet::getDouble(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("71685784D1EB", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getDouble(it->second + 1);
        }

        std::string MySQLDBResultSet::getString(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("089F37F0D90E", "Invalid column index", system_utils::captureCallStack());
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return "";
            }

            return std::string(m_currentRow[idx]);
        }

        std::string MySQLDBResultSet::getString(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("45B8E019C425", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getString(it->second + 1);
        }

        bool MySQLDBResultSet::getBoolean(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            // Note: getString also acquires the lock, but we need to hold it for the entire operation
            // Since we're using recursive mutex behavior through separate lock acquisition,
            // we need to call the internal implementation directly
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("089F37F0D90E", "Invalid column index", system_utils::captureCallStack());
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return false;
            }

            std::string value = std::string(m_currentRow[idx]);
            return (value == "1" || value == "true" || value == "TRUE" || value == "True");
        }

        bool MySQLDBResultSet::getBoolean(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("94A1D34DC156", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBoolean(it->second + 1);
        }

        bool MySQLDBResultSet::isNull(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("9BB5941B830C", "Invalid column index", system_utils::captureCallStack());
            }

            size_t idx = columnIndex - 1;
            return m_currentRow[idx] == nullptr;
        }

        bool MySQLDBResultSet::isNull(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("DA3E45676022", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> MySQLDBResultSet::getColumnNames()
        {
            return m_columnNames;
        }

        size_t MySQLDBResultSet::getColumnCount()
        {
            return m_fieldCount;
        }

        void MySQLDBResultSet::close()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (m_result)
            {
                // Smart pointer will automatically call mysql_free_result via MySQLResDeleter
                m_result.reset();
                m_currentRow = nullptr;
                m_rowPosition = 0;
                m_rowCount = 0;
                m_fieldCount = 0;
            }
        }

        // BLOB support methods for MySQLDBResultSet
        std::shared_ptr<Blob> MySQLDBResultSet::getBlob(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("B7C8D9E0F1G2", "Invalid column index for getBlob", system_utils::captureCallStack());
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                // Return an empty blob with no connection (data is already loaded)
                return std::make_shared<MySQL::MySQLBlob>(std::shared_ptr<MYSQL>());
            }

            // Get the length of the BLOB data
            unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                throw DBException("H3I4J5K6L7M8", "Failed to get BLOB data length", system_utils::captureCallStack());
            }

            // Create a new BLOB object with the data
            // Note: We pass an empty shared_ptr because the data is already loaded
            // and the blob won't need to query the database
            std::vector<uint8_t> data;
            if (lengths[idx] > 0)
            {
                data.resize(lengths[idx]);
                std::memcpy(data.data(), m_currentRow[idx], lengths[idx]);
            }

            return std::make_shared<MySQL::MySQLBlob>(std::shared_ptr<MYSQL>(), data);
        }

        std::shared_ptr<Blob> MySQLDBResultSet::getBlob(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("N9O0P1Q2R3S4", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBlob(it->second + 1); // +1 because getBlob(int) is 1-based
        }

        std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("T5U6V7W8X9Y0", "Invalid column index for getBinaryStream", system_utils::captureCallStack());
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                // Return an empty stream
                return std::make_shared<MySQL::MySQLInputStream>("", 0);
            }

            // Get the length of the BLOB data
            unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                throw DBException("Z1A2B3C4D5E6", "Failed to get BLOB data length", system_utils::captureCallStack());
            }

            // Create a new input stream with the data
            return std::make_shared<MySQL::MySQLInputStream>(m_currentRow[idx], lengths[idx]);
        }

        std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("F7G8H9I0J1K2", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBinaryStream(it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }

        std::vector<uint8_t> MySQLDBResultSet::getBytes(size_t columnIndex)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            validateCurrentRow();
            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                throw DBException("L3M4N5O6P7Q8", "Invalid column index for getBytes", system_utils::captureCallStack());
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return {};
            }

            // Get the length of the BLOB data
            unsigned long *lengths = mysql_fetch_lengths(m_result.get());
            if (!lengths)
            {
                throw DBException("R9S0T1U2V3W4", "Failed to get BLOB data length", system_utils::captureCallStack());
            }

            // Create a vector with the data
            std::vector<uint8_t> data;
            if (lengths[idx] > 0)
            {
                data.resize(lengths[idx]);
                std::memcpy(data.data(), m_currentRow[idx], lengths[idx]);
            }

            return data;
        }

        std::vector<uint8_t> MySQLDBResultSet::getBytes(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("X5Y6Z7A8B9C0", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBytes(it->second + 1); // +1 because getBytes(int) is 1-based
        }

        // MySQLDBPreparedStatement implementation
        MYSQL *MySQLDBPreparedStatement::getMySQLConnection() const
        {
            auto conn = m_mysql.lock();
            if (!conn)
            {
                throw DBException("471F2E35F961", "MySQL connection has been closed", system_utils::captureCallStack());
            }
            return conn.get();
        }

        MySQLDBPreparedStatement::MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql_conn, const std::string &sql_stmt)
            : m_mysql(mysql_conn), m_sql(sql_stmt), m_stmt(nullptr)
        {
            MYSQL *mysqlPtr = getMySQLConnection();

            m_stmt.reset(mysql_stmt_init(mysqlPtr));
            if (!m_stmt)
            {
                throw DBException("3Y4Z5A6B7C8D", "Failed to initialize statement", system_utils::captureCallStack());
            }

            if (mysql_stmt_prepare(m_stmt.get(), m_sql.c_str(), m_sql.length()) != 0)
            {
                std::string error = mysql_stmt_error(m_stmt.get());
                m_stmt.reset(); // Smart pointer will call mysql_stmt_close via deleter
                throw DBException("9E0F1G2H3I4J", "Failed to prepare statement: " + error, system_utils::captureCallStack());
            }

            // Count parameters (question marks) in the SQL
            unsigned long paramCount = mysql_stmt_param_count(m_stmt.get());
            m_binds.resize(paramCount);
            memset(m_binds.data(), 0, sizeof(MYSQL_BIND) * paramCount);

            // Initialize string values vector
            m_stringValues.resize(paramCount);

            // Initialize parameter values vector for query reconstruction
            m_parameterValues.resize(paramCount);

            // Initialize numeric value vectors
            m_intValues.resize(paramCount);
            m_longValues.resize(paramCount);
            m_doubleValues.resize(paramCount);
            m_nullFlags.resize(paramCount);

            // Initialize BLOB-related vectors
            m_blobValues.resize(paramCount);
            m_blobObjects.resize(paramCount);
            m_streamObjects.resize(paramCount);
        }

        MySQLDBPreparedStatement::~MySQLDBPreparedStatement()
        {
            close();
        }

        void MySQLDBPreparedStatement::close()
        {
            if (m_stmt)
            {
                m_stmt.reset(); // Smart pointer will call mysql_stmt_close via deleter
            }
        }

        void MySQLDBPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling mysql_stmt_close
            // since the connection is already being destroyed
            this->close();
        }

        void MySQLDBPreparedStatement::setInt(int parameterIndex, int value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("5K6L7M8N9O0P", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_intValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
            m_binds[idx].buffer = &m_intValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
        }

        void MySQLDBPreparedStatement::setLong(int parameterIndex, long value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("1Q2R3S4T5U6V", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_longValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
            m_binds[idx].buffer = &m_longValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
        }

        // BLOB support methods
        void MySQLDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("D1E2F3G4H5I6", "Invalid parameter index for setBlob", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the blob object to keep it alive
            m_blobObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_nullFlags[idx] = 1;
                m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
                m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]);
                m_binds[idx].buffer = nullptr;
                m_binds[idx].buffer_length = 0;
                m_binds[idx].length = nullptr;
                return;
            }

            // Get the blob data
            std::vector<uint8_t> data = x->getBytes(0, x->length());

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction (with proper escaping)
            m_parameterValues[idx] = "BINARY DATA";
        }

        void MySQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("J7K8L9M0N1O2", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_nullFlags[idx] = 1;
                m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
                m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]);
                m_binds[idx].buffer = nullptr;
                m_binds[idx].buffer_length = 0;
                m_binds[idx].length = nullptr;
                return;
            }

            // Read all data from the stream
            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
        }

        void MySQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("P3Q4R5S6T7U8", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_nullFlags[idx] = 1;
                m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
                m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]);
                m_binds[idx].buffer = nullptr;
                m_binds[idx].buffer_length = 0;
                m_binds[idx].length = nullptr;
                return;
            }

            // Read up to 'length' bytes from the stream
            std::vector<uint8_t> data;
            data.reserve(length);
            uint8_t buffer[4096];
            size_t totalBytesRead = 0;
            int bytesRead;
            while (totalBytesRead < length && (bytesRead = x->read(buffer, std::min(sizeof(buffer), length - totalBytesRead))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
                totalBytesRead += bytesRead;
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
        }

        void MySQLDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("V9W0X1Y2Z3A4", "Invalid parameter index for setBytes", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = x;

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
        }

        void MySQLDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("B5C6D7E8F9G0", "Invalid parameter index for setBytes", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            if (!x)
            {
                // Set to NULL
                m_nullFlags[idx] = 1;
                m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
                m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]);
                m_binds[idx].buffer = nullptr;
                m_binds[idx].buffer_length = 0;
                m_binds[idx].length = nullptr;
                return;
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx].resize(length);
            std::memcpy(m_blobValues[idx].data(), x, length);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = "BINARY DATA";
        }

        void MySQLDBPreparedStatement::setDouble(int parameterIndex, double value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("7W8X9Y0Z1A2B", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_doubleValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
            m_binds[idx].buffer = &m_doubleValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
        }

        void MySQLDBPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("3C4D5E6F7G8H", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
            // WARNING_CHECK: Check more carefully
            m_binds[idx].buffer = const_cast<char *>(m_stringValues[idx].c_str());
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction (with proper escaping)
            std::string escapedValue = value;
            // Simple escape for single quotes
            size_t pos = 0;
            while ((pos = escapedValue.find("'", pos)) != std::string::npos)
            {
                escapedValue.replace(pos, 1, "''");
                pos += 2;
            }
            m_parameterValues[idx] = "'" + escapedValue + "'";
        }

        void MySQLDBPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("9I0J1K2L3M4N", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store as int value
            m_intValues[idx] = value ? 1 : 0;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
            m_binds[idx].buffer = &m_intValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(m_intValues[idx]);
        }

        void MySQLDBPreparedStatement::setNull(int parameterIndex, Types type)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("5O6P7Q8R9S0T", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Setup MySQL type based on our Types enum
            enum_field_types mysqlType;
            switch (type)
            {
            case Types::INTEGER:
                mysqlType = MYSQL_TYPE_LONG;
                break;
            case Types::FLOAT:
                mysqlType = MYSQL_TYPE_FLOAT;
                break;
            case Types::DOUBLE:
                mysqlType = MYSQL_TYPE_DOUBLE;
                break;
            case Types::VARCHAR:
                mysqlType = MYSQL_TYPE_STRING;
                break;
            case Types::DATE:
                mysqlType = MYSQL_TYPE_DATE;
                break;
            case Types::TIMESTAMP:
                mysqlType = MYSQL_TYPE_TIMESTAMP;
                break;
            case Types::BOOLEAN:
                mysqlType = MYSQL_TYPE_TINY;
                break;
            case Types::BLOB:
                mysqlType = MYSQL_TYPE_BLOB;
                break;
            default:
                mysqlType = MYSQL_TYPE_NULL;
            }

            // Store the null flag in our vector (1 for true, 0 for false)
            m_nullFlags[idx] = 1;

            m_binds[idx].buffer_type = mysqlType;
            m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]);
            m_binds[idx].buffer = nullptr;
            m_binds[idx].buffer_length = 0;
            m_binds[idx].length = nullptr;
        }

        void MySQLDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("1U2V3W4X5Y6Z", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the date string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_DATE;
            // WARNING_CHECK: Check more carefully
            m_binds[idx].buffer = const_cast<char *>(m_stringValues[idx].c_str());
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;
        }

        void MySQLDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                throw DBException("7A8B9C0D1E2F", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            // Store the timestamp string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_TIMESTAMP;
            // WARNING_CHECK: Check more carefully
            m_binds[idx].buffer = const_cast<char *>(m_stringValues[idx].c_str());
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;
        }

        std::shared_ptr<RelationalDBResultSet> MySQLDBPreparedStatement::executeQuery()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (!m_stmt)
            {
                throw DBException("3G4H5I6J7K8L", "Statement is applied", system_utils::captureCallStack());
            }

            // Get the MySQL connection safely (throws if connection is closed)
            MYSQL *mysqlPtr = getMySQLConnection();

            // Reconstruct the query with bound parameters to avoid "Commands out of sync" issue
            std::string finalQuery = m_sql;

            // Replace each '?' with the corresponding parameter value
            for (size_t i = 0; i < m_parameterValues.size(); i++)
            {
                size_t pos = finalQuery.find('?');
                if (pos != std::string::npos)
                {
                    finalQuery.replace(pos, 1, m_parameterValues[i]);
                }
            }

            // Execute the reconstructed query using the regular connection interface
            if (mysql_query(mysqlPtr, finalQuery.c_str()) != 0)
            {
                throw DBException("9M0N1O2P3Q4R", std::string("Query failed: ") + mysql_error(mysqlPtr), system_utils::captureCallStack());
            }

            MYSQL_RES *result = mysql_store_result(mysqlPtr);
            if (!result && mysql_field_count(mysqlPtr) > 0)
            {
                throw DBException("H1I2J3K4L5M6", std::string("Failed to get result set: ") + mysql_error(mysqlPtr), system_utils::captureCallStack());
            }

            auto resultSet = std::make_shared<MySQLDBResultSet>(result);

            // Close the statement after execution (single-use)
            // This is safe because mysql_store_result() copies all data to client memory
            // close();

            return resultSet;
        }

        uint64_t MySQLDBPreparedStatement::executeUpdate()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (!m_stmt)
            {
                throw DBException("255F5A0C6008", "Statement is applied", system_utils::captureCallStack());
            }

            // Bind parameters
            if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
            {
                throw DBException("9B7E537EB656", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack());
            }

            // Execute the query
            if (mysql_stmt_execute(m_stmt.get()) != 0)
            {
                throw DBException("547F7466347C", std::string("Failed to execute update: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack());
            }

            // auto result = mysql_stmt_affected_rows(m_stmt.get());

            // this->close();

            // Return the number of affected rows
            return mysql_stmt_affected_rows(m_stmt.get());
        }

        bool MySQLDBPreparedStatement::execute()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
#endif
            if (!m_stmt)
            {
                throw DBException("5S6T7U8V9W0X", "Statement is not initialized", system_utils::captureCallStack());
            }

            // Bind parameters
            if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
            {
                throw DBException("1Y2Z3A4B5C6D", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack());
            }

            // Execute the query
            if (mysql_stmt_execute(m_stmt.get()) != 0)
            {
                throw DBException("7E8F9G0H1I2J", std::string("Failed to execute statement: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack());
            }

            // Return whether there's a result set
            return mysql_stmt_field_count(m_stmt.get()) > 0;
        }

        // MySQLDBConnection implementation
        MySQLDBConnection::MySQLDBConnection(const std::string &host,
                                             int port,
                                             const std::string &database,
                                             const std::string &user,
                                             const std::string &password,
                                             const std::map<std::string, std::string> &options)
            : m_closed(false), m_autoCommit(true), m_transactionActive(false),
              m_isolationLevel(TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ) // MySQL default
        {
            // Create shared_ptr with custom deleter for MYSQL*
            m_mysql = std::shared_ptr<MYSQL>(mysql_init(nullptr), MySQLDeleter());
            if (!m_mysql)
            {
                throw DBException("3K4L5M6N7O8P", "Failed to initialize MySQL connection", system_utils::captureCallStack());
            }

            // Force TCP/IP connection
            unsigned int protocol = MYSQL_PROTOCOL_TCP;
            mysql_options(m_mysql.get(), MYSQL_OPT_PROTOCOL, &protocol);

            // Aplicar opciones de conexión desde el mapa
            for (const auto &option : options)
            {
                if (option.first == "connect_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(m_mysql.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
                }
                else if (option.first == "read_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(m_mysql.get(), MYSQL_OPT_READ_TIMEOUT, &timeout);
                }
                else if (option.first == "write_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(m_mysql.get(), MYSQL_OPT_WRITE_TIMEOUT, &timeout);
                }
                else if (option.first == "charset")
                {
                    mysql_options(m_mysql.get(), MYSQL_SET_CHARSET_NAME, option.second.c_str());
                }
                else if (option.first == "auto_reconnect" && option.second == "true")
                {
                    bool reconnect = true;
                    mysql_options(m_mysql.get(), MYSQL_OPT_RECONNECT, &reconnect);
                }
            }

            // Connect to the database
            if (!mysql_real_connect(m_mysql.get(), host.c_str(), user.c_str(), password.c_str(),
                                    nullptr, port, nullptr, 0))
            {
                std::string error = mysql_error(m_mysql.get());
                // unique_ptr will automatically call mysql_close via the deleter
                m_mysql.reset();
                throw DBException("9Q0R1S2T3U4V", "Failed to connect to MySQL: " + error, system_utils::captureCallStack());
            }

            // Select the database if provided
            if (!database.empty())
            {
                if (mysql_select_db(m_mysql.get(), database.c_str()) != 0)
                {
                    std::string error = mysql_error(m_mysql.get());
                    // unique_ptr will automatically call mysql_close via the deleter
                    m_mysql.reset();
                    throw DBException("5W6X7Y8Z9A0B", "Failed to select database: " + error, system_utils::captureCallStack());
                }
            }

            // Disable auto-commit by default to match JDBC behavior
            setAutoCommit(true);

            // Initialize URL string once
            std::stringstream urlBuilder;
            urlBuilder << "cpp_dbc:mysql://" << host << ":" << port;
            if (!database.empty())
            {
                urlBuilder << "/" << database;
            }
            m_url = urlBuilder.str();
        }

        MySQLDBConnection::~MySQLDBConnection()
        {
            // cpp_dbc::system_utils::safePrint(cpp_dbc::system_utils::currentTimeMillis(), "(1) MySQLConnection::~MySQLConnection()");

            close();
        }

        void MySQLDBConnection::close()
        {
            if (!m_closed && m_mysql)
            {
                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(m_statementsMutex);
                    for (auto &stmt : m_activeStatements)
                    {
                        // if (auto stmt = weakStmt.lock())
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    m_activeStatements.clear();
                }

                // Sleep for 25ms to avoid problems with corrency
                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                // cpp_dbc::system_utils::safePrint(cpp_dbc::system_utils::currentTimeMillis(), "(1) MySQLConnection::close()");
                // unique_ptr will automatically call mysql_close via the deleter
                m_mysql.reset();
                m_closed = true;
            }
        }

        bool MySQLDBConnection::isClosed()
        {
            return m_closed;
        }

        void MySQLDBConnection::returnToPool()
        {
            // Don't physically close the connection, just mark it as available
            // so it can be reused by the pool

            // Reset the connection state if necessary
            try
            {
                // Make sure autocommit is enabled for the next time the connection is used
                if (!m_autoCommit)
                {
                    setAutoCommit(true);
                }

                // We don't set m_closed = true because we want to keep the connection open
                // Just mark it as available for reuse
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
        }

        bool MySQLDBConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<RelationalDBPreparedStatement> MySQLDBConnection::prepareStatement(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("1C2D3E4F5G6H", "Connection is closed", system_utils::captureCallStack());
            }

            // Pass weak_ptr to the PreparedStatement so it can safely detect when connection is closed
            auto stmt = std::make_shared<MySQLDBPreparedStatement>(std::weak_ptr<MYSQL>(m_mysql), sql);

            // Register the statement in our registry
            registerStatement(stmt);

            return stmt;
        }

        std::shared_ptr<RelationalDBResultSet> MySQLDBConnection::executeQuery(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("7I8J9K0L1M2N", "Connection is closed", system_utils::captureCallStack());
            }

            if (mysql_query(m_mysql.get(), sql.c_str()) != 0)
            {
                throw DBException("3O4P5Q6R7S8T", std::string("Query failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            MYSQL_RES *result = mysql_store_result(m_mysql.get());
            if (!result && mysql_field_count(m_mysql.get()) > 0)
            {
                throw DBException("9U0V1W2X3Y4Z", std::string("Failed to get result set: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            return std::make_shared<MySQLDBResultSet>(result);
        }

        uint64_t MySQLDBConnection::executeUpdate(const std::string &sql)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("5A6B7C8D9E0F", "Connection is closed", system_utils::captureCallStack());
            }

            if (mysql_query(m_mysql.get(), sql.c_str()) != 0)
            {
                throw DBException("1G2H3I4J5K6L", std::string("Update failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            return mysql_affected_rows(m_mysql.get());
        }

        bool MySQLDBConnection::beginTransaction()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("7M8N9O0P1Q2R", "Connection is closed", system_utils::captureCallStack());
            }

            // If transaction is already active, just return true
            if (m_transactionActive)
            {
                return true;
            }

            // Start transaction by disabling autocommit
            std::string query = "SET autocommit=0";
            if (mysql_query(m_mysql.get(), query.c_str()) != 0)
            {
                throw DBException("N3O4P5Q6R7S8", std::string("Failed to begin transaction: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            m_autoCommit = false;
            m_transactionActive = true;
            return true;
        }

        bool MySQLDBConnection::transactionActive()
        {
            return m_transactionActive;
        }

        void MySQLDBConnection::setAutoCommit(bool autoCommitFlag)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("7M8N9O0P1Q2R", "Connection is closed", system_utils::captureCallStack());
            }

            /*
            // Cannot enable autocommit if a transaction is active
            if (autoCommitFlag && m_transactionActive)
            {
                throw DBException("Z5A6B7C8D9E0", "Cannot enable autocommit while a transaction is active. Commit or rollback first.", system_utils::captureCallStack());
            }
            */

            // Only change the SQL state if we're actually changing the mode
            if (this->m_autoCommit != autoCommitFlag)
            {
                if (autoCommitFlag)
                {
                    // Habilitar autocommit (1) y desactivar transacciones
                    std::string query = "SET autocommit=1";
                    if (mysql_query(m_mysql.get(), query.c_str()) != 0)
                    {
                        throw DBException("N3O4P5Q6R7S8", std::string("Failed to set autocommit mode: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
                    }

                    this->m_autoCommit = true;
                    this->m_transactionActive = false;
                }
                else
                {
                    // Si estamos desactivando autocommit, usar beginTransaction para iniciar la transacción
                    beginTransaction();
                }
            }
        }

        bool MySQLDBConnection::getAutoCommit()
        {
            return m_autoCommit;
        }

        void MySQLDBConnection::commit()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("3S4T5U6V7W8X", "Connection is closed", system_utils::captureCallStack());
            }

            // No transaction active, nothing to commit
            if (!m_transactionActive)
            {
                return;
            }

            if (mysql_query(m_mysql.get(), "COMMIT") != 0)
            {
                throw DBException("9Y0Z1A2B3C4D", std::string("Commit failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            m_transactionActive = false;

            // If autoCommit is still false, a new transaction starts automatically in MySQL
            if (!m_autoCommit)
            {
                m_transactionActive = true;
            }
        }

        void MySQLDBConnection::rollback()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("5E6F7G8H9I0J", "Connection is closed", system_utils::captureCallStack());
            }

            // No transaction active, nothing to rollback
            if (!m_transactionActive)
            {
                return;
            }

            if (mysql_query(m_mysql.get(), "ROLLBACK") != 0)
            {
                throw DBException("1K2L3M4N5O6P", std::string("Rollback failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            m_transactionActive = false;

            // If autoCommit is still false, a new transaction starts automatically in MySQL
            if (!m_autoCommit)
            {
                m_transactionActive = true;
            }
        }

        void MySQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("47FCEE77D4F3", "Connection is closed", system_utils::captureCallStack());
            }

            std::string query;
            switch (level)
            {
            case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                break;
            case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                break;
            default:
                throw DBException("7Q8R9S0T1U2V", "Unsupported transaction isolation level", system_utils::captureCallStack());
            }

            if (mysql_query(m_mysql.get(), query.c_str()) != 0)
            {
                throw DBException("3W4X5Y6Z7A8B", std::string("Failed to set transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
            }

            // Verify that the isolation level was actually set
            TransactionIsolationLevel actualLevel = getTransactionIsolation();
            if (actualLevel != level)
            {
                // Some MySQL configurations might not allow certain isolation levels
                // In this case, we'll update our internal state to match what MySQL actually set
                this->m_isolationLevel = actualLevel;
            }
            else
            {
                this->m_isolationLevel = level;
            }
        }

        TransactionIsolationLevel MySQLDBConnection::getTransactionIsolation()
        {
#if DB_DRIVER_THREAD_SAFE
            std::lock_guard<std::recursive_mutex> lock(m_connMutex);
#endif
            if (m_closed || !m_mysql)
            {
                throw DBException("9C0D1E2F3G4H", "Connection is closed", system_utils::captureCallStack());
            }

            // If we're being called from setTransactionIsolation, return the cached value
            // to avoid potential infinite recursion
            static bool inGetTransactionIsolation = false;
            if (inGetTransactionIsolation)
            {
                return this->m_isolationLevel;
            }

            inGetTransactionIsolation = true;

            try
            {
                // Query the current isolation level
                if (mysql_query(m_mysql.get(), "SELECT @@transaction_isolation") != 0)
                {
                    // Fall back to older MySQL versions that use tx_isolation
                    if (mysql_query(m_mysql.get(), "SELECT @@tx_isolation") != 0)
                    {
                        inGetTransactionIsolation = false;
                        throw DBException("5I6J7K8L9M0N", std::string("Failed to get transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
                    }
                }

                MYSQL_RES *result = mysql_store_result(m_mysql.get());
                if (!result)
                {
                    inGetTransactionIsolation = false;
                    throw DBException("1O2P3Q4R5S6T", std::string("Failed to get result set: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack());
                }

                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    inGetTransactionIsolation = false;
                    throw DBException("7U8V9W0X1Y2Z", "Failed to fetch transaction isolation level", system_utils::captureCallStack());
                }

                std::string level = row[0];
                mysql_free_result(result);

                // Convert the string value to the enum
                TransactionIsolationLevel isolationResult;
                if (level == "READ-UNCOMMITTED" || level == "READ_UNCOMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
                else if (level == "READ-COMMITTED" || level == "READ_COMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
                else if (level == "REPEATABLE-READ" || level == "REPEATABLE_READ")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
                else if (level == "SERIALIZABLE")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
                else
                    isolationResult = TransactionIsolationLevel::TRANSACTION_NONE;

                // Update our cached value
                this->m_isolationLevel = isolationResult;
                inGetTransactionIsolation = false;
                return isolationResult;
            }
            catch (...)
            {
                inGetTransactionIsolation = false;
                throw;
            }
        }

        void MySQLDBConnection::registerStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.insert(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.insert(stmt);
        }

        void MySQLDBConnection::unregisterStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.erase(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.erase(stmt);
        }

        std::string MySQLDBConnection::getURL() const
        {
            return m_url;
        }

        // MySQLDBDriver implementation
        MySQLDBDriver::MySQLDBDriver()
        {
            // Initialize MySQL library
            mysql_library_init(0, nullptr, nullptr);
        }

        MySQLDBDriver::~MySQLDBDriver()
        {
            // Cleanup MySQL library
            mysql_library_end();
        }

        std::shared_ptr<RelationalDBConnection> MySQLDBDriver::connectRelational(const std::string &url,
                                                                                 const std::string &user,
                                                                                 const std::string &password,
                                                                                 const std::map<std::string, std::string> &options)
        {
            std::string host;
            int port;
            std::string database = ""; // Default to empty database

            // Simple parsing for common URL formats
            if (url.substr(0, 16) == "cpp_dbc:mysql://")
            {
                std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

                // Check if there's a port specified
                size_t colonPos = temp.find(":");
                if (colonPos != std::string::npos)
                {
                    // Host with port
                    host = temp.substr(0, colonPos);

                    // Find if there's a database specified
                    size_t slashPos = temp.find("/", colonPos);
                    if (slashPos != std::string::npos)
                    {
                        // Extract port
                        std::string portStr = temp.substr(colonPos + 1, slashPos - colonPos - 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw DBException("3A4B5C6D7E8F", "Invalid port in URL: " + url, system_utils::captureCallStack());
                        }

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }
                    }
                    else
                    {
                        // No database specified, just port
                        std::string portStr = temp.substr(colonPos + 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw DBException("9G0H1I2J3K4L", "Invalid port in URL: " + url, system_utils::captureCallStack());
                        }
                    }
                }
                else
                {
                    // No port specified
                    size_t slashPos = temp.find("/");
                    if (slashPos != std::string::npos)
                    {
                        // Host with database
                        host = temp.substr(0, slashPos);

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }

                        port = 3306; // Default MySQL port
                    }
                    else
                    {
                        // Just host
                        host = temp;
                        port = 3306; // Default MySQL port
                    }
                }
            }
            else
            {
                throw DBException("5M6N7O8P9Q0R", "Invalid MySQL connection URL: " + url, system_utils::captureCallStack());
            }

            return std::make_shared<MySQLDBConnection>(host, port, database, user, password, options);
        }

        bool MySQLDBDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 16) == "cpp_dbc:mysql://";
        }

        bool MySQLDBDriver::parseURL(const std::string &url,
                                     std::string &host,
                                     int &port,
                                     std::string &database)
        {
            // Parse URL of format: cpp_dbc:mysql://host:port/database
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract host, port, and database
            std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

            // Find host:port separator
            size_t hostEnd = temp.find(":");
            if (hostEnd == std::string::npos)
            {
                // Try to find database separator if no port is specified
                hostEnd = temp.find("/");
                if (hostEnd == std::string::npos)
                {
                    // No port and no database specified
                    host = temp;
                    port = 3306;   // Default MySQL port
                    database = ""; // No database
                    return true;
                }

                host = temp.substr(0, hostEnd);
                port = 3306; // Default MySQL port
            }
            else
            {
                host = temp.substr(0, hostEnd);

                // Find port/database separator
                size_t portEnd = temp.find("/", hostEnd + 1);
                if (portEnd == std::string::npos)
                {
                    // No database specified, just port
                    std::string portStr = temp.substr(hostEnd + 1);
                    try
                    {
                        port = std::stoi(portStr);
                    }
                    catch (...)
                    {
                        return false;
                    }

                    // No database part
                    temp = "";
                }
                else
                {
                    std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                    try
                    {
                        port = std::stoi(portStr);
                    }
                    catch (...)
                    {
                        return false;
                    }

                    temp = temp.substr(portEnd);
                }
            }

            // Extract database name (remove leading '/')
            if (temp.length() > 0)
            {
                database = temp.substr(1);
            }
            else
            {
                // No database specified
                database = "";
            }

            return true;
        }

    } // namespace MySQL
} // namespace cpp_dbc
