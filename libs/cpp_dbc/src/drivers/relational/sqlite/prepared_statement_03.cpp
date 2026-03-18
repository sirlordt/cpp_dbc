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
 @brief SQLite database driver implementation - SQLiteDBPreparedStatement nothrow methods (part 2 - blob/binary)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // BLOB support methods for SQLiteDBPreparedStatement - Nothrow API
    // No try/catch: all inner calls are nothrow (scoped_lock, C API calls, nothrow Blob/InputStream API).
    // Only death-sentence exceptions possible (std::bad_alloc from vector resize).

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("U1A2B3C4D5E6", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        if (parameterIndex <= 0)
        {
            return cpp_dbc::unexpected(DBException("U2A3B4C5D6E7", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                   system_utils::captureCallStack()));
        }

        int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
        if (parameterIndex > paramCount)
        {
            return cpp_dbc::unexpected(DBException("U3A4B5C6D7E8", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                   system_utils::captureCallStack()));
        }

        m_blobObjects[parameterIndex - 1] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        auto lengthResult = x->length(std::nothrow);
        if (!lengthResult.has_value())
        {
            return cpp_dbc::unexpected(lengthResult.error());
        }
        auto bytesResult = x->getBytes(std::nothrow, 0, lengthResult.value());
        if (!bytesResult.has_value())
        {
            return cpp_dbc::unexpected(bytesResult.error());
        }
        m_blobValues[parameterIndex - 1] = std::move(bytesResult.value());

        int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                       m_blobValues[parameterIndex - 1].data(),
                                       static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                       SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("SLHQ7R8S9T0U", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                   system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("B9C0D1E2F3G4", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        if (parameterIndex <= 0)
        {
            return cpp_dbc::unexpected(DBException("H5I6J7K8L9M0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                   system_utils::captureCallStack()));
        }

        int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
        if (parameterIndex > paramCount)
        {
            return cpp_dbc::unexpected(DBException("N1O2P3Q4R5S6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                   system_utils::captureCallStack()));
        }

        m_streamObjects[parameterIndex - 1] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        std::vector<uint8_t> data;
        uint8_t buffer[4096];
        auto readResult = x->read(std::nothrow, buffer, sizeof(buffer));
        while (readResult.has_value() && readResult.value() > 0)
        {
            data.insert(data.end(), buffer, buffer + readResult.value());
            readResult = x->read(std::nothrow, buffer, sizeof(buffer));
        }
        if (!readResult.has_value())
        {
            return cpp_dbc::unexpected(readResult.error());
        }

        m_blobValues[parameterIndex - 1] = std::move(data);

        int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                       m_blobValues[parameterIndex - 1].data(),
                                       static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                       SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("Z3A4B5C6D7E8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                   system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("F9G0H1I2J3K4", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        if (parameterIndex <= 0)
        {
            return cpp_dbc::unexpected(DBException("L5M6N7O8P9Q0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                   system_utils::captureCallStack()));
        }

        int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
        if (parameterIndex > paramCount)
        {
            return cpp_dbc::unexpected(DBException("R1S2T3U4V5W6", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                   system_utils::captureCallStack()));
        }

        m_streamObjects[parameterIndex - 1] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        std::vector<uint8_t> data;
        data.reserve(length);
        uint8_t buffer[4096];
        size_t totalBytesRead = 0;
        while (totalBytesRead < length)
        {
            size_t toRead = std::min(sizeof(buffer), length - totalBytesRead);
            auto readResult = x->read(std::nothrow, buffer, toRead);
            if (!readResult.has_value())
            {
                return cpp_dbc::unexpected(readResult.error());
            }
            if (readResult.value() <= 0)
            {
                break;
            }
            data.insert(data.end(), buffer, buffer + readResult.value());
            totalBytesRead += static_cast<size_t>(readResult.value());
        }

        m_blobValues[parameterIndex - 1] = std::move(data);

        int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                       m_blobValues[parameterIndex - 1].data(),
                                       static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                       SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("0W5BO5PAE56Z", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                   system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("U4A5B6C7D8E9", "Statement is closed");

        auto dbPtrResult = getSQLiteConnection(std::nothrow);
        if (!dbPtrResult.has_value())
        {
            return cpp_dbc::unexpected(dbPtrResult.error());
        }
        sqlite3 *dbPtr = dbPtrResult.value();

        if (parameterIndex <= 0)
        {
            return cpp_dbc::unexpected(DBException("U5A6B7C8D9E0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                   system_utils::captureCallStack()));
        }

        int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
        if (parameterIndex > paramCount)
        {
            return cpp_dbc::unexpected(DBException("U6A7B8C9D0E1", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                   system_utils::captureCallStack()));
        }

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        m_blobValues[parameterIndex - 1].resize(length);
        std::memcpy(m_blobValues[parameterIndex - 1].data(), x, length);

        int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                       m_blobValues[parameterIndex - 1].data(),
                                       static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                       SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("U7A8B9C0D1E2", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                   system_utils::captureCallStack()));
        }
        return {};
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
