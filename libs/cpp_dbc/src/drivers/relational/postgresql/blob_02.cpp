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
 @brief PostgreSQL database driver implementation - PostgreSQLBlob nothrow methods (factories, overrides, ensureLoaded, save, free)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // ── Nothrow static factories ─────────────────────────────────────────────────
    // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

    cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> PostgreSQLBlob::create(std::nothrow_t,
                                                                                            std::weak_ptr<PostgreSQLDBConnection> conn) noexcept
    {
        auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn));
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> PostgreSQLBlob::create(std::nothrow_t,
                                                                                            std::weak_ptr<PostgreSQLDBConnection> conn, Oid oid) noexcept
    {
        auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), oid);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> PostgreSQLBlob::create(std::nothrow_t,
                                                                                            std::weak_ptr<PostgreSQLDBConnection> conn,
                                                                                            const std::vector<uint8_t> &initialData) noexcept
    {
        auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, std::move(conn), initialData);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    // ── Nothrow public methods ───────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::copyFrom(std::nothrow_t, const PostgreSQLBlob &other) noexcept
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
        m_lobOid = other.m_lobOid;
        m_loaded = other.m_loaded;
        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::ensureLoaded(std::nothrow_t) const noexcept
    {
        if (m_loaded || m_lobOid == 0)
        {
            return {};
        }

        // Acquire connection lock for the entire lo_* lifecycle
        POSTGRESQL_STMT_LOCK_OR_RETURN("4LLXBMB5AMJ9", "Blob connection closed");

        auto connResult = getPGConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        PGconn *conn = connResult.value();

        bool ownTx = false;
        auto scopeResult = beginLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
        if (!scopeResult.has_value())
        {
            return cpp_dbc::unexpected(scopeResult.error());
        }

        int fd = lo_open(conn, m_lobOid, INV_READ);
        if (fd < 0)
        {
            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
            return cpp_dbc::unexpected(DBException("4LLXBMB5AMJ9", "Failed to open large object: " + std::to_string(m_lobOid), system_utils::captureCallStack()));
        }

        int loSize = lo_lseek(conn, fd, 0, SEEK_END);
        if (loSize < 0)
        {
            lo_close(conn, fd);
            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
            return cpp_dbc::unexpected(DBException("GO1IJP3EU26A", "Failed to determine large object size (lo_lseek SEEK_END)", system_utils::captureCallStack()));
        }

        if (lo_lseek(conn, fd, 0, SEEK_SET) < 0)
        {
            lo_close(conn, fd);
            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
            return cpp_dbc::unexpected(DBException("GO1IJP3EU26A", "Failed to reset large object position (lo_lseek SEEK_SET)", system_utils::captureCallStack()));
        }

        if (loSize > 0)
        {
            m_data.resize(loSize);
            int bytesRead = lo_read(conn, fd, reinterpret_cast<char *>(m_data.data()), loSize);
            if (bytesRead != loSize)
            {
                lo_close(conn, fd);
                rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
                return cpp_dbc::unexpected(DBException("GO1IJP3EU26A", "Failed to read large object data", system_utils::captureCallStack()));
            }
        }
        else
        {
            m_data.clear();
        }

        lo_close(conn, fd);

        auto commitResult = commitLobScope(std::nothrow, conn, "cpp_dbc_blob_load", ownTx);
        if (!commitResult.has_value())
        {
            return cpp_dbc::unexpected(commitResult.error());
        }

        m_loaded = true;
        return {};
    }

    cpp_dbc::expected<Oid, DBException> PostgreSQLBlob::save(std::nothrow_t) noexcept
    {
        // Ensure BLOB data is loaded before saving
        auto loadResult = ensureLoaded(std::nothrow);
        if (!loadResult.has_value())
        {
            return cpp_dbc::unexpected(loadResult.error());
        }

        // Acquire connection lock for the entire lo_* lifecycle
        POSTGRESQL_STMT_LOCK_OR_RETURN("4KAPV652CRQU", "Blob connection closed");

        auto connResult = getPGConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        PGconn *conn = connResult.value();

        bool ownTx = false;
        auto scopeResult = beginLobScope(std::nothrow, conn, "cpp_dbc_blob_save", ownTx);
        if (!scopeResult.has_value())
        {
            return cpp_dbc::unexpected(scopeResult.error());
        }

        if (m_lobOid == 0)
        {
            m_lobOid = lo_creat(conn, INV_WRITE);
            if (m_lobOid == 0)
            {
                rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_save", ownTx);
                return cpp_dbc::unexpected(DBException("4KAPV652CRQU", "Failed to create large object", system_utils::captureCallStack()));
            }
        }

        int fd = lo_open(conn, m_lobOid, INV_WRITE);
        if (fd < 0)
        {
            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_save", ownTx);
            return cpp_dbc::unexpected(DBException("N38L3OFZ9NK6", "Failed to open large object for writing", system_utils::captureCallStack()));
        }

        lo_truncate(conn, fd, 0);

        if (!m_data.empty())
        {
            int bytesWritten = lo_write(conn, fd, reinterpret_cast<const char *>(m_data.data()), m_data.size());
            if (bytesWritten != static_cast<int>(m_data.size()))
            {
                lo_close(conn, fd);
                rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_save", ownTx);
                return cpp_dbc::unexpected(DBException("PV3L5F2NMUOH", "Failed to write large object data", system_utils::captureCallStack()));
            }
        }

        lo_close(conn, fd);

        auto commitResult = commitLobScope(std::nothrow, conn, "cpp_dbc_blob_save", ownTx);
        if (!commitResult.has_value())
        {
            return cpp_dbc::unexpected(commitResult.error());
        }

        return m_lobOid;
    }

    // ── Nothrow Blob overrides ───────────────────────────────────────────────────

    cpp_dbc::expected<size_t, DBException> PostgreSQLBlob::length(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::length(std::nothrow);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> PostgreSQLBlob::getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::getBytes(std::nothrow, pos, length);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> PostgreSQLBlob::getBinaryStream(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::getBinaryStream(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> PostgreSQLBlob::setBinaryStream(std::nothrow_t, size_t pos) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::setBinaryStream(std::nothrow, pos);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::truncate(std::nothrow_t, size_t len) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
        return MemoryBlob::truncate(std::nothrow, len);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::free(std::nothrow_t) noexcept
    {
        if (m_lobOid != 0)
        {
            // Try to acquire lock and unlink the large object from the server
            auto connPtr = m_connection.lock();
            if (connPtr)
            {
                // Acquire connection lock for lo_unlink
                POSTGRESQL_STMT_LOCK_OR_RETURN("EMRA89D8U2RC", "Blob connection closed during free");

                auto pgResult = getPGConnection(std::nothrow);
                if (pgResult.has_value())
                {
                    PGconn *conn = pgResult.value();
                    bool ownTx = false;
                    auto scopeResult = beginLobScope(std::nothrow, conn, "cpp_dbc_blob_free", ownTx);
                    if (scopeResult.has_value())
                    {
                        int unlinkResult = lo_unlink(conn, m_lobOid);
                        if (unlinkResult != 1)
                        {
                            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_free", ownTx);
                            return cpp_dbc::unexpected(DBException("EMRA89D8U2RC",
                                "lo_unlink failed: large object still exists on server",
                                system_utils::captureCallStack()));
                        }

                        auto commitResult = commitLobScope(std::nothrow, conn, "cpp_dbc_blob_free", ownTx);
                        if (!commitResult.has_value())
                        {
                            rollbackLobScope(std::nothrow, conn, "cpp_dbc_blob_free", ownTx);
                            return cpp_dbc::unexpected(commitResult.error());
                        }
                    }
                }
            }
            // Connection gone or lock failed — cannot delete server-side, clear local state only
            m_lobOid = 0;
        }

        auto r = MemoryBlob::free(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        m_loaded = false;
        return {};
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
