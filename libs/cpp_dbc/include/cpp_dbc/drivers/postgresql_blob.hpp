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
            const std::vector<uint8_t> data;
            size_t position;

        public:
            PostgreSQLInputStream(const char *buffer, size_t length)
                : data(buffer, buffer + length), position(0) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (position >= data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min(length, data.size() - position);
                std::memcpy(buffer, data.data() + position, bytesToRead);
                position += bytesToRead;
                return static_cast<int>(bytesToRead);
            }

            void skip(size_t n) override
            {
                position = std::min(position + n, data.size());
            }

            void close() override
            {
                // Nothing to do for memory stream
            }
        };

        // PostgreSQL implementation of Blob
        class PostgreSQLBlob : public MemoryBlob
        {
        private:
            PGconn *conn;
            Oid lobOid;
            bool loaded;

        public:
            // Constructor for creating a new BLOB
            PostgreSQLBlob(PGconn *conn)
                : conn(conn), lobOid(0), loaded(true) {}

            // Constructor for loading an existing BLOB by OID
            PostgreSQLBlob(PGconn *conn, Oid oid)
                : conn(conn), lobOid(oid), loaded(false) {}

            // Constructor for creating a BLOB from existing data
            PostgreSQLBlob(PGconn *conn, const std::vector<uint8_t> &initialData)
                : MemoryBlob(initialData), conn(conn), lobOid(0), loaded(true) {}

            // Load the BLOB data from the database if not already loaded
            void ensureLoaded()
            {
                if (loaded || lobOid == 0)
                    return;

                // Start a transaction if not already in one
                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("Failed to start transaction for BLOB loading: " + error);
                }
                PQclear(res);

                // Open the large object
                int fd = lo_open(conn, lobOid, INV_READ);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    throw DBException("Failed to open large object: " + std::to_string(lobOid));
                }

                // Get the size of the large object
                int loSize = lo_lseek(conn, fd, 0, SEEK_END);
                lo_lseek(conn, fd, 0, SEEK_SET);

                if (loSize > 0)
                {
                    // Read the data
                    data.resize(loSize);
                    int bytesRead = lo_read(conn, fd, reinterpret_cast<char *>(data.data()), loSize);
                    if (bytesRead != loSize)
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        throw DBException("Failed to read large object data");
                    }
                }
                else
                {
                    data.clear();
                }

                // Close the large object
                lo_close(conn, fd);

                // Commit the transaction
                res = PQexec(conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("Failed to commit transaction for BLOB loading: " + error);
                }
                PQclear(res);

                loaded = true;
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

            // Save the BLOB data to the database
            Oid save()
            {
                // Start a transaction if not already in one
                PGresult *res = PQexec(conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(res);
                    PQclear(res);
                    throw DBException("Failed to start transaction for BLOB saving: " + error);
                }
                PQclear(res);

                // Create a new large object if needed
                if (lobOid == 0)
                {
                    lobOid = lo_creat(conn, INV_WRITE);
                    if (lobOid == 0)
                    {
                        PQexec(conn, "ROLLBACK");
                        throw DBException("Failed to create large object");
                    }
                }

                // Open the large object
                int fd = lo_open(conn, lobOid, INV_WRITE);
                if (fd < 0)
                {
                    PQexec(conn, "ROLLBACK");
                    throw DBException("Failed to open large object for writing");
                }

                // Truncate the large object
                lo_truncate(conn, fd, 0);

                // Write the data
                if (!data.empty())
                {
                    int bytesWritten = lo_write(conn, fd, reinterpret_cast<const char *>(data.data()), data.size());
                    if (bytesWritten != static_cast<int>(data.size()))
                    {
                        lo_close(conn, fd);
                        PQexec(conn, "ROLLBACK");
                        throw DBException("Failed to write large object data");
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
                    throw DBException("Failed to commit transaction for BLOB saving: " + error);
                }
                PQclear(res);

                return lobOid;
            }

            // Get the OID of the large object
            Oid getOid() const
            {
                return lobOid;
            }

            void free() override
            {
                if (lobOid != 0)
                {
                    // Start a transaction if not already in one
                    PGresult *res = PQexec(conn, "BEGIN");
                    if (PQresultStatus(res) == PGRES_COMMAND_OK)
                    {
                        PQclear(res);

                        // Delete the large object
                        lo_unlink(conn, lobOid);

                        // Commit the transaction
                        res = PQexec(conn, "COMMIT");
                        PQclear(res);
                    }
                    else
                    {
                        PQclear(res);
                    }

                    lobOid = 0;
                }

                MemoryBlob::free();
                loaded = false;
            }
        };

    } // namespace PostgreSQL
} // namespace cpp_dbc

#endif // USE_POSTGRESQL

#endif // CPP_DBC_POSTGRESQL_BLOB_HPP