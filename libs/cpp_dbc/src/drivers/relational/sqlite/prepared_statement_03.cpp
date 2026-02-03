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
#include "cpp_dbc/drivers/relational/sqlite_blob.hpp"
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
    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("U1A2B3C4D5E6", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

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
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            std::vector<uint8_t> data = x->getBytes(0, x->length());
            m_blobValues[parameterIndex - 1] = std::move(data);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SBLO1A2B3C4D",
                                                   std::string("setBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SBLO1A2B3C4E",
                                                   "setBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("B9C0D1E2F3G4", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            sqlite3 *dbPtr = getSQLiteConnection();

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
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            std::vector<uint8_t> data;
            uint8_t buffer[4096];
            int bytesRead;
            while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SBIS1A2B3C4D",
                                                   std::string("setBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SBIS1A2B3C4E",
                                                   "setBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("F9G0H1I2J3K4", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            sqlite3 *dbPtr = getSQLiteConnection();

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
                if (!nullResult)
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            std::vector<uint8_t> data;
            data.reserve(length);
            uint8_t buffer[4096];
            size_t totalBytesRead = 0;
            int bytesRead;
            while (totalBytesRead < length && (bytesRead = x->read(buffer, static_cast<int>(std::min(sizeof(buffer), length - totalBytesRead)))) > 0)
            {
                data.insert(data.end(), buffer, buffer + bytesRead);
                totalBytesRead += bytesRead;
            }

            m_blobValues[parameterIndex - 1] = std::move(data);

            int result = sqlite3_bind_blob(m_stmt.get(), parameterIndex,
                                           m_blobValues[parameterIndex - 1].data(),
                                           static_cast<int>(m_blobValues[parameterIndex - 1].size()),
                                           SQLITE_STATIC);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("D3E4F5G6H7I8", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(dbPtr)),
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SBISL1A2B3C4",
                                                   std::string("setBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SBISL1A2B3C5",
                                                   "setBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("U4A5B6C7D8E9", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            sqlite3 *dbPtr = getSQLiteConnection();

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
                if (!nullResult)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SBYT1A2B3C4D",
                                                   std::string("setBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SBYT1A2B3C4E",
                                                   "setBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
