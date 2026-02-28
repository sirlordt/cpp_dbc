#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"

#if USE_POSTGRESQL
#include <libpq-fe.h>
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
        private:
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

            /**
             * @brief Get a locked pointer to the PostgreSQL connection handle
             * @return Raw pointer to the PGconn handle
             * @throws DBException if the connection has been closed
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

            PGconn *getPGConnection() const
            {
                auto r = getPGConnection(std::nothrow);
                if (!r.has_value())
                {
                    throw r.error();
                }
                return r.value();
            }

        public:
            /**
             * @brief Constructor for creating a new BLOB
             * @param conn Shared pointer to the PostgreSQL connection handle
             */
            explicit PostgreSQLBlob(std::shared_ptr<PGconn> conn)
                : m_conn(conn), m_loaded(true) {}

            /**
             * @brief Constructor for loading an existing BLOB by OID
             * @param conn Shared pointer to the PostgreSQL connection handle
             * @param oid The OID of the large object to load
             */
            PostgreSQLBlob(std::shared_ptr<PGconn> conn, Oid oid)
                : m_conn(conn), m_lobOid(oid) {}

            /**
             * @brief Constructor for creating a BLOB from existing data
             * @param conn Shared pointer to the PostgreSQL connection handle
             * @param initialData The initial data for the BLOB
             */
            PostgreSQLBlob(std::shared_ptr<PGconn> conn, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_conn(conn), m_loaded(true) {}

            static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn) noexcept
            {
                return std::make_shared<PostgreSQLBlob>(conn);
            }

            static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn, Oid oid) noexcept
            {
                return std::make_shared<PostgreSQLBlob>(conn, oid);
            }

            static cpp_dbc::expected<std::shared_ptr<PostgreSQLBlob>, DBException> create(std::nothrow_t, std::shared_ptr<PGconn> conn,
                const std::vector<uint8_t> &initialData) noexcept
            {
                return std::make_shared<PostgreSQLBlob>(conn, initialData);
            }

            static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn)
            {
                auto r = create(std::nothrow, conn);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn, Oid oid)
            {
                auto r = create(std::nothrow, conn, oid);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            static std::shared_ptr<PostgreSQLBlob> create(std::shared_ptr<PGconn> conn,
                const std::vector<uint8_t> &initialData)
            {
                auto r = create(std::nothrow, conn, initialData);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const PostgreSQLBlob &other) noexcept
            {
                if (this == &other) { return {}; }
                auto r = MemoryBlob::copyFrom(std::nothrow, other);
                if (!r.has_value()) { return r; }
                m_conn = other.m_conn;
                m_lobOid = other.m_lobOid;
                m_loaded = other.m_loaded;
                return {};
            }

            void copyFrom(const PostgreSQLBlob &other)
            {
                auto r = copyFrom(std::nothrow, other);
                if (!r.has_value()) { throw r.error(); }
            }

            /**
             * @brief Check if the connection is still valid
             * @return true if the connection is still valid
             */
            bool isConnectionValid() const
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

                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    return cpp_dbc::unexpected(DBException("PLX9YW4JL8V7", "Failed to start transaction for BLOB loading: " + error, system_utils::captureCallStack()));
                }
                PQclear(res);

                int fd = lo_open(conn, m_lobOid, INV_READ);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    return cpp_dbc::unexpected(DBException("4LLXBMB5AMJ9", "Failed to open large object: " + std::to_string(m_lobOid), system_utils::captureCallStack()));
                }

                int loSize = lo_lseek(conn, fd, 0, SEEK_END);
                lo_lseek(conn, fd, 0, SEEK_SET);

                if (loSize > 0)
                {
                    m_data.resize(loSize);
                    int bytesRead = lo_read(conn, fd, reinterpret_cast<char *>(m_data.data()), loSize);
                    if (bytesRead != loSize)
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        return cpp_dbc::unexpected(DBException("GO1IJP3EU26A", "Failed to read large object data", system_utils::captureCallStack()));
                    }
                }
                else
                {
                    m_data.clear();
                }

                lo_close(conn, fd);

                res = PQexec(conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    return cpp_dbc::unexpected(DBException("PYQGW1S0NFFX", "Failed to commit transaction for BLOB loading: " + error, system_utils::captureCallStack()));
                }
                PQclear(res);

                m_loaded = true;
                return {};
            }

            void ensureLoaded() const
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value())
                {
                    throw r.error();
                }
            }

            // ====================================================================
            // THROWING API - Exception-based (requires __cpp_exceptions)
            // ====================================================================

            #ifdef __cpp_exceptions

            size_t length() const override
            {
                auto r = length(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                auto r = getBytes(std::nothrow, pos, length);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                auto r = getBinaryStream(std::nothrow);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                auto r = setBinaryStream(std::nothrow, pos);
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                auto r = setBytes(std::nothrow, pos, bytes);
                if (!r.has_value()) { throw r.error(); }
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                auto r = setBytes(std::nothrow, pos, bytes, length);
                if (!r.has_value()) { throw r.error(); }
            }

            void truncate(size_t len) override
            {
                auto r = truncate(std::nothrow, len);
                if (!r.has_value()) { throw r.error(); }
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

            // Get the OID of the large object
            Oid getOid() const
            {
                return m_lobOid;
            }

            cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::length(std::nothrow);
            }

            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::getBytes(std::nothrow, pos, length);
            }

            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::getBinaryStream(std::nothrow);
            }

            cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBinaryStream(std::nothrow, pos);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes);
            }

            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
                return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
            }

            cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override
            {
                auto r = ensureLoaded(std::nothrow);
                if (!r.has_value()) { return cpp_dbc::unexpected(r.error()); }
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

                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    return cpp_dbc::unexpected(DBException("MP57LH4DNE61", "Failed to start transaction for BLOB saving: " + error, system_utils::captureCallStack()));
                }
                PQclear(res);

                if (m_lobOid == 0)
                {
                    m_lobOid = lo_creat(conn, INV_WRITE);
                    if (m_lobOid == 0)
                    {
                        PQexec(conn, "ROLLBACK");
                        return cpp_dbc::unexpected(DBException("4KAPV652CRQU", "Failed to create large object", system_utils::captureCallStack()));
                    }
                }

                int fd = lo_open(conn, m_lobOid, INV_WRITE);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    return cpp_dbc::unexpected(DBException("N38L3OFZ9NK6", "Failed to open large object for writing", system_utils::captureCallStack()));
                }

                lo_truncate(conn, fd, 0);

                if (!m_data.empty())
                {
                    int bytesWritten = lo_write(conn, fd, reinterpret_cast<const char *>(m_data.data()), m_data.size());
                    if (bytesWritten != static_cast<int>(m_data.size()))
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        return cpp_dbc::unexpected(DBException("PV3L5F2NMUOH", "Failed to write large object data", system_utils::captureCallStack()));
                    }
                }

                lo_close(conn, fd);

                res = PQexec(conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    return cpp_dbc::unexpected(DBException("AZ9LET1SNHY9", "Failed to commit transaction for BLOB saving: " + error, system_utils::captureCallStack()));
                }
                PQclear(res);

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
                        // Start a transaction if not already in one
                        PGresult *res = PQexec(conn.get(), "BEGIN");
                        if (PQresultStatus(res) == PGRES_COMMAND_OK)
                        {
                            PQclear(res);

                            // Delete the large object
                            lo_unlink(conn.get(), m_lobOid);

                            // Commit the transaction
                            res = PQexec(conn.get(), "COMMIT");
                            PQclear(res);
                        }
                        else
                        {
                            PQclear(res);
                        }
                    }

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
        };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
