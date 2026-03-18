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
 @brief PostgreSQL database driver implementation - PostgreSQLBlob (private helpers, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

namespace cpp_dbc::PostgreSQL
{

    // ── Private helpers ───────────────────────────────────────────────────────────

    cpp_dbc::expected<PGconn *, DBException> PostgreSQLBlob::getPGConnection(std::nothrow_t) const noexcept
    {
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("G4XKOYHVFUEK", "Connection has been closed", system_utils::captureCallStack()));
        }
        if (!conn->m_conn)
        {
            return cpp_dbc::unexpected(DBException("G4XKOYHVFUEK", "PostgreSQL connection handle is null", system_utils::captureCallStack()));
        }
        return conn->m_conn.get();
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::beginLobScope(std::nothrow_t, PGconn *conn,
                                                                        const char *savepointName,
                                                                        bool &ownTransaction) const noexcept
    {
        ownTransaction = (PQtransactionStatus(conn) == PQTRANS_IDLE);
        if (ownTransaction)
        {
            PGresultHandle res(PQexec(conn, "BEGIN"));
            if (PQresultStatus(res.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(res.get());
                return cpp_dbc::unexpected(DBException("PLX9YW4JL8V7",
                    "Failed to start transaction for BLOB operation: " + error,
                    system_utils::captureCallStack()));
            }
        }
        else
        {
            std::string sql = "SAVEPOINT ";
            sql += savepointName;
            PGresultHandle res(PQexec(conn, sql.c_str()));
            if (PQresultStatus(res.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(res.get());
                return cpp_dbc::unexpected(DBException("G4RSVK0YUWCJ",
                    "Failed to create savepoint for BLOB operation: " + error,
                    system_utils::captureCallStack()));
            }
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLBlob::commitLobScope(std::nothrow_t, PGconn *conn,
                                                                         const char *savepointName,
                                                                         bool ownTransaction) const noexcept
    {
        if (ownTransaction)
        {
            PGresultHandle res(PQexec(conn, "COMMIT"));
            if (PQresultStatus(res.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(res.get());
                return cpp_dbc::unexpected(DBException("PYQGW1S0NFFX",
                    "Failed to commit transaction for BLOB operation: " + error,
                    system_utils::captureCallStack()));
            }
        }
        else
        {
            std::string sql = "RELEASE SAVEPOINT ";
            sql += savepointName;
            PGresultHandle res(PQexec(conn, sql.c_str()));
            if (PQresultStatus(res.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(res.get());
                return cpp_dbc::unexpected(DBException("C13W7UNP30RS",
                    "Failed to release savepoint for BLOB operation: " + error,
                    system_utils::captureCallStack()));
            }
        }
        return {};
    }

    void PostgreSQLBlob::rollbackLobScope(std::nothrow_t, PGconn *conn,
                                           const char *savepointName,
                                           bool ownTransaction) const noexcept
    {
        if (ownTransaction)
        {
            PGresultHandle res(PQexec(conn, "ROLLBACK"));
            // Intentionally ignoring errors — already on error path
        }
        else
        {
            std::string rbSql = "ROLLBACK TO SAVEPOINT ";
            rbSql += savepointName;
            PGresultHandle rbRes(PQexec(conn, rbSql.c_str()));

            std::string rlSql = "RELEASE SAVEPOINT ";
            rlSql += savepointName;
            PGresultHandle rlRes(PQexec(conn, rlSql.c_str()));
            // Intentionally ignoring errors — already on error path
        }
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    std::shared_ptr<PostgreSQLBlob> PostgreSQLBlob::create(std::weak_ptr<PostgreSQLDBConnection> conn)
    {
        auto r = create(std::nothrow, std::move(conn));
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<PostgreSQLBlob> PostgreSQLBlob::create(std::weak_ptr<PostgreSQLDBConnection> conn, Oid oid)
    {
        auto r = create(std::nothrow, std::move(conn), oid);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<PostgreSQLBlob> PostgreSQLBlob::create(std::weak_ptr<PostgreSQLDBConnection> conn,
                                                            const std::vector<uint8_t> &initialData)
    {
        auto r = create(std::nothrow, std::move(conn), initialData);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    void PostgreSQLBlob::copyFrom(const PostgreSQLBlob &other)
    {
        auto r = copyFrom(std::nothrow, other);
        if (!r.has_value()) { throw r.error(); }
    }

    void PostgreSQLBlob::ensureLoaded() const
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value()) { throw r.error(); }
    }

    size_t PostgreSQLBlob::length() const
    {
        auto r = length(std::nothrow);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::vector<uint8_t> PostgreSQLBlob::getBytes(size_t pos, size_t len) const
    {
        auto r = getBytes(std::nothrow, pos, len);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<InputStream> PostgreSQLBlob::getBinaryStream() const
    {
        auto r = getBinaryStream(std::nothrow);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<OutputStream> PostgreSQLBlob::setBinaryStream(size_t pos)
    {
        auto r = setBinaryStream(std::nothrow, pos);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    void PostgreSQLBlob::setBytes(size_t pos, const std::vector<uint8_t> &bytes)
    {
        auto r = setBytes(std::nothrow, pos, bytes);
        if (!r.has_value()) { throw r.error(); }
    }

    void PostgreSQLBlob::setBytes(size_t pos, const uint8_t *bytes, size_t len)
    {
        auto r = setBytes(std::nothrow, pos, bytes, len);
        if (!r.has_value()) { throw r.error(); }
    }

    void PostgreSQLBlob::truncate(size_t len)
    {
        auto r = truncate(std::nothrow, len);
        if (!r.has_value()) { throw r.error(); }
    }

    Oid PostgreSQLBlob::save()
    {
        auto r = save(std::nothrow);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    void PostgreSQLBlob::free()
    {
        auto r = free(std::nothrow);
        if (!r.has_value()) { throw r.error(); }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
