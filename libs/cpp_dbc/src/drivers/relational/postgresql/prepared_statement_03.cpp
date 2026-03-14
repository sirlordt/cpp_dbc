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
        PG_STMT_LOCK_OR_RETURN("SDG0G3ALP2L1", "Statement closed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
        {
            return cpp_dbc::unexpected<DBException>(DBException("SZV5YN1UUMSY", "Invalid parameter index for setBlob", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;
        m_paramBound[idx] = true;

        // Store the blob object to keep it alive
        m_blobObjects[idx] = x;

        if (!x)
        {
            // Set to NULL
            m_paramIsNull[idx] = true;
            m_paramValues[idx] = "";
            m_paramLengths[idx] = 0;
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 17;  // BYTEAOID
            return {};
        }

        // Get the blob data using nothrow overloads
        auto lenResult = x->length(std::nothrow);
        if (!lenResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(lenResult.error());
        }
        auto bytesResult = x->getBytes(std::nothrow, 0, lenResult.value());
        if (!bytesResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(bytesResult.error());
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = std::move(bytesResult.value());

        // Use binary format for BYTEA data
        m_paramIsNull[idx] = false;
        m_paramValues[idx].resize(m_blobValues[idx].size());
        std::memcpy(m_paramValues[idx].data(), m_blobValues[idx].data(), m_blobValues[idx].size());

        m_paramLengths[idx] = m_blobValues[idx].size();
        m_paramFormats[idx] = 1; // Binary format
        m_paramTypes[idx] = 17;  // BYTEAOID

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("K2BMSIXEOBRX", "Statement closed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
        {
            return cpp_dbc::unexpected<DBException>(DBException("D182B9C3A9CC", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;
        m_paramBound[idx] = true;

        // Store the stream object to keep it alive
        m_streamObjects[idx] = x;

        if (!x)
        {
            // Set to NULL
            m_paramIsNull[idx] = true;
            m_paramValues[idx] = "";
            m_paramLengths[idx] = 0;
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 17;  // BYTEAOID
            return {};
        }

        // Read all data from the stream using nothrow overload
        std::vector<uint8_t> data;
        std::array<uint8_t, 4096> buffer{};
        for (;;)
        {
            auto readResult = x->read(std::nothrow, buffer.data(), buffer.size());
            if (!readResult.has_value())
            {
                return cpp_dbc::unexpected<DBException>(readResult.error());
            }
            int bytesRead = readResult.value();
            if (bytesRead <= 0)
            {
                break;
            }
            data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = std::move(data);

        // Use binary format for BYTEA data
        m_paramIsNull[idx] = false;
        m_paramValues[idx].resize(m_blobValues[idx].size());
        std::memcpy(m_paramValues[idx].data(), m_blobValues[idx].data(), m_blobValues[idx].size());

        m_paramLengths[idx] = m_blobValues[idx].size();
        m_paramFormats[idx] = 1; // Binary format
        m_paramTypes[idx] = 17;  // BYTEAOID

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("VAKVCYJ43IZ8", "Statement closed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
        {
            return cpp_dbc::unexpected<DBException>(DBException("K8J9LKO7GB19", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;
        m_paramBound[idx] = true;

        // Store the stream object to keep it alive
        m_streamObjects[idx] = x;

        if (!x)
        {
            // Set to NULL
            m_paramIsNull[idx] = true;
            m_paramValues[idx] = "";
            m_paramLengths[idx] = 0;
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 17;  // BYTEAOID
            return {};
        }

        // Read up to 'length' bytes from the stream using nothrow overload
        std::vector<uint8_t> data;
        data.reserve(length);
        std::array<uint8_t, 4096> buffer{};
        size_t totalBytesRead = 0;
        while (totalBytesRead < length)
        {
            auto readResult = x->read(std::nothrow, buffer.data(), std::min(buffer.size(), length - totalBytesRead));
            if (!readResult.has_value())
            {
                return cpp_dbc::unexpected<DBException>(readResult.error());
            }
            int bytesRead = readResult.value();
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
        m_paramIsNull[idx] = false;
        m_paramValues[idx].resize(m_blobValues[idx].size());
        std::memcpy(m_paramValues[idx].data(), m_blobValues[idx].data(), m_blobValues[idx].size());

        m_paramLengths[idx] = m_blobValues[idx].size();
        m_paramFormats[idx] = 1; // Binary format
        m_paramTypes[idx] = 17;  // BYTEAOID

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("58SW1M7BK5ZZ", "Statement closed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
        {
            return cpp_dbc::unexpected<DBException>(DBException("D6EC2CC8C12C", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;
        m_paramBound[idx] = true;

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = x;

        // Use binary format for BYTEA data
        m_paramIsNull[idx] = false;
        m_paramValues[idx].resize(m_blobValues[idx].size());
        std::memcpy(m_paramValues[idx].data(), m_blobValues[idx].data(), m_blobValues[idx].size());

        m_paramLengths[idx] = m_blobValues[idx].size();
        m_paramFormats[idx] = 1; // Binary format
        m_paramTypes[idx] = 17;  // BYTEAOID

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("K8B6T29V5FMQ", "Statement closed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
        {
            return cpp_dbc::unexpected<DBException>(DBException("VVY0L1LZS14O", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;
        m_paramBound[idx] = true;

        if (!x)
        {
            if (length > 0)
            {
                return cpp_dbc::unexpected<DBException>(DBException("0123JWAENYHP",
                    "setBytes: null pointer with non-zero length is invalid; use setNull() to bind SQL NULL",
                    system_utils::captureCallStack()));
            }

            // nullptr + length==0: empty BYTEA (zero bytes, not SQL NULL).
            // Callers who intend SQL NULL must use setNull().
            m_paramIsNull[idx] = false;
            m_blobValues[idx].clear();
            m_paramValues[idx].clear();
            m_paramLengths[idx] = 0;
            m_paramFormats[idx] = 1; // Binary format
            m_paramTypes[idx] = 17;  // BYTEAOID
            return {};
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx].resize(length);
        std::memcpy(m_blobValues[idx].data(), x, length);

        // Use binary format for BYTEA data
        m_paramIsNull[idx] = false;
        m_paramValues[idx].resize(m_blobValues[idx].size());
        std::memcpy(m_paramValues[idx].data(), m_blobValues[idx].data(), m_blobValues[idx].size());

        m_paramLengths[idx] = m_blobValues[idx].size();
        m_paramFormats[idx] = 1; // Binary format
        m_paramTypes[idx] = 17;  // BYTEAOID

        return {};
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
