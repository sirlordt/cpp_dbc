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
#include <charconv>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ============================================================================
    // FirebirdDBPreparedStatement - NOTHROW METHODS (part 1)
    // ============================================================================

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("7OAQLW6S1NV5", "Connection lost in setInt");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("QLZNHJHJ02IA", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        ISC_LONG val = static_cast<ISC_LONG>(value);
        return setParameter(std::nothrow, parameterIndex, &val, sizeof(ISC_LONG), SQL_LONG);
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("4Q9KI4JP2XTB", "Connection lost in setLong");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("4EW6UBYPUTTG", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        ISC_INT64 val = static_cast<ISC_INT64>(value);
        return setParameter(std::nothrow, parameterIndex, &val, sizeof(ISC_INT64), SQL_INT64);
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("ZRWB1E4M2L52", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("TNPH5JLMW6GS", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        if (parameterIndex < 1 || parameterIndex > m_inputSqlda->sqld)
        {
            return cpp_dbc::unexpected(DBException("D8E4F0A6B3C8", "Parameter index out of range: " + std::to_string(parameterIndex),
                                                   system_utils::captureCallStack()));
        }

        size_t idx = static_cast<size_t>(parameterIndex - 1);
        XSQLVAR *var = &m_inputSqlda->sqlvar[idx];
        short sqlType = var->sqltype & ~1;

        FIREBIRD_DEBUG("setDouble: parameterIndex=%d, value=%f", parameterIndex, value);
        FIREBIRD_DEBUG("  sqlType=%d, sqlscale=%d, sqllen=%d", (int)sqlType, (int)var->sqlscale, (int)var->sqllen);

        // Handle DECIMAL/NUMERIC types which are stored as scaled integers
        if (var->sqlscale < 0)
        {
            double scaleFactor = std::pow(10.0, -var->sqlscale);
            FIREBIRD_DEBUG("  DECIMAL type detected, scaleFactor=%f", scaleFactor);

            if (sqlType == SQL_SHORT)
            {
                short scaledValue = static_cast<short>(std::round(value * scaleFactor));
                FIREBIRD_DEBUG("  SQL_SHORT: scaledValue=%d", (int)scaledValue);
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
                FIREBIRD_DEBUG("  SQL_LONG: scaledValue=%ld", (long)scaledValue);
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
                FIREBIRD_DEBUG("  SQL_INT64: scaledValue=%lld", (long long)scaledValue);
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
                auto setResult = setParameter(std::nothrow, parameterIndex, &value, sizeof(double), SQL_DOUBLE);
                if (!setResult.has_value())
                {
                    return cpp_dbc::unexpected(setResult.error());
                }
            }
        }
        else if (sqlType == SQL_FLOAT)
        {
            float floatValue = static_cast<float>(value);
            FIREBIRD_DEBUG("  SQL_FLOAT: floatValue=%f", (double)floatValue);
            auto setResult = setParameter(std::nothrow, parameterIndex, &floatValue, sizeof(float), SQL_FLOAT);
            if (!setResult.has_value())
            {
                return cpp_dbc::unexpected(setResult.error());
            }
        }
        else
        {
            FIREBIRD_DEBUG("  SQL_DOUBLE: value=%f", value);
            auto setResult = setParameter(std::nothrow, parameterIndex, &value, sizeof(double), SQL_DOUBLE);
            if (!setResult.has_value())
            {
                return cpp_dbc::unexpected(setResult.error());
            }
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("FXE1E4KBWNL8", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("2K8QBRG3F50U", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
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
            FIREBIRD_DEBUG("setString: parameterIndex=%d is BLOB type, converting to BLOB", parameterIndex);
            // Convert string to bytes and use setBytes
            std::vector<uint8_t> data(value.begin(), value.end());

            auto conn = m_connection.lock();
            if (!conn)
            {
                return cpp_dbc::unexpected(DBException("E9F5A1B7C4D1", "Connection has been closed", system_utils::captureCallStack()));
            }

            // Create a FirebirdBlob with the data using the connection weak_ptr
            auto blobResult = FirebirdBlob::create(std::nothrow, m_connection, data);
            if (!blobResult.has_value())
            {
                return cpp_dbc::unexpected(blobResult.error());
            }
            auto blob = blobResult.value();

            // Save the blob to the database and get its ID via nothrow overload
            auto saveResult = blob->save(std::nothrow);
            if (!saveResult.has_value())
            {
                return cpp_dbc::unexpected(saveResult.error());
            }
            ISC_QUAD blobId = saveResult.value();

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

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("HJ4C76QCZV7E", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("KIHGPUFTQYQZ", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        short val = value ? 1 : 0;
        return setParameter(std::nothrow, parameterIndex, &val, sizeof(short), SQL_SHORT);
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("OJ9DMC2WW02G", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
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

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("JVSOO8279IPP", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("FBN1V4SDAT02", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        // Parse date string (expected format: YYYY-MM-DD)
        struct tm time = {};
        {
            const char *ptr = value.data();
            const char *end = ptr + value.size();

            int year = 0;
            auto [p1, ec1] = std::from_chars(ptr, end, year);
            if (ec1 != std::errc{} || p1 == end || *p1 != '-')
            {
                return cpp_dbc::unexpected(DBException("A1B7C3D9E6F2", "Invalid date format: " + value, system_utils::captureCallStack()));
            }
            ptr = p1 + 1;

            int month = 0;
            auto [p2, ec2] = std::from_chars(ptr, end, month);
            if (ec2 != std::errc{} || p2 == end || *p2 != '-')
            {
                return cpp_dbc::unexpected(DBException("A1B7C3D9E6F2", "Invalid date format: " + value, system_utils::captureCallStack()));
            }
            ptr = p2 + 1;

            int day = 0;
            auto [p3, ec3] = std::from_chars(ptr, end, day);
            if (ec3 != std::errc{} || p3 != end)
            {
                return cpp_dbc::unexpected(DBException("A1B7C3D9E6F2", "Invalid date format: " + value, system_utils::captureCallStack()));
            }

            time.tm_year = year - 1900;
            time.tm_mon = month - 1;
            time.tm_mday = day;
        }

        ISC_DATE date;
        isc_encode_sql_date(&time, &date);
        return setParameter(std::nothrow, parameterIndex, &date, sizeof(ISC_DATE), SQL_TYPE_DATE);
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("98XYZF12NSDR", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("FBN1V4STMP03", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        // Parse timestamp string (expected format: YYYY-MM-DD HH:MM:SS)
        struct tm time = {};
        {
            const char *ptr = value.data();
            const char *end = ptr + value.size();

            int year = 0;
            auto [p1, ec1] = std::from_chars(ptr, end, year);
            if (ec1 != std::errc{} || p1 == end || *p1 != '-')
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            ptr = p1 + 1;

            int month = 0;
            auto [p2, ec2] = std::from_chars(ptr, end, month);
            if (ec2 != std::errc{} || p2 == end || *p2 != '-')
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            ptr = p2 + 1;

            int day = 0;
            auto [p3, ec3] = std::from_chars(ptr, end, day);
            if (ec3 != std::errc{} || p3 == end || *p3 != ' ')
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            ptr = p3 + 1;

            int hour = 0;
            auto [p4, ec4] = std::from_chars(ptr, end, hour);
            if (ec4 != std::errc{} || p4 == end || *p4 != ':')
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            ptr = p4 + 1;

            int min = 0;
            auto [p5, ec5] = std::from_chars(ptr, end, min);
            if (ec5 != std::errc{} || p5 == end || *p5 != ':')
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }
            ptr = p5 + 1;

            int sec = 0;
            auto [p6, ec6] = std::from_chars(ptr, end, sec);
            if (ec6 != std::errc{} || p6 != end)
            {
                return cpp_dbc::unexpected(DBException("B2C8D4E0F7A3", "Invalid timestamp format: " + value, system_utils::captureCallStack()));
            }

            time.tm_year = year - 1900;
            time.tm_mon = month - 1;
            time.tm_mday = day;
            time.tm_hour = hour;
            time.tm_min = min;
            time.tm_sec = sec;
        }

        ISC_TIMESTAMP ts;
        isc_encode_timestamp(&time, &ts);
        return setParameter(std::nothrow, parameterIndex, &ts, sizeof(ISC_TIMESTAMP), SQL_TIMESTAMP);
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::setTime(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("Z21IV6744VHW", "Connection lost");

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("J9K0L1M2N3O4", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        // Parse time string (expected format: HH:MM:SS)
        struct tm time = {};
        {
            const char *ptr = value.data();
            const char *end = ptr + value.size();

            int hour = 0;
            auto [p1, ec1] = std::from_chars(ptr, end, hour);
            if (ec1 != std::errc{} || p1 == end || *p1 != ':')
            {
                return cpp_dbc::unexpected(DBException("68TNRM1CR27K", "Invalid time format: " + value, system_utils::captureCallStack()));
            }
            ptr = p1 + 1;

            int min = 0;
            auto [p2, ec2] = std::from_chars(ptr, end, min);
            if (ec2 != std::errc{} || p2 == end || *p2 != ':')
            {
                return cpp_dbc::unexpected(DBException("68TNRM1CR27K", "Invalid time format: " + value, system_utils::captureCallStack()));
            }
            ptr = p2 + 1;

            int sec = 0;
            auto [p3, ec3] = std::from_chars(ptr, end, sec);
            if (ec3 != std::errc{} || p3 != end)
            {
                return cpp_dbc::unexpected(DBException("68TNRM1CR27K", "Invalid time format: " + value, system_utils::captureCallStack()));
            }

            time.tm_hour = hour;
            time.tm_min = min;
            time.tm_sec = sec;
        }

        ISC_TIME t;
        isc_encode_sql_time(&time, &t);
        return setParameter(std::nothrow, parameterIndex, &t, sizeof(ISC_TIME), SQL_TYPE_TIME);
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
