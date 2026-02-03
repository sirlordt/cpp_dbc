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

 @file result_set_04.cpp
 @brief Firebird database driver implementation - Part 7: FirebirdDBResultSet nothrow methods (part 2 - blob/binary)

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

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> FirebirdDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (columnIndex >= m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("FB1M3N4O5P6Q", "Column index out of range: " + std::to_string(columnIndex),
                                                       system_utils::captureCallStack()));
            }

            if (m_nullIndicators[columnIndex] < 0)
            {
                return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>(nullptr);
            }

            XSQLVAR *var = &m_sqlda->sqlvar[columnIndex];
            if ((var->sqltype & ~1) != SQL_BLOB)
            {
                return cpp_dbc::unexpected(DBException("FB2N4O5P6Q7R", "Column is not a BLOB type", system_utils::captureCallStack()));
            }

            auto conn = m_connection.lock();
            if (!conn)
            {
                return cpp_dbc::unexpected(DBException("FB3O5P6Q7R8S", "Connection has been closed", system_utils::captureCallStack()));
            }

            ISC_QUAD *blobId = reinterpret_cast<ISC_QUAD *>(var->sqldata);
            return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>(std::static_pointer_cast<Blob>(std::make_shared<FirebirdBlob>(conn, *blobId)));
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A3B0D4F6C8E2", std::string("Exception in getBlob: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B4C1E5A7D9F3", "Unknown exception in getBlob", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> FirebirdDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB4P6Q7R8S9T", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBlob(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C5D2F6B8E0A4", std::string("Exception in getBlob(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D6E3A7C9F1B5", "Unknown exception in getBlob(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> FirebirdDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            auto blobResult = getBlob(std::nothrow, columnIndex);
            if (!blobResult)
            {
                return cpp_dbc::unexpected(blobResult.error());
            }
            auto blob = blobResult.value();
            if (!blob)
                return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>(nullptr);
            return blob->getBinaryStream();
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E7F4B8D0A2C6", std::string("Exception in getBinaryStream: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F8A5C9E1B3D7", "Unknown exception in getBinaryStream", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> FirebirdDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB5Q7R8S9T0U", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBinaryStream(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A9B6D0F2C4E8", std::string("Exception in getBinaryStream(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B0C7E1A3D5F9", "Unknown exception in getBinaryStream(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> FirebirdDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            auto blobResult = getBlob(std::nothrow, columnIndex);
            if (!blobResult)
            {
                return cpp_dbc::unexpected(blobResult.error());
            }
            auto blob = blobResult.value();
            if (!blob)
                return std::vector<uint8_t>();
            return blob->getBytes(0, blob->length());
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C1D8F2B4E6A0", std::string("Exception in getBytes: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D2E9A3C5F7B1", "Unknown exception in getBytes", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> FirebirdDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB6R8S9T0U1V", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBytes(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E3F0A4B6D8C2", std::string("Exception in getBytes(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F4A1B5C7E9D3", "Unknown exception in getBytes(string)", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
