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

 @file prepared_statement_03.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBPreparedStatement nothrow methods (part 2 - blob/binary)

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

    // Nothrow BLOB support methods for PostgreSQLDBPreparedStatement
    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("3C2333857671", "Invalid parameter index for setBlob", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the blob object to keep it alive
            m_blobObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_paramValues[idx] = "";
                m_paramLengths[idx] = 0;
                m_paramFormats[idx] = 0; // Text format
                m_paramTypes[idx] = 17;  // BYTEAOID
                return {};
            }

            // Get the blob data
            std::vector<uint8_t> data = x->getBytes(0, x->length());

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            // Use binary format for BYTEA data
            m_paramValues[idx].resize(m_blobValues[idx].size());
            std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

            m_paramLengths[idx] = m_blobValues[idx].size();
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7D9E3B5F8A2C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4B6A9C1E8D3F", "Unknown error in PostgreSQLDBPreparedStatement::setBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("D182B9C3A9CC", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_paramValues[idx] = "";
                m_paramLengths[idx] = 0;
                m_paramFormats[idx] = 0; // Text format
                m_paramTypes[idx] = 17;  // BYTEAOID
                return {};
            }

            // Read all data from the stream
            std::vector<uint8_t> data;
            std::array<uint8_t, 4096> buffer{};
            int bytesRead = 0;
            while ((bytesRead = x->read(buffer.data(), buffer.size())) > 0)
            {
                data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            // Use binary format for BYTEA data
            m_paramValues[idx].resize(m_blobValues[idx].size());
            std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

            m_paramLengths[idx] = m_blobValues[idx].size();
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2A6D9F4B7E3C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8C5E1B7A2D9F", "Unknown error in PostgreSQLDBPreparedStatement::setBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("13B0690421E5", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the stream object to keep it alive
            m_streamObjects[idx] = x;

            if (!x)
            {
                // Set to NULL
                m_paramValues[idx] = "";
                m_paramLengths[idx] = 0;
                m_paramFormats[idx] = 0; // Text format
                m_paramTypes[idx] = 17;  // BYTEAOID
                return {};
            }

            // Read up to 'length' bytes from the stream
            std::vector<uint8_t> data;
            data.reserve(length);
            std::array<uint8_t, 4096> buffer{};
            size_t totalBytesRead = 0;
            int bytesRead = 0;
            while (totalBytesRead < length)
            {
                bytesRead = x->read(buffer.data(), std::min(buffer.size(), length - totalBytesRead));
                if (bytesRead <= 0)
                {
                    break;
                }
                data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
                totalBytesRead += bytesRead;
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            // Use binary format for BYTEA data
            m_paramValues[idx].resize(m_blobValues[idx].size());
            std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

            m_paramLengths[idx] = m_blobValues[idx].size();
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5D7F3A2E9B1C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8C2E6B9A4F1D", "Unknown error in PostgreSQLDBPreparedStatement::setBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("D6EC2CC8C12C", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = x;

            // Use binary format for BYTEA data
            m_paramValues[idx].resize(m_blobValues[idx].size());
            std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

            m_paramLengths[idx] = m_blobValues[idx].size();
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3A7F9D2E5B8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6E1B9D4A7C2F", "Unknown error in PostgreSQLDBPreparedStatement::setBytes", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("D8D28AD75097", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            if (!x)
            {
                // Set to NULL
                m_paramValues[idx] = "";
                m_paramLengths[idx] = 0;
                m_paramFormats[idx] = 0; // Text format
                m_paramTypes[idx] = 17;  // BYTEAOID
                return {};
            }

            // Store the data in our vector to keep it alive
            m_blobValues[idx].resize(length);
            std::memcpy(m_blobValues[idx].data(), x, length);

            // Use binary format for BYTEA data
            m_paramValues[idx].resize(m_blobValues[idx].size());
            std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

            m_paramLengths[idx] = m_blobValues[idx].size();
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7D9F2B5E3A8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4E8B3F9A2D6C", "Unknown error in PostgreSQLDBPreparedStatement::setBytes", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
