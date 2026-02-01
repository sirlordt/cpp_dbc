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
 * @file prepared_statement_02.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBPreparedStatement nothrow methods (part 1)
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <array>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{
    // Nothrow API

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        // Check bounds... Cassandra binds by index 0-based, JDBC is 1-based
        if (cass_statement_bind_int32(m_statement.get(), parameterIndex - 1, value) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind int", system_utils::captureCallStack()));
        }

        // For batching, store value
        m_currentEntry.intParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, long value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        if (cass_statement_bind_int64(m_statement.get(), parameterIndex - 1, value) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind long", system_utils::captureCallStack()));
        }
        m_currentEntry.longParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        if (cass_statement_bind_double(m_statement.get(), parameterIndex - 1, value) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind double", system_utils::captureCallStack()));
        }
        m_currentEntry.doubleParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        // Simple string binding without type detection
        // For special types like UUID, timestamp, etc., use the dedicated methods
        CassError rc;

        if (value.empty())
        {
            // For empty strings, bind as empty string rather than null
            rc = cass_statement_bind_string(m_statement.get(), parameterIndex - 1, "");
        }
        else
        {
            // For non-empty strings, bind normally
            rc = cass_statement_bind_string(m_statement.get(), parameterIndex - 1, value.c_str());
        }

        if (rc != CASS_OK)
        {
            std::string errorMsg = "Failed to bind string value '" + value + "'";
            return cpp_dbc::unexpected(DBException("735497230592", errorMsg, system_utils::captureCallStack()));
        }

        m_currentEntry.stringParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        if (cass_statement_bind_bool(m_statement.get(), parameterIndex - 1, value ? cass_true : cass_false) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind bool", system_utils::captureCallStack()));
        }
        m_currentEntry.boolParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types type) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        if (cass_statement_bind_null(m_statement.get(), parameterIndex - 1) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind null", system_utils::captureCallStack()));
        }
        m_currentEntry.nullParams.emplace_back(parameterIndex - 1, type);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        bool parseSuccess = false;
        cass_uint32_t cass_date = 0;

        try
        {
            std::tm tm = {};
            std::istringstream ss(value);

            // For date, we expect just YYYY-MM-DD
            ss >> std::get_time(&tm, "%Y-%m-%d");

            if (!ss.fail())
            {
                // Convert to epoch seconds for Cassandra DATE type
                tm.tm_hour = 0;
                tm.tm_min = 0;
                tm.tm_sec = 0;

                // Use timegm for UTC conversion (or portable equivalent)
#ifdef _WIN32
                std::time_t epoch_seconds = _mkgmtime(&tm);
#else
                std::time_t epoch_seconds = timegm(&tm);
#endif

                // Use cass_date_from_epoch to convert seconds to Cassandra DATE format
                // Cassandra DATE is uint32: days_since_epoch + 2^31 (bias offset)
                cass_date = cass_date_from_epoch(epoch_seconds);
                parseSuccess = true;
            }
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setDate - Failed to parse date string");
            parseSuccess = false;
        }

        CassError rc;
        if (parseSuccess)
        {
            // Bind as native Cassandra DATE (uint32 - days since epoch with 2^31 bias)
            rc = cass_statement_bind_uint32(m_statement.get(), parameterIndex - 1, cass_date);
        }
        else
        {
            // Fall back to string binding if parsing failed
            rc = cass_statement_bind_string(m_statement.get(), parameterIndex - 1, value.c_str());
        }

        if (rc != CASS_OK)
        {
            std::string errorMsg = "Failed to bind date value '" + value + "'";
            return cpp_dbc::unexpected(DBException("735497230592", errorMsg, system_utils::captureCallStack()));
        }

        m_currentEntry.stringParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        bool parseSuccess = false;
        cass_int64_t timestamp_ms = 0;

        try
        {
            std::tm tm = {};
            std::istringstream ss(value);

            if (value.contains(':'))
            {
                // Format with time component: YYYY-MM-DD HH:MM:SS
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            }
            else
            {
                // Format with just date: YYYY-MM-DD
                ss >> std::get_time(&tm, "%Y-%m-%d");
            }

            if (!ss.fail())
            {
                // Convert to epoch milliseconds for Cassandra using UTC
#ifdef _WIN32
                std::time_t epoch_seconds = _mkgmtime(&tm);
#else
                std::time_t epoch_seconds = timegm(&tm);
#endif
                auto timePoint = std::chrono::system_clock::from_time_t(epoch_seconds);
                timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   timePoint.time_since_epoch())
                                   .count();
                parseSuccess = true;
            }
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setTimestamp - Failed to parse timestamp string");
            parseSuccess = false;
        }

        CassError rc;
        if (parseSuccess)
        {
            // Bind as native Cassandra timestamp (milliseconds since epoch)
            rc = cass_statement_bind_int64(m_statement.get(), parameterIndex - 1, timestamp_ms);
        }
        else
        {
            // Fall back to string binding if parsing failed
            rc = cass_statement_bind_string(m_statement.get(), parameterIndex - 1, value.c_str());
        }

        if (rc != CASS_OK)
        {
            std::string errorMsg = "Failed to bind timestamp value '" + value + "'";
            return cpp_dbc::unexpected(DBException("735497230592", errorMsg, system_utils::captureCallStack()));
        }

        m_currentEntry.stringParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setUUID(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("869623869235", "Statement closed", system_utils::captureCallStack()));
        }

        // Format the UUID string if needed
        std::string uuidStr = value;
        if (value.length() == 32 && !value.contains('-'))
        {
            // Insert hyphens in UUID format: 8-4-4-4-12
            uuidStr.insert(8, "-");
            uuidStr.insert(13, "-");
            uuidStr.insert(18, "-");
            uuidStr.insert(23, "-");
        }

        // Try to parse and bind as UUID
        CassUuid uuid;
        CassError uuidParseResult = cass_uuid_from_string(uuidStr.c_str(), &uuid);
        CassError rc;

        if (uuidParseResult == CASS_OK)
        {
            rc = cass_statement_bind_uuid(m_statement.get(), parameterIndex - 1, uuid);
        }
        else
        {
            // UUID parsing failed, fall back to string binding
            rc = cass_statement_bind_string(m_statement.get(), parameterIndex - 1, value.c_str());
        }

        if (rc != CASS_OK)
        {
            std::string errorMsg = "Failed to bind UUID value '" + value + "'";
            return cpp_dbc::unexpected(DBException("735497230592", errorMsg, system_utils::captureCallStack()));
        }

        m_currentEntry.stringParams.emplace_back(parameterIndex - 1, value);
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        if (!x)
        {
            return cpp_dbc::unexpected(DBException("982374982374", "InputStream is null", system_utils::captureCallStack()));
        }

        try
        {
            std::vector<uint8_t> buffer;
            std::array<uint8_t, 4096> temp{};
            while (true)
            {
                int bytesRead = x->read(temp.data(), temp.size());
                if (bytesRead <= 0)
                {
                    break;
                }
                buffer.insert(buffer.end(), temp.begin(), temp.begin() + bytesRead);
            }
            return setBytes(std::nothrow, parameterIndex, buffer);
        }
        catch (const std::exception &ex)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setBinaryStream - Exception: " << ex.what());
            return cpp_dbc::unexpected(DBException("892374892374", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setBinaryStream - Unknown exception");
            return cpp_dbc::unexpected(DBException("892374892374", "Unknown error reading stream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        if (!x)
        {
            return cpp_dbc::unexpected(DBException("982374982374", "InputStream is null", system_utils::captureCallStack()));
        }

        try
        {
            std::vector<uint8_t> buffer;
            buffer.reserve(length);
            std::array<uint8_t, 4096> temp{};
            size_t totalRead = 0;

            while (totalRead < length)
            {
                size_t toRead = std::min(temp.size(), length - totalRead);
                int bytesRead = x->read(temp.data(), toRead);
                if (bytesRead <= 0)
                {
                    break;
                }
                buffer.insert(buffer.end(), temp.begin(), temp.begin() + bytesRead);
                totalRead += bytesRead;
            }

            return setBytes(std::nothrow, parameterIndex, buffer);
        }
        catch (const std::exception &ex)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setBinaryStream - Exception: " << ex.what());
            return cpp_dbc::unexpected(DBException("892374892374", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::setBinaryStream - Unknown exception");
            return cpp_dbc::unexpected(DBException("892374892374", "Unknown error reading stream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
    }

    cpp_dbc::expected<void, DBException> ScyllaDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_statement)
        {
            return cpp_dbc::unexpected(DBException("0DD0D3E7440E", "Statement closed", system_utils::captureCallStack()));
        }

        if (cass_statement_bind_bytes(m_statement.get(), parameterIndex - 1, x, length) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("735497230592", "Failed to bind bytes", system_utils::captureCallStack()));
        }

        std::vector<uint8_t> vec(x, x + length);
        m_currentEntry.bytesParams.emplace_back(parameterIndex - 1, vec);
        return {};
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
