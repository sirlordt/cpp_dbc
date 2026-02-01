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

 @file result_set_03.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBResultSet nothrow methods (part 2 - blob/binary)

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

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("9S0T1U2V3W4X", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4E2B8C7F1D9A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3A5D1F8B2E7C", "Unknown error in PostgreSQLDBResultSet::isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> PostgreSQLDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {
            return m_columnNames;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7B3E9C2D5F8A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4D2A8F6E9B3C", "Unknown error in PostgreSQLDBResultSet::getColumnNames", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<size_t, DBException> PostgreSQLDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        try
        {
            return m_fieldCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2F9E4B7A3D8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5A8D3F1C6E9B", "Unknown error in PostgreSQLDBResultSet::getColumnCount", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> PostgreSQLDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5K6L7M8N9O0P", "Invalid column index or row position for getBlob", system_utils::captureCallStack()));
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result.get(), row, idx))
            {
                // Return an empty blob with no connection (data is already loaded)
                return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>(std::make_shared<PostgreSQLBlob>(std::shared_ptr<PGconn>()));
            }

            // Check if the column is a bytea type
            Oid type = PQftype(m_result.get(), idx);
            if (type != 17) // BYTEAOID
            {
                return cpp_dbc::unexpected<DBException>(DBException("EA04B0D9155C", "Column is not a BLOB/bytea type", system_utils::captureCallStack()));
            }

            // Get the binary data using our getBytes method
            auto bytesResult = getBytes(std::nothrow, columnIndex);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected<DBException>(bytesResult.error());
            }
            std::vector<uint8_t> data = bytesResult.value();

            // Create a PostgreSQLBlob with the data
            // Note: We pass an empty shared_ptr because the data is already loaded
            // and the blob won't need to query the database
            return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>{std::make_shared<PostgreSQLBlob>(std::shared_ptr<PGconn>(), data)};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("F93D7A2B6E1C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4B8C2E5A1F9D", "Unknown error in PostgreSQLDBResultSet::getBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> PostgreSQLDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("392BEAA07684", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5A1D9F8E3B7C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2C6E8A4B9D1F", "Unknown error in PostgreSQLDBResultSet::getBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> PostgreSQLDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("FC94875EDF73", "Invalid column index or row position for getBinaryStream", system_utils::captureCallStack()));
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result.get(), row, idx))
            {
                // Return an empty stream
                return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{std::make_shared<PostgreSQLInputStream>("", 0)};
            }

            // Get the binary data using our getBytes method
            auto bytesResult = getBytes(std::nothrow, columnIndex);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected<DBException>(bytesResult.error());
            }
            std::vector<uint8_t> data = bytesResult.value();

            // Create a new input stream with the data
            if (data.empty())
            {
                return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{std::make_shared<PostgreSQLInputStream>("", 0)};
            }
            else
            {
                return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{
                    std::make_shared<PostgreSQLInputStream>(
                        reinterpret_cast<const char *>(data.data()),
                        data.size())};
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3A7C9E5F1D2B", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8D4B2F6E9A1C", "Unknown error in PostgreSQLDBResultSet::getBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> PostgreSQLDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("27EF08AD722D", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7B2E9A4F1D5C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3C8F5E1A9D7B", "Unknown error in PostgreSQLDBResultSet::getBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> PostgreSQLDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("D5E8D5D3A7A4", "Invalid column index or row position for getBytes", system_utils::captureCallStack()));
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result.get(), row, idx))
            {
                return std::vector<uint8_t>{};
            }

            // Get the binary data
            const char *value = PQgetvalue(m_result.get(), row, idx);
            int length = PQgetlength(m_result.get(), row, idx);

            // Check if the column is a bytea type
            Oid type = PQftype(m_result.get(), idx);

            // Create a vector with the data
            std::vector<uint8_t> data;

            if (length > 0)
            {
                // For BYTEA type, we need to handle the hex format
                if (type == 17) // BYTEAOID
                {
                    // Check if the data is in hex format (starts with \x)
                    if (length >= 2 && value[0] == '\\' && value[1] == 'x')
                    {
                        // Skip the \x prefix
                        const char *hexData = value + 2;
                        int hexLength = length - 2;

                        // Each byte is represented by 2 hex characters
                        data.reserve(hexLength / 2);

                        for (int i = 0; i < hexLength; i += 2)
                        {
                            if (i + 1 < hexLength)
                            {
                                // Convert hex pair to byte
                                std::array<char, 3> hexPair = {hexData[i], hexData[i + 1], 0};
                                auto byte = static_cast<uint8_t>(strtol(hexPair.data(), nullptr, 16));
                                data.push_back(byte);
                            }
                        }
                    }
                    else
                    {
                        // Use PQunescapeBytea for other bytea formats
                        size_t unescapedLength;
                        unsigned char *unescapedData = PQunescapeBytea(reinterpret_cast<const unsigned char *>(value), &unescapedLength);

                        if (unescapedData)
                        {
                            data.resize(unescapedLength);
                            std::memcpy(data.data(), unescapedData, unescapedLength);
                            PQfreemem(unescapedData);
                        }
                    }
                }
                else
                {
                    // For non-BYTEA types, just copy the raw data
                    data.resize(length);
                    std::memcpy(data.data(), value, length);
                }
            }

            return data;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7F2E9D3B8C5A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4A1D8B5F2E7C", "Unknown error in PostgreSQLDBResultSet::getBytes", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> PostgreSQLDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("599349A7DAA4", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getBytes(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3D8B7E2F5A9C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6F1C8A3B9D2E", "Unknown error in PostgreSQLDBResultSet::getBytes", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
