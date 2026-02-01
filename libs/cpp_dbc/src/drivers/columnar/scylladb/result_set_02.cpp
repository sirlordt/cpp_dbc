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
 * @file result_set_02.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBResultSet nothrow methods (part 1)
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

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::next(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        if (!m_iterator)
        {
            SCYLLADB_DEBUG("ScyllaDBResultSet::next - Iterator is null, returning false");
            return false;
        }

        if (cass_iterator_next(m_iterator.get()))
        {
            m_currentRow = cass_iterator_get_row(m_iterator.get());
            m_rowPosition++;
            SCYLLADB_DEBUG("ScyllaDBResultSet::next - Advanced to row " << m_rowPosition);
            return true;
        }
        SCYLLADB_DEBUG("ScyllaDBResultSet::next - No more rows");
        m_currentRow = nullptr;
        return false;
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return m_rowPosition == 0;
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return !m_currentRow && m_rowPosition > 0; // Approximate
    }

    cpp_dbc::expected<uint64_t, DBException> ScyllaDBResultSet::getRow(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> ScyllaDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("Q7R8S9T0U1V2", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return 0;
        }

        cass_int32_t output;
        if (cass_value_get_int32(val, &output) != CASS_OK)
        {
            // Try parsing from other types? For now, strict.
            return cpp_dbc::unexpected(DBException("I9J0K1L2M3N4", "Failed to get int32", system_utils::captureCallStack()));
        }
        return output;
    }

    cpp_dbc::expected<long, DBException> ScyllaDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("D7F6C2471F23", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return 0;
        }

        cass_int64_t output;
        if (cass_value_get_int64(val, &output) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("SC2B3C4D5E6F", "Failed to get int64", system_utils::captureCallStack()));
        }
        return output;
    }

    cpp_dbc::expected<double, DBException> ScyllaDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("C6D5D1730470", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return 0.0;
        }

        cass_double_t output;
        if (cass_value_get_double(val, &output) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("K1L2M3N4O5P6", "Failed to get double", system_utils::captureCallStack()));
        }
        return output;
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("R8S9T0U1V2W3", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return std::string("");
        }

        // Standard string handling - no special type detection
        // For special types like UUID, timestamp, etc., use the dedicated methods
        const char *output;
        size_t output_length;
        if (cass_value_get_string(val, &output, &output_length) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("L2M3N4O5P6Q7", "Failed to get string", system_utils::captureCallStack()));
        }
        return std::string(output, output_length);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("S9T0U1V2W3X4", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return false;
        }

        cass_bool_t output;
        if (cass_value_get_bool(val, &output) != CASS_OK)
        {
            return cpp_dbc::unexpected(DBException("M3N4O5P6Q7R8", "Failed to get boolean", system_utils::captureCallStack()));
        }
        return output == cass_true;
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("T0U1V2W3X4Y5", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        return cass_value_is_null(val);
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getUUID(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("U1V2W3X4Y5Z6", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return std::string("");
        }

        CassValueType valueType = cass_value_type(val);
        if (valueType == CASS_VALUE_TYPE_UUID || valueType == CASS_VALUE_TYPE_TIMEUUID)
        {
            CassUuid uuid;
            if (cass_value_get_uuid(val, &uuid) == CASS_OK)
            {
                char uuidStr[CASS_UUID_STRING_LENGTH];
                cass_uuid_string(uuid, uuidStr);
                return std::string(uuidStr);
            }
        }

        // If not a UUID or couldn't extract as UUID, try to get as string
        const char *output;
        size_t output_length;
        if (cass_value_get_string(val, &output, &output_length) == CASS_OK)
        {
            return std::string(output, output_length);
        }

        return cpp_dbc::unexpected(DBException("N4O5P6Q7R8S9", "Failed to get UUID", system_utils::captureCallStack()));
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("V2W3X4Y5Z6A7", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return std::string("");
        }

        CassValueType valueType = cass_value_type(val);

        // Handle native Cassandra DATE type (uint32 - days since epoch with 2^31 bias)
        if (valueType == CASS_VALUE_TYPE_DATE)
        {
            cass_uint32_t date_value;
            if (cass_value_get_uint32(val, &date_value) == CASS_OK)
            {
                // Convert Cassandra date (days since epoch with 2^31 bias) to time_t
                // Cassandra DATE is stored as: days_since_epoch + 2^31
                // So we subtract 2^31 to get actual days since 1970-01-01
                int64_t days_since_epoch = static_cast<int64_t>(date_value) - (1LL << 31);
                std::time_t time_seconds = days_since_epoch * 86400; // 86400 seconds per day

                std::tm tm_buf{};
#ifdef _WIN32
                if (gmtime_s(&tm_buf, &time_seconds) == 0)
#else
                if (gmtime_r(&time_seconds, &tm_buf))
#endif
                {
                    std::stringstream ss;
                    ss << std::put_time(&tm_buf, "%Y-%m-%d");
                    return ss.str();
                }
            }
        }

        // Handle TIMESTAMP type (int64 - milliseconds since epoch)
        if (valueType == CASS_VALUE_TYPE_TIMESTAMP)
        {
            cass_int64_t timestamp_ms;
            if (cass_value_get_int64(val, &timestamp_ms) == CASS_OK)
            {
                // Convert timestamp to date string (YYYY-MM-DD)
                std::time_t time_seconds = timestamp_ms / 1000;
                std::tm tm_buf{};
#ifdef _WIN32
                if (gmtime_s(&tm_buf, &time_seconds) == 0)
#else
                if (gmtime_r(&time_seconds, &tm_buf))
#endif
                {
                    std::stringstream ss;
                    ss << std::put_time(&tm_buf, "%Y-%m-%d");
                    return ss.str();
                }
            }
        }

        // Try to get as string if not a date/timestamp or conversion failed
        const char *output;
        size_t output_length;
        if (cass_value_get_string(val, &output, &output_length) == CASS_OK)
        {
            return std::string(output, output_length);
        }

        return cpp_dbc::unexpected(DBException("O5P6Q7R8S9T0", "Failed to get date", system_utils::captureCallStack()));
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        auto validationResult = validateCurrentRow(std::nothrow);
        if (!validationResult.has_value())
        {
            return cpp_dbc::unexpected(validationResult.error());
        }
        if (columnIndex < 1 || columnIndex > m_columnCount)
        {
            return cpp_dbc::unexpected(DBException("W3X4Y5Z6A7B8", "Invalid column index", system_utils::captureCallStack()));
        }

        const CassValue *val = get_column_value(m_currentRow, columnIndex - 1, m_columnCount);
        if (cass_value_is_null(val))
        {
            return std::string("");
        }

        if (cass_value_type(val) == CASS_VALUE_TYPE_TIMESTAMP)
        {
            cass_int64_t timestamp_ms;
            if (cass_value_get_int64(val, &timestamp_ms) == CASS_OK)
            {
                // Convert timestamp to full datetime string (YYYY-MM-DD HH:MM:SS)
                std::time_t time_seconds = timestamp_ms / 1000;
                std::tm tm_buf{};
#ifdef _WIN32
                if (gmtime_s(&tm_buf, &time_seconds) == 0)
#else
                if (gmtime_r(&time_seconds, &tm_buf))
#endif
                {
                    std::stringstream ss;
                    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
                    return ss.str();
                }
            }
        }

        // Try to get as string if not a timestamp or conversion failed
        const char *output;
        size_t output_length;
        if (cass_value_get_string(val, &output, &output_length) == CASS_OK)
        {
            return std::string(output, output_length);
        }

        return cpp_dbc::unexpected(DBException("P6Q7R8S9T0U1", "Failed to get timestamp", system_utils::captureCallStack()));
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
