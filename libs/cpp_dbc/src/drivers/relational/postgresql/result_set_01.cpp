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
 @brief PostgreSQL database driver implementation - PostgreSQLDBResultSet (constructor, destructor, close, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // PostgreSQLDBResultSet implementation
    PostgreSQLDBResultSet::PostgreSQLDBResultSet(PGresult *res) : m_result(res)
    {
        if (m_result)
        {
            m_rowCount = PQntuples(m_result.get());
            m_fieldCount = PQnfields(m_result.get());

            // Store column names and create column name to index mapping
            for (int i = 0; i < m_fieldCount; i++)
            {
                std::string name = PQfname(m_result.get(), i);
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

    PostgreSQLDBResultSet::~PostgreSQLDBResultSet()
    {
        PostgreSQLDBResultSet::close();
    }

    void PostgreSQLDBResultSet::close()
    {

        DB_DRIVER_LOCK_GUARD(m_mutex);

        if (m_result)
        {
            // Smart pointer will automatically call PQclear via PGresultDeleter
            m_result.reset();
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }
    }

    bool PostgreSQLDBResultSet::isEmpty()
    {

        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_rowCount == 0;
    }

    bool PostgreSQLDBResultSet::next()
    {
        auto result = this->next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isBeforeFirst()
    {
        auto result = this->isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isAfterLast()
    {
        auto result = this->isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t PostgreSQLDBResultSet::getRow()
    {
        auto result = this->getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int PostgreSQLDBResultSet::getInt(size_t columnIndex)
    {
        auto result = this->getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int PostgreSQLDBResultSet::getInt(const std::string &columnName)
    {
        auto result = this->getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    long PostgreSQLDBResultSet::getLong(size_t columnIndex)
    {
        auto result = this->getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    long PostgreSQLDBResultSet::getLong(const std::string &columnName)
    {
        auto result = this->getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double PostgreSQLDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = this->getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double PostgreSQLDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = this->getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getString(size_t columnIndex)
    {
        auto result = this->getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string PostgreSQLDBResultSet::getString(const std::string &columnName)
    {
        auto result = this->getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = this->getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = this->getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isNull(size_t columnIndex)
    {
        auto result = this->isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBResultSet::isNull(const std::string &columnName)
    {
        auto result = this->isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> PostgreSQLDBResultSet::getColumnNames()
    {
        auto result = this->getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t PostgreSQLDBResultSet::getColumnCount()
    {
        auto result = this->getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    // BLOB support methods
    std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = this->getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = this->getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = this->getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = this->getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = this->getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(const std::string &columnName)
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            throw DBException("599349A7DAA4", "Column not found: " + columnName, system_utils::captureCallStack());
        }

        return getBytes(it->second + 1); // +1 because getBytes(int) is 1-based
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
