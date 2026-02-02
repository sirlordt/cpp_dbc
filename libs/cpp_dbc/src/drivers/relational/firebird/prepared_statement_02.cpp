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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file prepared_statement_02.cpp
 @brief Firebird database driver implementation - FirebirdDBPreparedStatement nothrow methods (part 1)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ============================================================================
    // FirebirdDBPreparedStatement - NOTHROW METHODS (part 2)
    // ============================================================================

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FB6NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                return cpp_dbc::unexpected(DBException("D8E4F0A6B3C8", "Parameter index out of range: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];
            short sqlType = var->sqltype & ~1;

            FIREBIRD_DEBUG("setDouble: parameterIndex=" << parameterIndex << ", value=" << value);
            FIREBIRD_DEBUG("  sqlType=" << sqlType << ", sqlscale=" << var->sqlscale << ", sqllen=" << var->sqllen);

            // Handle DECIMAL/NUMERIC types which are stored as scaled integers
            if (var->sqlscale < 0)
            {
                double scaleFactor = std::pow(10.0, -var->sqlscale);
                FIREBIRD_DEBUG("  DECIMAL type detected, scaleFactor=" << scaleFactor);

                if (sqlType == SQL_SHORT)
                {
                    short scaledValue = static_cast<short>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_SHORT: scaledValue=" << scaledValue);
                    if (m_paramBuffers[idx].size() < sizeof(short))
                    {
                        m_paramBuffers[idx].resize(sizeof(short), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(short));
                    m_paramNullIndicators[idx] = 0;
                }
                else if (sqlType == SQL_LONG)
                {
                    ISC_LONG scaledValue = static_cast<ISC_LONG>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_LONG: scaledValue=" << scaledValue);
                    if (m_paramBuffers[idx].size() < sizeof(ISC_LONG))
                    {
                        m_paramBuffers[idx].resize(sizeof(ISC_LONG), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(ISC_LONG));
                    m_paramNullIndicators[idx] = 0;
                }
                else if (sqlType == SQL_INT64)
                {
                    ISC_INT64 scaledValue = static_cast<ISC_INT64>(std::round(value * scaleFactor));
                    FIREBIRD_DEBUG("  SQL_INT64: scaledValue=" << scaledValue);
                    if (m_paramBuffers[idx].size() < sizeof(ISC_INT64))
                    {
                        m_paramBuffers[idx].resize(sizeof(ISC_INT64), 0);
                        var->sqldata = m_paramBuffers[idx].data();
                    }
                    std::memcpy(var->sqldata, &scaledValue, sizeof(ISC_INT64));
                    m_paramNullIndicators[idx] = 0;
                }
                else
                {
                    FIREBIRD_DEBUG("  Unknown scaled type, falling back to double");
                    setParameter(parameterIndex, &value, sizeof(double), SQL_DOUBLE);
                }
            }
            else if (sqlType == SQL_FLOAT)
            {
                float floatValue = static_cast<float>(value);
                FIREBIRD_DEBUG("  SQL_FLOAT: floatValue=" << floatValue);
                setParameter(parameterIndex, &floatValue, sizeof(float), SQL_FLOAT);
            }
            else
            {
                FIREBIRD_DEBUG("  SQL_DOUBLE: value=" << value);
                setParameter(parameterIndex, &value, sizeof(double), SQL_DOUBLE);
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E5F6A7B8C9D0", std::string("Exception in setDouble: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F6A7B8C9D0E1", "Unknown exception in setDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FB7NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                return cpp_dbc::unexpected(DBException("E9F5A1B7C4D0", "Parameter index out of range: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            XSQLVAR *var = &m_inputSqlda->sqlvar[idx];
            short sqlType = var->sqltype & ~1;

            // Handle BLOB type - convert string to BLOB
            if (sqlType == SQL_BLOB)
            {
                FIREBIRD_DEBUG("setString: parameterIndex=" << parameterIndex << " is BLOB type, converting to BLOB");
                // Convert string to bytes and use setBytes
                std::vector<uint8_t> data(value.begin(), value.end());

                auto conn = m_connection.lock();
                if (!conn)
                {
                    return cpp_dbc::unexpected(DBException("E9F5A1B7C4D1", "Connection has been closed", system_utils::captureCallStack()));
                }

                // Create a FirebirdBlob with the data using the connection-based constructor
                auto blob = std::make_shared<FirebirdBlob>(conn, data);

                // Save the blob to the database and get its ID
                ISC_QUAD blobId = blob->save();

                // Store the blob data to keep it alive
                m_blobValues.push_back(data);

                // Ensure buffer is large enough for ISC_QUAD
                if (m_paramBuffers[idx].size() < sizeof(ISC_QUAD))
                {
                    m_paramBuffers[idx].resize(sizeof(ISC_QUAD), 0);
                    var->sqldata = m_paramBuffers[idx].data();
                }

                // Copy the blob ID to the parameter buffer
                std::memcpy(var->sqldata, &blobId, sizeof(ISC_QUAD));
                var->sqllen = sizeof(ISC_QUAD);
                m_paramNullIndicators[idx] = 0;
                return {};
            }

            // Handle VARCHAR type
            size_t totalLen = sizeof(short) + value.length();
            if (totalLen > m_paramBuffers[idx].size())
            {
                m_paramBuffers[idx].resize(totalLen + 1, 0);
                var->sqldata = m_paramBuffers[idx].data();
            }

            short len = static_cast<short>(value.length());
            std::memcpy(var->sqldata, &len, sizeof(short));
            std::memcpy(var->sqldata + sizeof(short), value.c_str(), value.length());
            var->sqllen = static_cast<short>(totalLen);
            m_paramNullIndicators[idx] = 0;
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E4D915378DB1", std::string("Exception in setString: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("4AC7DE73254B", "Unknown exception in setString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FB8NV4L1D4T3D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            short val = value ? 1 : 0;
            setParameter(parameterIndex, &val, sizeof(short), SQL_SHORT);
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A8B9C0D1E2F3", std::string("Exception in setBoolean: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B9C0D1E2F3A4", "Unknown exception in setBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SN0L01", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
            {
                return cpp_dbc::unexpected(DBException("F0A6B2C8D5E1", "Parameter index out of range: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            size_t idx = static_cast<size_t>(parameterIndex - 1);
            m_paramNullIndicators[idx] = -1;
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C0D1E2F3A4B5", std::string("Exception in setNull: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D1E2F3A4B5C6", "Unknown exception in setNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SDAT02", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            // Parse date string (expected format: YYYY-MM-DD)
            struct tm time = {};
            if (sscanf(value.c_str(), "%d-%d-%d", &time.tm_year, &time.tm_mon, &time.tm_mday) != 3)
            {
                return cpp_dbc::unexpected(DBException("A1B7C3D9E6F2", "Invalid date format: " + value, system_utils::captureCallStack()));
            }
            time.tm_year -= 1900;
            time.tm_mon -= 1;

            ISC_DATE date;
            isc_encode_sql_date(&time, &date);
            setParameter(parameterIndex, &date, sizeof(ISC_DATE), SQL_TYPE_DATE);
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E2F3A4B5C6D7", std::string("Exception in setDate: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F3A4B5C6D7E8", "Unknown exception in setDate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4STMP03", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            // Parse timestamp string (expected format: YYYY-MM-DD HH:MM:SS)
            struct tm time = {};
            if (sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                       &time.tm_year, &time.tm_mon, &time.tm_mday,
                       &time.tm_hour, &time.tm_min, &time.tm_sec) != 6)
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            time.tm_year -= 1900;
            time.tm_mon -= 1;

            ISC_TIMESTAMP ts;
            isc_encode_timestamp(&time, &ts);
            setParameter(parameterIndex, &ts, sizeof(ISC_TIMESTAMP), SQL_TIMESTAMP);
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A4B5C6D7E8F9", std::string("Exception in setTimestamp: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B5C6D7E8F9A0", "Unknown exception in setTimestamp", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBLB04", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            m_blobObjects.push_back(x);
            std::vector<uint8_t> data = x->getBytes(0, x->length());
            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C6D7E8F9A0B1", std::string("Exception in setBlob: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D7E8F9A0B1C2", "Unknown exception in setBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBNS05", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            m_streamObjects.push_back(x);

            // Read all data from stream
            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
            }

            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E8F9A0B1C2D3", std::string("Exception in setBinaryStream: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F9A0B1C2D3E4", "Unknown exception in setBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBNL06", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            m_streamObjects.push_back(x);

            // Read specified length from stream
            std::vector<uint8_t> data(length);
            size_t totalRead = 0;
            while (totalRead < length)
            {
                int bytesRead = x->read(data.data() + totalRead, length - totalRead);
                if (bytesRead <= 0)
                    break;
                totalRead += static_cast<size_t>(bytesRead);
            }
            data.resize(totalRead);

            auto bytesResult = setBytes(std::nothrow, parameterIndex, data);
            if (!bytesResult)
            {
                return cpp_dbc::unexpected(bytesResult.error());
            }
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A0B1C2D3E4F5", std::string("Exception in setBinaryStream(length): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B1C2D3E4F5A6", "Unknown exception in setBinaryStream(length)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        try
        {
            // Check if statement was invalidated by connection due to DDL operation
            if (m_invalidated.load(std::memory_order_acquire))
            {
                return cpp_dbc::unexpected(DBException("FBN1V4SBYT07", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
            }

            return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C2D3E4F5A6B7", std::string("Exception in setBytes(vector): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D3E4F5A6B7C8", "Unknown exception in setBytes(vector)", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
