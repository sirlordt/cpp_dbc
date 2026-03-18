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

 @file blob_02.cpp
 @brief MySQL database driver implementation - MySQLBlob nothrow methods (factories, overrides, ensureLoaded, save)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    // ── Nothrow static factories ─────────────────────────────────────────────────
    // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

    cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> MySQLBlob::create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn) noexcept
    {
        auto obj = std::make_shared<MySQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn));
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> MySQLBlob::create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn,
                                                                                  const std::string &tableName, const std::string &columnName,
                                                                                  const std::string &whereClause) noexcept
    {
        auto obj = std::make_shared<MySQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), tableName, columnName, whereClause);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> MySQLBlob::create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn,
                                                                                  const std::vector<uint8_t> &initialData) noexcept
    {
        auto obj = std::make_shared<MySQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), initialData);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    // ── Nothrow public methods ───────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> MySQLBlob::copyFrom(std::nothrow_t, const MySQLBlob &other) noexcept
    {
        if (this == &other)
        {
            return {};
        }
        auto r = MemoryBlob::copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            return r;
        }
        m_connection = other.m_connection;
        m_tableName = other.m_tableName;
        m_columnName = other.m_columnName;
        m_whereClause = other.m_whereClause;
        m_loaded = other.m_loaded;
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::ensureLoaded(std::nothrow_t) const noexcept
    {
        if (m_loaded)
        {
            return {};
        }

        // Validate identifiers before SQL construction to prevent injection
        auto colValid = validateIdentifier(std::nothrow, m_columnName);
        if (!colValid.has_value())
        {
            return cpp_dbc::unexpected(colValid.error());
        }
        auto tblValid = validateIdentifier(std::nothrow, m_tableName);
        if (!tblValid.has_value())
        {
            return cpp_dbc::unexpected(tblValid.error());
        }

        // Acquire connection lock for the entire MySQL statement lifecycle
        MYSQL_STMT_LOCK_OR_RETURN("6VDNWQ8STSAG", "Blob connection closed");

        auto mysqlResult = getMySQLHandle(std::nothrow);
        if (!mysqlResult.has_value())
        {
            return cpp_dbc::unexpected(mysqlResult.error());
        }
        MYSQL *mysql = mysqlResult.value();

        std::string query = "SELECT " + m_columnName + " FROM " + m_tableName;
        if (!m_whereClause.empty())
        {
            query += " WHERE " + m_whereClause;
        }
        if (mysql_query(mysql, query.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("6VDNWQ8STSAG", "Failed to fetch BLOB data: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
        }
        MySQLResHandle resultHandle(mysql_store_result(mysql));
        if (!resultHandle)
        {
            return cpp_dbc::unexpected(DBException("476QLR8BRXQK", "Failed to get result set for BLOB data: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
        }
        MYSQL_ROW row = mysql_fetch_row(resultHandle.get());
        if (!row)
        {
            return cpp_dbc::unexpected(DBException("QQ7NT0640BWW", "No data found for BLOB", system_utils::captureCallStack()));
        }
        const unsigned long *lengths = mysql_fetch_lengths(resultHandle.get());
        if (!lengths)
        {
            return cpp_dbc::unexpected(DBException("VKZ2WW134M1Q", "Failed to get BLOB data length", system_utils::captureCallStack()));
        }
        if (row[0] && lengths[0] > 0)
        {
            m_data.resize(lengths[0]);
            std::memcpy(m_data.data(), row[0], lengths[0]);
        }
        else
        {
            m_data.clear();
        }
        m_loaded = true;
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::save(std::nothrow_t) noexcept
    {
        if (m_tableName.empty() || m_columnName.empty() || m_whereClause.empty())
        {
            return {}; // Nothing to save
        }

        // Ensure BLOB data is loaded before saving to prevent overwriting with empty data
        auto loadResult = ensureLoaded(std::nothrow);
        if (!loadResult.has_value())
        {
            return cpp_dbc::unexpected(loadResult.error());
        }

        // Validate identifiers before SQL construction to prevent injection
        auto tblValid = validateIdentifier(std::nothrow, m_tableName);
        if (!tblValid.has_value())
        {
            return cpp_dbc::unexpected(tblValid.error());
        }
        auto colValid = validateIdentifier(std::nothrow, m_columnName);
        if (!colValid.has_value())
        {
            return cpp_dbc::unexpected(colValid.error());
        }

        // Acquire connection lock for the entire MySQL statement lifecycle
        MYSQL_STMT_LOCK_OR_RETURN("F5W35AZHZKK7", "Blob connection closed");

        auto mysqlResult = getMySQLHandle(std::nothrow);
        if (!mysqlResult.has_value())
        {
            return cpp_dbc::unexpected(mysqlResult.error());
        }
        MYSQL *mysql = mysqlResult.value();

        std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE " + m_whereClause;
        MySQLStmtHandle stmtHandle(mysql_stmt_init(mysql));
        if (!stmtHandle)
        {
            return cpp_dbc::unexpected(DBException("F5W35AZHZKK7", "Failed to initialize statement for BLOB update: " + std::string(mysql_error(mysql)), system_utils::captureCallStack()));
        }
        if (mysql_stmt_prepare(stmtHandle.get(), query.c_str(), query.length()) != 0)
        {
            std::string error = mysql_stmt_error(stmtHandle.get());
            return cpp_dbc::unexpected(DBException("1M7I6MJ8UHON", "Failed to prepare statement for BLOB update: " + error, system_utils::captureCallStack()));
        }
        MYSQL_BIND bind{};
        bind.buffer_type = MYSQL_TYPE_BLOB;
        bind.buffer = m_data.data();
        bind.buffer_length = m_data.size();
        bind.length_value = m_data.size();
        if (mysql_stmt_bind_param(stmtHandle.get(), &bind) != 0)
        {
            std::string error = mysql_stmt_error(stmtHandle.get());
            return cpp_dbc::unexpected(DBException("K4PXEPQPAR99", "Failed to bind BLOB data: " + error, system_utils::captureCallStack()));
        }
        if (mysql_stmt_execute(stmtHandle.get()) != 0)
        {
            std::string error = mysql_stmt_error(stmtHandle.get());
            return cpp_dbc::unexpected(DBException("SMT5X3RWBNV7", "Failed to update BLOB data: " + error, system_utils::captureCallStack()));
        }
        return {};
    }

    // ── Nothrow Blob overrides ───────────────────────────────────────────────────

    cpp_dbc::expected<size_t, DBException> MySQLBlob::length(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::length(std::nothrow);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> MySQLBlob::getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::getBytes(std::nothrow, pos, length);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> MySQLBlob::getBinaryStream(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::getBinaryStream(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> MySQLBlob::setBinaryStream(std::nothrow_t, size_t pos) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBinaryStream(std::nothrow, pos);
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes);
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::truncate(std::nothrow_t, size_t len) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::truncate(std::nothrow, len);
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::free(std::nothrow_t) noexcept
    {
        auto r = MemoryBlob::free(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        m_loaded = false;
        return {};
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
