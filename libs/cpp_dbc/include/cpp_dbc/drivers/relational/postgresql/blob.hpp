#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"
#include "handles.hpp"

#if USE_POSTGRESQL
#include <memory>
#include <cstring>
#include <string>

#include "input_stream.hpp"

namespace cpp_dbc::PostgreSQL
{
    /**
     * @brief PostgreSQL implementation of Blob using smart pointers for memory safety
     *
     * This class uses std::weak_ptr to safely reference the PostgreSQL connection handle,
     * preventing dangling pointer issues if the connection is closed while the
     * blob is still in use. All operations that require database access will
     * check if the connection is still valid before proceeding.
     */
    class PostgreSQLBlob : public MemoryBlob
    {
        using MemoryBlob::copyFrom; // Unhide base-class copyFrom overloads (cpp:S1242)

        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        /**
         * @brief Weak reference to the PostgreSQL connection handle
         *
         * Using weak_ptr allows us to detect when the connection has been closed
         * and avoid use-after-free errors. The connection owns the PGconn handle,
         * so we must ensure it's still valid before using it.
         */
        std::weak_ptr<PGconn> m_conn;
        mutable Oid m_lobOid{0};
        mutable bool m_loaded{false};

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Get a locked pointer to the PostgreSQL connection handle
         * @return Raw pointer to the PGconn handle
         */
        cpp_dbc::expected<PGconn *, DBException> getPGConnection(std::nothrow_t) const noexcept
        {
            auto conn = m_conn.lock();
            if (!conn)
            {
                return cpp_dbc::unexpected(DBException("G4XKOYHVFUEK", "Connection has been closed", system_utils::captureCallStack()));
            }
            return conn.get();
        }

        /**
         * @brief Begin a transaction scope for large-object operations
         *
         * PostgreSQL requires all lo_* calls to occur within a transaction.
         * If the caller is already inside a transaction, a SAVEPOINT is used
         * to avoid committing the caller's work. If idle, BEGIN is issued.
         *
         * @param conn Raw PGconn pointer
         * @param savepointName Unique name for the savepoint
         * @param[out] ownTransaction Set to true if BEGIN was issued, false if SAVEPOINT
         * @return Empty on success, DBException on failure
         */
        cpp_dbc::expected<void, DBException> beginLobScope(std::nothrow_t, PGconn *conn,
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
                    return cpp_dbc::unexpected(DBException("PLX9YW4JL8V7",
                        "Failed to create savepoint for BLOB operation: " + error,
                        system_utils::captureCallStack()));
                }
            }
            return {};
        }

        /**
         * @brief Commit/release a transaction scope for large-object operations
         *
         * Issues COMMIT if we own the transaction, RELEASE SAVEPOINT otherwise.
         */
        cpp_dbc::expected<void, DBException> commitLobScope(std::nothrow_t, PGconn *conn,
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
                    return cpp_dbc::unexpected(DBException("PYQGW1S0NFFX",
                        "Failed to release savepoint for BLOB operation: " + error,
                        system_utils::captureCallStack()));
                }
            }
            return {};
        }

        /**
         * @brief Rollback a transaction scope for large-object operations
         *
         * Issues ROLLBACK if we own the transaction, ROLLBACK TO SAVEPOINT + RELEASE otherwise.
         * Best-effort — errors are silently ignored since we are already on an error path.
         */
        void rollbackLobScope(std::nothrow_t, PGconn *conn,
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

    public:
        /**
         * @brief Constructor for creating a new BLOB
         * @param conn Shared pointer to the PostgreSQL connection handle
         */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<PGconn> conn) noexcept
            : m_conn(conn), m_loaded(true)
        {
            // Intentionally empty — all members initialized via in-class initializers
        }

        /**
         * @brief Constructor for loading an existing BLOB by OID
         * @param conn Shared pointer to the PostgreSQL connection handle
         * @param oid The OID of the large object to load
         */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<PGconn> conn, Oid oid) noexcept
            : m_conn(conn), m_lobOid(oid)
        {
            // Intentionally empty — all members initialized via in-class initializers
        }

        /**
         * @brief Constructor for creating a BLOB from existing data
         * @param conn Shared pointer to the PostgreSQL connection handle
         * @param initialData The initial data for the BLOB
         */
        PostgreSQLBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<PGconn> conn, const std::vector<uint8_t> &initialData) noexcept
            : MemoryBlob(initialData), m_conn(conn), m_loaded(true)
        {
            // Intentionally empty — MemoryBlob copies initialData, all members initialized
        }

        ~PostgreSQLBlob() override = default;

        PostgreSQLBlob(const PostgreSQLBlob &) = delete;
        PostgreSQLBlob &operator=(const PostgreSQLBlob &) = delete;
        PostgreSQLBlob(PostgreSQLBlob &&) = delete;
        PostgreSQLBlob &operator=(PostgreSQLBlob &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn)
        {
            auto r = create(std::nothrow, conn);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn, Oid oid)
        {
            auto r = create(std::nothrow, conn, oid);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn,
                                                      const std::vector<uint8_t> &initialData)
        {
            auto r = create(std::nothrow, conn, initialData);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }
#endif // __cpp_exceptions

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc (death sentence).
            auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, conn);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn, Oid oid) noexcept
        {
            auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, conn, oid);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn,
                                                                                      const std::vector<uint8_t> &initialData) noexcept
        {
            auto obj = std::make_shared<PostgreSQLBlob>(PrivateCtorTag{}, std::nothrow, conn, initialData);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        void copyFrom(const PostgreSQLBlob &other)
        {
            auto r = copyFrom(std::nothrow, other);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void ensureLoaded() const
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        size_t length() const override
        {
            auto r = length(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
        {
            auto r = getBytes(std::nothrow, pos, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<InputStream> getBinaryStream() const override
        {
            auto r = getBinaryStream(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
        {
            auto r = setBinaryStream(std::nothrow, pos);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
        {
            auto r = setBytes(std::nothrow, pos, bytes);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
        {
            auto r = setBytes(std::nothrow, pos, bytes, length);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void truncate(size_t len) override
        {
            auto r = truncate(std::nothrow, len);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        /**
         * @brief Save the BLOB data to the database
         *
         * This method safely accesses the connection through the weak_ptr,
         * ensuring the connection is still valid before attempting to write.
         *
         * @return The OID of the saved large object
         * @throws DBException if the connection has been closed or if writing fails
         */
        Oid save()
        {
            auto r = save(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void free() override
        {
            auto r = free(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const PostgreSQLBlob &other) noexcept
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
            m_conn = other.m_conn;
            m_lobOid = other.m_lobOid;
            m_loaded = other.m_loaded;
            return {};
        }

        /**
         * @brief Check if the connection is still valid
         * @return true if the connection is still valid
         */
        bool isConnectionValid() const noexcept override
        {
            return !m_conn.expired();
        }

        /**
         * @brief Load the BLOB data from the database if not already loaded
         *
         * This method safely accesses the connection through the weak_ptr,
         * ensuring the connection is still valid before attempting to read.
         */
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept
        {
            if (m_loaded || m_lobOid == 0)
            {
                return {};
            }
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

        // Get the OID of the large object
        Oid getOid() const noexcept
        {
            return m_lobOid;
        }

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::length(std::nothrow);
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::getBytes(std::nothrow, pos, length);
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::getBinaryStream(std::nothrow);
        }

        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBinaryStream(std::nothrow, pos);
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBytes(std::nothrow, pos, bytes);
        }

        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
        }

        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
        {
            auto r = ensureLoaded(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            return MemoryBlob::truncate(std::nothrow, len);
        }

        cpp_dbc::expected<Oid, DBException> save(std::nothrow_t) noexcept
        {
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

        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override
        {
            if (m_lobOid != 0)
            {
                // Try to get the connection - if it's closed, just clear local state
                auto conn = m_conn.lock();
                if (conn)
                {
                    bool ownTx = false;
                    auto scopeResult = beginLobScope(std::nothrow, conn.get(), "cpp_dbc_blob_free", ownTx);
                    if (scopeResult.has_value())
                    {
                        int unlinkResult = lo_unlink(conn.get(), m_lobOid);
                        if (unlinkResult == 1)
                        {
                            auto commitResult = commitLobScope(std::nothrow, conn.get(), "cpp_dbc_blob_free", ownTx);
                            if (commitResult.has_value())
                            {
                                m_lobOid = 0;
                            }
                            else
                            {
                                // Commit failed — large object may still exist
                                rollbackLobScope(std::nothrow, conn.get(), "cpp_dbc_blob_free", ownTx);
                            }
                        }
                        else
                        {
                            // lo_unlink failed — large object still exists
                            rollbackLobScope(std::nothrow, conn.get(), "cpp_dbc_blob_free", ownTx);
                        }
                    }
                    // If beginLobScope failed, m_lobOid is not cleared — object still exists
                }
                else
                {
                    // Connection is gone — cannot delete server-side, clear local state only
                    m_lobOid = 0;
                }
            }

            auto r = MemoryBlob::free(std::nothrow);
            if (!r.has_value())
            {
                return cpp_dbc::unexpected(r.error());
            }
            m_loaded = false;
            return {};
        }
    };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
