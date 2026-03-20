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

 @file result_set_05.cpp
 @brief Firebird database driver implementation - FirebirdDBResultSet nothrow methods: BLOB support

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
        FIREBIRD_STMT_LOCK_OR_RETURN("OJ9DMC2WW02G", "Connection lost");

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
        auto blobResult = FirebirdBlob::create(std::nothrow, m_connection, *blobId);
        if (!blobResult.has_value())
        {
            return cpp_dbc::unexpected(blobResult.error());
        }
        return std::shared_ptr<Blob>(blobResult.value());
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> FirebirdDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("4UBKWXAL1538", "ResultSet closed");

        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB4P6Q7R8S9T", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBlob(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> FirebirdDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        auto blobResult = getBlob(std::nothrow, columnIndex);
        if (!blobResult.has_value())
        {
            return cpp_dbc::unexpected(blobResult.error());
        }
        auto blob = blobResult.value();
        if (!blob)
        {
            return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>(nullptr);
        }
        return blob->getBinaryStream(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> FirebirdDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("G8RHAZ8GH89X", "ResultSet closed");

        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB5Q7R8S9T0U", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBinaryStream(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> FirebirdDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        auto blobResult = getBlob(std::nothrow, columnIndex);
        if (!blobResult.has_value())
        {
            return cpp_dbc::unexpected(blobResult.error());
        }
        auto blob = blobResult.value();
        if (!blob)
        {
            return std::vector<uint8_t>();
        }
        auto lenResult = blob->length(std::nothrow);
        if (!lenResult.has_value())
        {
            return cpp_dbc::unexpected(lenResult.error());
        }
        return blob->getBytes(std::nothrow, 0, lenResult.value());
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> FirebirdDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("PAQA9BFBHYCS", "ResultSet closed");

        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB6R8S9T0U1V", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBytes(std::nothrow, it->second);
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
