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

 @file prepared_statement_03.cpp
 @brief MySQL database driver implementation - MySQLDBPreparedStatement nothrow methods (part 2 - blob/binary and execute)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("ZPONUDZ1G6PM", "setBlob failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("I81HBMMW7EY7", "Invalid parameter index for setBlob", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the blob object to keep it alive
        m_blobObjects[idx] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        // Get the blob length via nothrow overload
        auto lengthResult = x->length(std::nothrow);
        if (!lengthResult.has_value())
        {
            return cpp_dbc::unexpected(lengthResult.error());
        }

        // Get the blob data via nothrow overload
        auto dataResult = x->getBytes(std::nothrow, 0, lengthResult.value());
        if (!dataResult.has_value())
        {
            return cpp_dbc::unexpected(dataResult.error());
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = std::move(dataResult.value());

        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = m_blobValues[idx].data();
        m_binds[idx].buffer_length = m_blobValues[idx].size();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;

        // Store parameter value for query reconstruction (with proper escaping)
        m_parameterValues[idx] = "BINARY DATA";
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("ATVKMR2CYNW3", "setBinaryStream failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("W32D7KYC25JV", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the stream object to keep it alive
        m_streamObjects[idx] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        // Read all data from the stream via nothrow overload
        std::vector<uint8_t> data;
        std::array<uint8_t, 4096> buffer{};
        for (;;)
        {
            auto readResult = x->read(std::nothrow, buffer.data(), buffer.size());
            if (!readResult.has_value())
            {
                return cpp_dbc::unexpected(readResult.error());
            }
            int bytesRead = readResult.value();
            if (bytesRead <= 0)
            {
                break;
            }
            data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = std::move(data);

        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = m_blobValues[idx].data();
        m_binds[idx].buffer_length = m_blobValues[idx].size();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;

        // Store parameter value for query reconstruction
        m_parameterValues[idx] = "BINARY DATA";
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("2TN4M0W7PMXX", "setBinaryStream failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("R6XHTB2M1HXD", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the stream object to keep it alive
        m_streamObjects[idx] = x;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        // Read up to 'length' bytes from the stream via nothrow overload
        std::vector<uint8_t> data;
        data.reserve(length);
        std::array<uint8_t, 4096> buffer{};
        size_t totalBytesRead = 0;
        while (totalBytesRead < length)
        {
            auto readResult = x->read(std::nothrow, buffer.data(), std::min(buffer.size(), length - totalBytesRead));
            if (!readResult.has_value())
            {
                return cpp_dbc::unexpected(readResult.error());
            }
            int bytesRead = readResult.value();
            if (bytesRead <= 0)
            {
                break;
            }
            data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);
            totalBytesRead += bytesRead;
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx] = std::move(data);

        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = m_blobValues[idx].data();
        m_binds[idx].buffer_length = m_blobValues[idx].size();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;

        // Store parameter value for query reconstruction
        m_parameterValues[idx] = "BINARY DATA";
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
    {
        // 2026-03-08T19:10:00Z
        // Bug: std::vector::data() may return nullptr for empty vectors
        // (implementation-defined). The raw-pointer overload treats nullptr as
        // SQL NULL, so setBytes(idx, emptyVector) stored NULL instead of a
        // zero-length blob.
        // Solution: Guard with x.empty() check — pass a non-null sentinel so the
        // raw-pointer overload sees a valid pointer with length 0.
        if (x.empty())
        {
            // Use a stack byte as non-null sentinel; length 0 means no data is copied
            uint8_t sentinel = 0;
            return setBytes(std::nothrow, parameterIndex, &sentinel, 0);
        }
        return setBytes(std::nothrow, parameterIndex, x.data(), x.size());
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("EI21DGR86T2Z", "setBytes failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("EZPZDJ1XBAUF", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        if (!x)
        {
            auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
            if (!nullResult.has_value())
            {
                return cpp_dbc::unexpected(nullResult.error());
            }
            return {};
        }

        // Store the data in our vector to keep it alive
        m_blobValues[idx].resize(length);
        if (length > 0)
        {
            std::memcpy(m_blobValues[idx].data(), x, length);
        }

        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = m_blobValues[idx].data();
        m_binds[idx].buffer_length = m_blobValues[idx].size();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;

        // Store parameter value for query reconstruction
        m_parameterValues[idx] = "BINARY DATA";
        return {};
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> MySQLDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        // 2026-03-08T18:30:00Z
        // Bug: executeQuery() was using mysql_query() with reconstructed SQL instead of
        // the mysql_stmt_* prepared statement API. This bypassed parameter binding, broke
        // binary/blob parameters, and was vulnerable to SQL injection.
        // Solution: Use mysql_stmt_bind_param() + mysql_stmt_execute() +
        // mysql_stmt_store_result() and materialize all rows into a ResultSet that
        // operates independently of the MYSQL_STMT handle.
        MYSQL_STMT_LOCK_OR_RETURN("LXJI5IP1RX5S", "executeQuery failed");

        if (!m_stmt)
        {
            return cpp_dbc::unexpected(DBException("8S667IDQSV9B", "Statement is not initialized", system_utils::captureCallStack()));
        }

        // Bind parameters
        if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
        {
            return cpp_dbc::unexpected(DBException("IBXPNGBMEO5Y",
                std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()),
                system_utils::captureCallStack()));
        }

        // Execute the prepared statement
        if (mysql_stmt_execute(m_stmt.get()) != 0)
        {
            return cpp_dbc::unexpected(DBException("B2LWZLJODUZC",
                std::string("Failed to execute query: ") + mysql_stmt_error(m_stmt.get()),
                system_utils::captureCallStack()));
        }

        // Get result metadata to discover column names and count
        MySQLResHandle metadata(mysql_stmt_result_metadata(m_stmt.get()));
        if (!metadata)
        {
            return cpp_dbc::unexpected(DBException("KVMVHI5T9YHO",
                std::string("Failed to get result metadata: ") + mysql_stmt_error(m_stmt.get()),
                system_utils::captureCallStack()));
        }

        unsigned int fieldCount = mysql_num_fields(metadata.get());

        // Extract column names from metadata
        std::vector<std::string> columnNames;
        columnNames.reserve(fieldCount);
        {
            const MYSQL_FIELD *fields = mysql_fetch_fields(metadata.get());
            for (unsigned int i = 0; i < fieldCount; ++i)
            {
                columnNames.emplace_back(fields[i].name);
            }
        }

        // Enable STMT_ATTR_UPDATE_MAX_LENGTH for accurate buffer sizing
        bool updateMaxLen = true;
        mysql_stmt_attr_set(m_stmt.get(), STMT_ATTR_UPDATE_MAX_LENGTH, &updateMaxLen);

        // Store all rows on the client side
        if (mysql_stmt_store_result(m_stmt.get()) != 0)
        {
            return cpp_dbc::unexpected(DBException("R7REPR1DR4SC",
                std::string("Failed to store result: ") + mysql_stmt_error(m_stmt.get()),
                system_utils::captureCallStack()));
        }

        // Re-fetch metadata fields to get max_length after store_result
        const MYSQL_FIELD *fields = mysql_fetch_fields(metadata.get());

        // Set up result binding buffers
        std::vector<MYSQL_BIND> resultBinds(fieldCount);
        std::vector<std::vector<char>> buffers(fieldCount);
        std::vector<unsigned long> lengths(fieldCount, 0);
        auto nullFlags = std::make_unique<bool[]>(fieldCount);
        std::fill_n(nullFlags.get(), fieldCount, false);
        std::memset(resultBinds.data(), 0, sizeof(MYSQL_BIND) * fieldCount);

        for (unsigned int i = 0; i < fieldCount; ++i)
        {
            // Use max_length + 1 for the buffer size (max_length is updated after store_result
            // when STMT_ATTR_UPDATE_MAX_LENGTH is set). Minimum 256 bytes as fallback.
            size_t bufSize = (fields[i].max_length > 0 ? fields[i].max_length : 256) + 1;
            buffers[i].resize(bufSize, '\0');

            resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
            resultBinds[i].buffer = buffers[i].data();
            resultBinds[i].buffer_length = buffers[i].size();
            resultBinds[i].length = &lengths[i];
            resultBinds[i].is_null = &nullFlags[i];
        }

        if (mysql_stmt_bind_result(m_stmt.get(), resultBinds.data()) != 0)
        {
            mysql_stmt_free_result(m_stmt.get());
            return cpp_dbc::unexpected(DBException("MOQYKOLM4PBP",
                std::string("Failed to bind result: ") + mysql_stmt_error(m_stmt.get()),
                system_utils::captureCallStack()));
        }

        // Materialize all rows
        std::vector<std::vector<std::optional<std::string>>> rows;
        while (true)
        {
            int fetchRc = mysql_stmt_fetch(m_stmt.get());
            if (fetchRc == MYSQL_NO_DATA)
            {
                break;
            }
            if (fetchRc == 1)
            {
                mysql_stmt_free_result(m_stmt.get());
                return cpp_dbc::unexpected(DBException("XJXDLLYIVFDW",
                    std::string("Failed to fetch row: ") + mysql_stmt_error(m_stmt.get()),
                    system_utils::captureCallStack()));
            }

            std::vector<std::optional<std::string>> row(fieldCount);
            for (unsigned int i = 0; i < fieldCount; ++i)
            {
                if (nullFlags[i] != 0)
                {
                    row[i] = std::nullopt;
                }
                else
                {
                    row[i] = std::string(buffers[i].data(), lengths[i]);
                }
            }
            rows.push_back(std::move(row));
        }

        // Free the stmt result — rows are now materialized
        mysql_stmt_free_result(m_stmt.get());

        // Create a materialized ResultSet
        auto conn = m_connection.lock();
        auto rsResult = MySQLDBResultSet::create(
            std::nothrow, std::move(columnNames), std::move(rows), conn);
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }

        auto rs = rsResult.value();
        if (conn)
        {
            [[maybe_unused]] auto regResult = conn->registerResultSet(
                std::nothrow, std::weak_ptr<MySQLDBResultSet>(rs));
        }

        return std::shared_ptr<RelationalDBResultSet>(rs);
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("04CY5QLA3WRQ", "executeUpdate failed");

        if (!m_stmt)
        {
            return cpp_dbc::unexpected(DBException("5LX1HOYTGK2I", "Statement is not initialized", system_utils::captureCallStack()));
        }

        // Bind parameters
        if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
        {
            return cpp_dbc::unexpected(DBException("BEQW22FPSELD", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
        }

        // Execute the query
        if (mysql_stmt_execute(m_stmt.get()) != 0)
        {
            return cpp_dbc::unexpected(DBException("GFRDKCPL25D3", std::string("Failed to execute update: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
        }

        return mysql_stmt_affected_rows(m_stmt.get());
    }

    cpp_dbc::expected<bool, DBException> MySQLDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("A9VR5AXG07G3", "execute failed");

        if (!m_stmt)
        {
            return cpp_dbc::unexpected(DBException("7QNIVNSAGDYI", "Statement is not initialized", system_utils::captureCallStack()));
        }

        // Bind parameters
        if (!m_binds.empty() && mysql_stmt_bind_param(m_stmt.get(), m_binds.data()) != 0)
        {
            return cpp_dbc::unexpected(DBException("MTD6DPBV993Q", std::string("Failed to bind parameters: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
        }

        // Execute the query
        if (mysql_stmt_execute(m_stmt.get()) != 0)
        {
            return cpp_dbc::unexpected(DBException("FIMP48JEJJHM", std::string("Failed to execute statement: ") + mysql_stmt_error(m_stmt.get()), system_utils::captureCallStack()));
        }

        // Return whether there's a result set
        return mysql_stmt_field_count(m_stmt.get()) > 0;
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        if (m_stmt)
        {
            m_stmt.reset(); // Smart pointer will call mysql_stmt_close via deleter
        }

        m_closed.store(true, std::memory_order_release);
        return {};
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
