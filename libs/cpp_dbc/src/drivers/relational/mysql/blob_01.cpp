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

 @file blob_01.cpp
 @brief MySQL database driver implementation - MySQLBlob (private helpers, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

namespace cpp_dbc::MySQL
{

    // ── Private helpers ───────────────────────────────────────────────────────────

    cpp_dbc::expected<MYSQL *, DBException> MySQLBlob::getMySQLHandle(std::nothrow_t) const noexcept
    {
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("SWUSJLDWXH53", "Connection has been closed", system_utils::captureCallStack()));
        }
        if (!conn->m_conn)
        {
            return cpp_dbc::unexpected(DBException("SWUSJLDWXH53", "MySQL connection handle is null", system_utils::captureCallStack()));
        }
        return conn->m_conn.get();
    }

    cpp_dbc::expected<void, DBException> MySQLBlob::validateIdentifier(std::nothrow_t, const std::string &identifier) noexcept
    {
        if (identifier.empty())
        {
            return cpp_dbc::unexpected(DBException("I9E7DCOTY7BD",
                "Empty database identifier",
                system_utils::captureCallStack()));
        }
        for (char c : identifier)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                return cpp_dbc::unexpected(DBException("BHE48KWWQ1GO",
                    "Invalid database identifier: contains disallowed characters (only alphanumeric and underscores allowed)",
                    system_utils::captureCallStack()));
            }
        }
        return {};
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    std::shared_ptr<MySQLBlob> MySQLBlob::create(std::weak_ptr<MySQLDBConnection> conn)
    {
        auto r = create(std::nothrow, std::move(conn));
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<MySQLBlob> MySQLBlob::create(std::weak_ptr<MySQLDBConnection> conn,
                                                  const std::string &tableName, const std::string &columnName,
                                                  const std::string &whereClause)
    {
        auto r = create(std::nothrow, std::move(conn), tableName, columnName, whereClause);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<MySQLBlob> MySQLBlob::create(std::weak_ptr<MySQLDBConnection> conn,
                                                  const std::vector<uint8_t> &initialData)
    {
        auto r = create(std::nothrow, std::move(conn), initialData);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void MySQLBlob::copyFrom(const MySQLBlob &other)
    {
        auto r = copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MySQLBlob::ensureLoaded() const
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    size_t MySQLBlob::length() const
    {
        auto r = length(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::vector<uint8_t> MySQLBlob::getBytes(size_t pos, size_t len) const
    {
        auto r = getBytes(std::nothrow, pos, len);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<InputStream> MySQLBlob::getBinaryStream() const
    {
        auto r = getBinaryStream(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<OutputStream> MySQLBlob::setBinaryStream(size_t pos)
    {
        auto r = setBinaryStream(std::nothrow, pos);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void MySQLBlob::setBytes(size_t pos, const std::vector<uint8_t> &bytes)
    {
        auto r = setBytes(std::nothrow, pos, bytes);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MySQLBlob::setBytes(size_t pos, const uint8_t *bytes, size_t len)
    {
        auto r = setBytes(std::nothrow, pos, bytes, len);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MySQLBlob::truncate(size_t len)
    {
        auto r = truncate(std::nothrow, len);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MySQLBlob::save()
    {
        auto r = save(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MySQLBlob::free()
    {
        auto r = free(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
