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

 @file postgresql_blob.hpp
 @brief Tests for PostgreSQL database operations

*/

#ifndef CPP_DBC_POSTGRESQL_BLOB_HPP
#define CPP_DBC_POSTGRESQL_BLOB_HPP

#include "../cpp_dbc.hpp"
#include "../blob.hpp"

#if USE_POSTGRESQL
#include <libpq-fe.h>

// Define constants for large object access modes if not already defined
#ifndef INV_READ
#define INV_READ 0x00040000
#endif

#ifndef INV_WRITE
#define INV_WRITE 0x00020000
#endif

namespace cpp_dbc
{
    namespace PostgreSQL
    {
        // PostgreSQL implementation of InputStream
        class PostgreSQLInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            PostgreSQLInputStream(const char *buffer, size_t length)
                : m_data(buffer, buffer + length), m_position(0) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (m_position >= m_data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min(length, m_data.size() - m_position);
                std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
                m_position += bytesToRead;
                return static_cast<int>(bytesToRead);
            }

            void skip(size_t n) override
            {
                m_position = std::min(m_position + n, m_data.size());
            }

            void close() override
            {
                // Nothing to do for memory stream
            }
        };

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
            Oid m_lobOid{0};
            bool m_loaded{false};

            /**
             * @brief Get a locked pointer to the PostgreSQL connection handle
             * @return Raw pointer to the PGconn handle
             * @throws DBException if the connection has been closed
             */
            PGconn *getPGConnection() const
            {
                auto conn = m_conn.lock();
                if (!conn)
                {
                    throw DBException("PG_BLOB_CONN_CLOSED", "Connection has been closed", system_utils::captureCallStack());
                }
                return conn.get();
            }

        public:
            /**
             * @brief Constructor for creating a new BLOB
             * @param conn Shared pointer to the PostgreSQL connection handle
             */
            PostgreSQLBlob(std::shared_ptr<PGconn> conn)
                : m_conn(conn), m_lobOid(0), m_loaded(true) {}

            /**
             * @brief Constructor for loading an existing BLOB by OID
             * @param conn Shared pointer to the PostgreSQL connection handle
             * @param oid The OID of the large object to load
             */
            PostgreSQLBlob(std::shared_ptr<PGconn> conn, Oid oid)
                : m_conn(conn), m_lobOid(oid), m_loaded(false) {}

            /**
             * @brief Constructor for creating a BLOB from existing data
             * @param conn Shared pointer to the PostgreSQL connection handle
             * @param initialData The initial data for the BLOB
             */
            PostgreSQLBlob(std::shared_ptr<PGconn> conn, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), m_conn(conn), m_lobOid(0), m_loaded(true) {}

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
            void ensureLoaded()
            {
                if (m_loaded || m_lobOid == 0)
                    return;

                // Get PostgreSQL connection safely - throws if connection is closed
                PGconn *conn = getPGConnection();

                // Start a transaction if not already in one
                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("P1G2S3Q4L5B", "Failed to start transaction for BLOB loading: " + error, system_utils::captureCallStack());
                }
                PQclear(res);

                // Open the large object
                int fd = lo_open(conn, m_lobOid, INV_READ);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    throw DBException("L6O7B8O9P0E", "Failed to open large object: " + std::to_string(m_lobOid), system_utils::captureCallStack());
                }

                // Get the size of the large object
                int loSize = lo_lseek(conn, fd, 0, SEEK_END);
                lo_lseek(conn, fd, 0, SEEK_SET);

                if (loSize > 0)
                {
                    // Read the data
                    m_data.resize(loSize);
                    int bytesRead = lo_read(conn, fd, reinterpret_cast<char *>(m_data.data()), loSize);
                    if (bytesRead != loSize)
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        throw DBException("N1R2E3A4D5L", "Failed to read large object data", system_utils::captureCallStack());
                    }
                }
                else
                {
                    m_data.clear();
                }

                // Close the large object
                lo_close(conn, fd);

                // Commit the transaction
                res = PQexec(conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("O6B7C8O9M0M", "Failed to commit transaction for BLOB loading: " + error, system_utils::captureCallStack());
                }
                PQclear(res);

                m_loaded = true;
            }

            // Override methods that need to ensure the BLOB is loaded
            size_t length() const override
            {
                const_cast<PostgreSQLBlob *>(this)->ensureLoaded();
                return MemoryBlob::length();
            }

            std::vector<uint8_t> getBytes(size_t pos, size_t length) const override
            {
                const_cast<PostgreSQLBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBytes(pos, length);
            }

            std::shared_ptr<InputStream> getBinaryStream() const override
            {
                const_cast<PostgreSQLBlob *>(this)->ensureLoaded();
                return MemoryBlob::getBinaryStream();
            }

            std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override
            {
                ensureLoaded();
                return MemoryBlob::setBinaryStream(pos);
            }

            void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes);
            }

            void setBytes(size_t pos, const uint8_t *bytes, size_t length) override
            {
                ensureLoaded();
                MemoryBlob::setBytes(pos, bytes, length);
            }

            void truncate(size_t len) override
            {
                ensureLoaded();
                MemoryBlob::truncate(len);
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
                // Get PostgreSQL connection safely - throws if connection is closed
                PGconn *conn = getPGConnection();

                // Start a transaction if not already in one
                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("I1T2S3A4V5E", "Failed to start transaction for BLOB saving: " + error, system_utils::captureCallStack());
                }
                PQclear(res);

                // Create a new large object if needed
                if (m_lobOid == 0)
                {
                    m_lobOid = lo_creat(conn, INV_WRITE);
                    if (m_lobOid == 0)
                    {
                        PQexec(conn, "ROLLBACK");
                        throw DBException("C6R7E8A9T0E", "Failed to create large object", system_utils::captureCallStack());
                    }
                }

                // Open the large object
                int fd = lo_open(conn, m_lobOid, INV_WRITE);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    throw DBException("O1P2E3N4W5R", "Failed to open large object for writing", system_utils::captureCallStack());
                }

                // Truncate the large object
                lo_truncate(conn, fd, 0);

                // Write the data
                if (!m_data.empty())
                {
                    int bytesWritten = lo_write(conn, fd, reinterpret_cast<const char *>(m_data.data()), m_data.size());
                    if (bytesWritten != static_cast<int>(m_data.size()))
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        throw DBException("W6R7I8T9E0D", "Failed to write large object data", system_utils::captureCallStack());
                    }
                }

                // Close the large object
                lo_close(conn, fd);

                // Commit the transaction
                res = PQexec(conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("A1T2A3C4O5M", "Failed to commit transaction for BLOB saving: " + error, system_utils::captureCallStack());
                }
                PQclear(res);

                return m_lobOid;
            }

            // Get the OID of the large object
            Oid getOid() const
            {
                return m_lobOid;
            }

            void free() override
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

                MemoryBlob::free();
                m_loaded = false;
            }
        };

    } // namespace PostgreSQL
} // namespace cpp_dbc

#endif // USE_POSTGRESQL

#endif // CPP_DBC_POSTGRESQL_BLOB_HPP