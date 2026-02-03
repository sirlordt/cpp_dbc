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

 @file result_set_01.cpp
 @brief MySQL database driver implementation - MySQLDBResultSet (constructor, destructor, close, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
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

    MySQLDBResultSet::MySQLDBResultSet(MYSQL_RES *res) : m_result(res)
    {
        if (m_result)
        {
            m_rowCount = mysql_num_rows(m_result.get());
            m_fieldCount = mysql_num_fields(m_result.get());

            // Store column names and create column name to index mapping
            const MYSQL_FIELD *fields = mysql_fetch_fields(m_result.get());
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
        MySQLDBResultSet::close();
    }

    void MySQLDBResultSet::close()
    {

        DB_DRIVER_LOCK_GUARD(m_mutex);

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

    bool MySQLDBResultSet::isEmpty()
    {

        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_rowCount == 0;
    }

    bool MySQLDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t MySQLDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int MySQLDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    int MySQLDBResultSet::getInt(const std::string &columnName)
    {
        auto result = getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    long MySQLDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    long MySQLDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double MySQLDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    double MySQLDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MySQLDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> MySQLDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    size_t MySQLDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // BLOB support methods for MySQLDBResultSet
    std::shared_ptr<Blob> MySQLDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<Blob> MySQLDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<InputStream> MySQLDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<uint8_t> MySQLDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<uint8_t> MySQLDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
