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
 @brief Firebird database driver implementation - FirebirdBlob (private helpers, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

namespace cpp_dbc::Firebird
{

    // ── Private helpers ───────────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<FirebirdDBConnection>, DBException> FirebirdBlob::getConnection(std::nothrow_t) const noexcept
    {
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("FAPY178ZS5UI", "Connection has been closed", system_utils::captureCallStack()));
        }
        return conn;
    }

    cpp_dbc::expected<isc_db_handle *, DBException> FirebirdBlob::getDbHandle(std::nothrow_t) const noexcept
    {
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("3K4ERTK6XMFR", "Connection has been destroyed (getDbHandle)", system_utils::captureCallStack()));
        }
        return conn->m_conn.get();
    }

    cpp_dbc::expected<isc_tr_handle *, DBException> FirebirdBlob::getTrHandle(std::nothrow_t) const noexcept
    {
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("X3OE58Y3Z22Z", "Connection has been destroyed (getTrHandle)", system_utils::captureCallStack()));
        }
        return &conn->m_tr;
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    std::shared_ptr<FirebirdBlob> FirebirdBlob::create(std::weak_ptr<FirebirdDBConnection> conn)
    {
        auto r = create(std::nothrow, std::move(conn));
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<FirebirdBlob> FirebirdBlob::create(std::weak_ptr<FirebirdDBConnection> conn, ISC_QUAD blobId)
    {
        auto r = create(std::nothrow, std::move(conn), blobId);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<FirebirdBlob> FirebirdBlob::create(std::weak_ptr<FirebirdDBConnection> conn,
                                                        const std::vector<uint8_t> &initialData)
    {
        auto r = create(std::nothrow, std::move(conn), initialData);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void FirebirdBlob::copyFrom(const FirebirdBlob &other)
    {
        auto r = copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void FirebirdBlob::ensureLoaded() const
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    size_t FirebirdBlob::length() const
    {
        auto r = length(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::vector<uint8_t> FirebirdBlob::getBytes(size_t pos, size_t len) const
    {
        auto r = getBytes(std::nothrow, pos, len);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<InputStream> FirebirdBlob::getBinaryStream() const
    {
        auto r = getBinaryStream(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<OutputStream> FirebirdBlob::setBinaryStream(size_t pos)
    {
        auto r = setBinaryStream(std::nothrow, pos);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void FirebirdBlob::setBytes(size_t pos, const std::vector<uint8_t> &bytes)
    {
        auto r = setBytes(std::nothrow, pos, bytes);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void FirebirdBlob::setBytes(size_t pos, const uint8_t *bytes, size_t len)
    {
        auto r = setBytes(std::nothrow, pos, bytes, len);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void FirebirdBlob::truncate(size_t len)
    {
        auto r = truncate(std::nothrow, len);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    ISC_QUAD FirebirdBlob::save()
    {
        auto r = save(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void FirebirdBlob::free()
    {
        auto r = free(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
